#include <iostream>
#include <set>
#include <queue>
#include <mutex>
#include <map>
#include <cpr/cpr.h>
#include <regex>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <fstream>
#include "robotstxt.cpp"
#include <ixwebsocket/IXWebSocketServer.h>



class Link {
public:
    Link(const std::string& fullLink) : fullLink(fullLink) {
        extractComponents(fullLink);
    }

    std::string getDomain() const {
        return domain;
    }

    std::string getFullLink() const {
        return fullLink;
    }

    std::string getRelativePath() const {
        return relativePath;
    }

private:
    std::string domain;
    std::string fullLink;
    std::string relativePath;

    void extractComponents(const std::string& url) {
        size_t start = url.find("://");
        if (start == std::string::npos)
            return; // Invalid URL
        start += 3;
        size_t end = url.find_first_of("/?", start);
        domain = url.substr(start, end - start);

        // Remove subdomains from the domain
        size_t pos = domain.rfind('.');
        if (pos != std::string::npos) {
            size_t pos2 = domain.rfind('.', pos - 1);
            if (pos2 != std::string::npos) {
                domain = domain.substr(pos2 + 1);
            }
        }

        if (end != std::string::npos) {
            relativePath = url.substr(end);
        }
    }
};
//probably shouldnt be named domain
class Domain {
public:
    Domain(const Link& startLink, int crawlDelay = 0) : m_link(startLink), m_crawlDelay(crawlDelay) {
        m_bfsQueue.push({ startLink, 0 });
        cpr::Response response = cpr::Get(cpr::Url(startLink.getDomain() + "/robots.txt"));
        std::cout << startLink.getDomain() + "/robots.txt" << std::endl;
        if (response.status_code == 200) {
            std::cout << "Successfully fetched robots.txt for domain: " << startLink.getDomain() << std::endl;
            m_parser = std::make_unique<robots::Parser>(response.text, startLink.getDomain());
        }
        else {
            std::cerr << "Failed to fetch robots.txt for domain: " << startLink.getDomain() << std::endl;
        }
    }



    bool hasLinks() const {
        return !m_bfsQueue.empty();
    }

    std::pair<Link, int> getNextLink() {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto link = m_bfsQueue.front();
        m_bfsQueue.pop();
        return link;
    }

    void addLink(const Link& link, int depth) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_bfsQueue.push({ link, depth });
    }

    int getCrawlDelay() {
        if (m_parser) {
            int delay = m_parser->getDelay();
            std::cout << delay;
            return delay;
        }
        return m_crawlDelay;
    }

    bool canCrawl(const Link& link) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_parser) {
            bool result = m_parser->checkUrl(link.getRelativePath());
            std::cout << "Result of checkUrl: " << (result ? "true" : "false") << std::endl;
            return result;
        }
        else {
            std::cout << "No parser found, allowing crawl by default" << std::endl;
        }
        return true;
    }

    std::string serialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ostringstream oss;

        // Serialize link
        oss << m_link.getFullLink() << "\n";

        // Serialize BFS queue
        oss << m_bfsQueue.size() << "\n";
        while (!m_bfsQueue.empty()) {
            oss << m_bfsQueue.front().first.getFullLink() << " " << m_bfsQueue.front().second << "\n";
            m_bfsQueue.pop();
        }

        // Serialize crawl delay
        oss << m_crawlDelay << "\n";

        // Serialize whether parser exists
        oss << (m_parser ? "true" : "false") << "\n";

        return oss.str();
    }

private: // Callback function to send back links to WebSocket client
    Link m_link;
    std::queue<std::pair<Link, int>> m_bfsQueue;
    std::mutex m_mutex;
    int m_crawlDelay;
    std::unique_ptr<robots::Parser> m_parser;
};


class ThreadPool { //not my implementation
public:
    ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i)
            workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    condition.wait(lock, [this] { return stop || !tasks.empty(); });
                    if (stop && tasks.empty())
                        return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }

                task();
            }
        });
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers)
            worker.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

class Crawler {
public:
    Crawler(ix::WebSocket* webSocket) : m_pool(std::thread::hardware_concurrency()), m_webSocket(webSocket) {
    }
    // Pass callback function to the constructor // Create a thread pool with the number of hardware threads

    void crawl(const Link& startLink) {

        Domain domain(startLink);

        while (domain.hasLinks()) {
            auto [link, depth] = domain.getNextLink();

            std::string domainName = link.getDomain();
            std::lock_guard<std::mutex> lock(m_mutexes[domainName]); // Lock access to the queue for this domain

            std::string fullLink = link.getFullLink();

            if (m_visitedUrls.count(fullLink) || depth >= 10) {
                // Skip if URL has already been visited or depth limit reached
                continue;
            }

            m_visitedUrls.insert(fullLink);

            cpr::Response response = cpr::Get(cpr::Url{ fullLink });

            if (response.status_code == 200) {
                std::cout << "Successfully crawled: " << fullLink << std::endl;

                // Extract URLs using regex
                std::set<std::string> links = extractUrls(response.text, fullLink);

                // Add child URLs to the BFS queue
                for (const auto& childLink : links) {
                    Link child(childLink);
                    std::cout << childLink << std::endl;
                    if (domain.canCrawl(child)) {
                        if (child.getDomain() != domainName) {
                            // Submit crawling task to the thread pool for URLs from a new domain
                            m_pool.enqueue([this, child] { crawl(child); });
                        }
                        else {
                            domain.addLink(child, depth + 1);
                            m_webSocket->sendText("Crawled: " + childLink);
                        }
                    }
                }
            }
            else {
                std::cerr << "Failed to crawl: " << fullLink << std::endl;
                std::cerr << "Status code: " << response.status_code << std::endl;
            }

            // Delay before crawling next URL for this domain
            std::this_thread::sleep_for(std::chrono::milliseconds(domain.getCrawlDelay()));
        }
    }

    void saveState(const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << serializeState();
            file.close();
            std::cout << "Crawling session state saved to file: " << filename << std::endl;
        }
        else {
            std::cerr << "Failed to save crawling session state to file: " << filename << std::endl;
        }
    }

    void resumeState(const std::string& filename) {
        std::ifstream file(filename);
        if (file.is_open()) {
            std::string state;
            std::getline(file, state);
            deserializeState(state);
            file.close();
            std::cout << "Crawling session state resumed from file: " << filename << std::endl;
        }
        else {
            std::cerr << "Failed to resume crawling session state from file: " << filename << std::endl;
        }
    }

private:
    bool should_crawl(const std::string& link) {
        return true; // You can implement custom logic for deciding whether to crawl a link
    }

    std::set<std::string> extractUrls(const std::string& html, const std::string& baseUrl) {
        std::set<std::string> links;
        std::regex linkRegex("<a\\s+(?:[^>]*?\\s+)?href=([\"'])(.*?)\\1", std::regex::icase);
        auto words_begin = std::sregex_iterator(html.begin(), html.end(), linkRegex);
        auto words_end = std::sregex_iterator();
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            std::string link = match.str(2);
            if (!link.empty() && link[0] != '#') {
                if (link.find("http") != 0) {
                    if (!baseUrl.empty() && baseUrl.back() == '/')
                        link = baseUrl.substr(0, baseUrl.length() - 1) + link; // Remove trailing slash from baseUrl
                    else
                        link = baseUrl + link;
                }
                // Normalize the URL to remove any trailing slash
                if (!link.empty() && link.back() == '/')
                    link.pop_back();
                links.insert(link);
            }
        }
        return links;
    }

    std::string serializeState() {
        std::ostringstream oss;

        // Serialize visited URLs
        oss << m_visitedUrls.size() << "\n";
        for (const auto& url : m_visitedUrls) {
            oss << url << "\n";
        }

        return oss.str();
    }

    void deserializeState(const std::string& state) {
        std::istringstream iss(state);

        // Deserialize visited URLs
        size_t numVisitedUrls;
        iss >> numVisitedUrls;
        m_visitedUrls.clear();
        for (size_t i = 0; i < numVisitedUrls; ++i) {
            std::string url;
            iss >> url;
            m_visitedUrls.insert(url);
        }
    }

    std::mutex m_callbackMutex;
    std::map<std::string, std::mutex> m_mutexes; // Mutexes for each domain to synchronize access to queues
    std::unordered_set<std::string> m_visitedUrls;
    ThreadPool m_pool;
    ix::WebSocket *m_webSocket;
};

//int main(int argc, char** argv) {
//    Crawler crawler;
//    std::string url = "example.com"; // Change this to the URL you want to start crawling
//
//    // Check if there is a command line argument for resuming state
//    if (argc > 1) {
//        std::string arg = argv[1];
//        if (arg == "-resume" && argc > 2) {
//            std::string filename = argv[2];
//            crawler.resumeState(filename);
//        }
//    }
//    else {
//        crawler.crawl(Link(url));
//    }
//    
//
//
//    // Save state before exiting
//    crawler.saveState("crawler_state.txt");
//
//    std::this_thread::sleep_for(std::chrono::minutes(10)); // Wait for 10 minutes
//    return 0;
//}
