#pragma once

#include "ThreadPool.cpp"
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <regex>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <iostream>
#include <memory>
#include "Link.cpp"
#include "Domain.cpp"
#include <ixwebsocket/IXWebSocket.h>

using json = nlohmann::json;

class Crawler {
public:
    Crawler(ix::WebSocket* webSocket) : m_pool(std::thread::hardware_concurrency()), m_webSocket(webSocket) {}

    // Crawl function to be called by the websocket
    void crawl(const Link& startLink) {
        auto domain = std::make_unique<Domain>(startLink);

        std::unordered_map<std::string, std::unique_ptr<Domain>> visitedDomains;
        std::unordered_set<std::string> visitedUrls;
        std::unordered_set<std::string> testset;

        visitedDomains[startLink.getDomain()] = std::move(domain);

        while (visitedDomains[startLink.getDomain()]->hasLinks() && !m_shouldStop) {
            auto [link, depth] = visitedDomains[startLink.getDomain()]->getNextLink();

            std::string domainName = link.getDomain();
            std::lock_guard<std::mutex> lock(m_mutexes[domainName]);

            std::string fullLink = link.getFullLink();

            if (visitedUrls.count(fullLink) != 0 || depth >= 10) {
                continue;
            }

            visitedUrls.insert(fullLink);

            cpr::Response response = cpr::Get(cpr::Url{ fullLink });

            if (response.status_code == 200) {
                std::cout << "Successfully crawled: " << fullLink << std::endl;

                std::set<std::string> links = extractUrls(response.text, fullLink);

                for (const auto& childLink : links) {
                    Link child(childLink);
                    std::cout << childLink << std::endl;

                    if (visitedDomains[startLink.getDomain()]->canCrawl(child)) {
                        if (child.getDomain() != domainName) {
                            json jsonData;
                            jsonData["message_type"] = "new_domain";
                            jsonData["parent_domain"] = domainName;
                            jsonData["child_domain"] = child.getDomain();
                            m_webSocket->sendText(jsonData.dump());

                            m_pool.enqueue([this, child] { crawl(child); });
                        }
                        else {
                            json jsonData;
                            jsonData["message_type"] = "new_link";
                            jsonData["childLink"] = childLink;
                            std::string jsonStr = jsonData.dump();

                            visitedDomains[startLink.getDomain()]->addLink(child, depth + 1);

                            if (testset.count(childLink) == 0) {
                                m_webSocket->sendText(jsonStr);
                            }
                            testset.insert(childLink);
                        }
                    }
                }
            }
            else {
                std::cerr << "Failed to crawl: " << fullLink << std::endl;
                std::cerr << "Status code: " << response.status_code << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(visitedDomains[startLink.getDomain()]->getCrawlDelay()));
        }
    }

    void queueFromWebsocket(const std::string& url) {
        Link link(url);
        m_pool.enqueue([this, link] { crawl(link); });
    }

    void stop() {
        {
            std::unique_lock<std::mutex> lock(m_pool.queueMutex);
            m_shouldStop = true;
            m_pool.stop = true;
        }
    }

    void setParams(bool _ignoreRobots, bool _domainsOnly) {
        ignoreRobots = _ignoreRobots;
        domainsOnly = _domainsOnly;
    }

private:
    bool ignoreRobots = false;
    bool domainsOnly = false;

    bool should_crawl(const std::string& link) {
        return true;
    }

    bool isValidUrl(const std::string& url) {
        // Check if the URL has a valid scheme (http or https)
        std::regex urlRegex(R"((http|https)://([^\s/$.?#].[^\s]*)$)", std::regex::icase);
        return std::regex_match(url, urlRegex);
    }

    std::string resolveRelativeUrl(const std::string& link, const std::string& baseUrl) {
        if (link.find("http") == 0) {
            return link;
        }

        // Handle the relative URL
        std::string resolvedUrl = baseUrl;
        if (!baseUrl.empty() && baseUrl.back() == '/' && link.front() == '/') {
            resolvedUrl.pop_back();
        }
        return resolvedUrl + link;
    }

    std::set<std::string> extractUrls(const std::string& html, const std::string& baseUrl) {
        std::set<std::string> links;
        std::regex linkRegex(R"(<a\s+(?:[^>]*?\s+)?href=([\"'])(.*?)\1)", std::regex::icase);
        auto words_begin = std::sregex_iterator(html.begin(), html.end(), linkRegex);
        auto words_end = std::sregex_iterator();

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            std::string link = match.str(2);

            if (!link.empty() && link[0] != '#' && link.find("javascript:") != 0 && link.find("mailto:") != 0) {
                std::string resolvedLink = resolveRelativeUrl(link, baseUrl);
                if (isValidUrl(resolvedLink)) {
                    if (resolvedLink.back() == '/') {
                        resolvedLink.pop_back();
                    }
                    links.insert(resolvedLink);
                }
            }
        }

        return links;
    }

    std::map<std::string, std::mutex> m_mutexes;
    ThreadPool m_pool;
    ix::WebSocket* m_webSocket;
    bool m_shouldStop = false;
};
