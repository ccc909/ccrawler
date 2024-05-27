#pragma once 

#include "ThreadPool.cpp"
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <regex>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <fstream>
#include "Link.cpp"
#include "Domain.cpp"
#include <ixwebsocket/IXWebSocket.h>
#include <iostream>


using json = nlohmann::json;

class Crawler {
public:
	//init crawler with websocket and thread pool
	Crawler(ix::WebSocket* webSocket) : m_pool(std::thread::hardware_concurrency()), m_webSocket(webSocket) {
	}

	std::unordered_map<std::string, Domain*> m_visitedDomains;


	//crawl function, should be called by the websocket
	void crawl(const Link& startLink) {
		Domain* domain;

		//try to find a previously created domain, also should help to avoid double crawl??
		auto it = m_visitedDomains.find(startLink.getDomain());
		if (it != m_visitedDomains.end()) {
			domain = it->second;
		}
		else {
			domain = new Domain(startLink);
			m_visitedDomains[startLink.getDomain()] = domain;
		}


		//might miss some links, but good enough
		while (domain->hasLinks() && !m_shouldStop) {
			//if (m_shouldStop) {
			//    break; // Exit the loop
			//}

			//get next link
			auto [link, depth] = domain->getNextLink();

			std::string domainName = link.getDomain();
			std::lock_guard<std::mutex> lock(m_mutexes[domainName]); // Lock access to the queue for this domain

			std::string fullLink = link.getFullLink();

			if (m_visitedUrls.count(fullLink) != 0 || depth >= 10) {
				//skip if URL has already been visited or depth limit reached
				continue;
			}

			//insert visited link
			m_visitedUrls.insert(fullLink);

			cpr::Response response = cpr::Get(cpr::Url{ fullLink });

			if (response.status_code == 200) {
				std::cout << "Successfully crawled: " << fullLink << std::endl;

				// extract URLs using regex
				std::set<std::string> links = extractUrls(response.text, fullLink);

				// add child URLs to the BFS queue
				for (const auto& childLink : links) {
					Link child(childLink);
					std::cout << childLink << std::endl;
					if (domain->canCrawl(child)) {
						//should immediatelly return if same domain is visited??? maybe add another check , it doesnt stop

						//!!!!!!!!!!!!!!!!!!!!!!!!need one more condition to check if domain already has a thread running!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
						if (child.getDomain() != domainName) {
							json jsonData;
							jsonData["message_type"] = "new_domain";
							jsonData["parent_domain"] = domainName;
							jsonData["child_domain"] = child.getDomain();
							m_webSocket->sendText(jsonData.dump());
							// submit crawling task to the thread pool for URLs from a new domain
							m_pool.enqueue([this, child] { crawl(child); });
						}
						else {
							json jsonData;
							jsonData["message_type"] = "new_link";
							jsonData["childLink"] = childLink;

							// Convert JSON object to string
							std::string jsonStr = jsonData.dump();

							domain->addLink(child, depth + 1);
							//cheap fix for now but works, cralwer still might be doing double work in background but frontend receives correct data
							if (testset.count(childLink) == 0)
								m_webSocket->sendText(jsonStr);
							testset.insert(childLink);
						}
					}
				}
			}
			else {
				std::cerr << "Failed to crawl: " << fullLink << std::endl;
				std::cerr << "Status code: " << response.status_code << std::endl;
			}

			//delay crwaling per domain
			std::this_thread::sleep_for(std::chrono::milliseconds(domain->getCrawlDelay()));
		}
	}

	void queueFromWebsocket(const std::string& url) {
		Link link(url);
		m_pool.enqueue([this, link] { crawl(link); });
	}

	//unused
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

	//unused
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


	void stop() {
		{
			std::unique_lock<std::mutex> lock(m_pool.queueMutex);
			m_shouldStop = true; // Set the flag to indicate that the crawler should stop
			m_pool.stop = true; // Set the pool's stop flag
		}


		m_visitedDomains.clear();
		m_visitedUrls.clear();
		testset.clear();
	}

	void setParams(bool _ignoreRobots, bool _domainsOnly) {
		ignoreRobots = _ignoreRobots;
		domainsOnly = _domainsOnly;
	}

private:
	bool ignoreRobots = false;
	bool domainsOnly = false;

	//always true for now, wanted to use to limit crawler to only domains, but not needed anymore
	bool should_crawl(const std::string& link) {
		return true;
	}

	//extract urls from html 
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
						link = baseUrl.substr(0, baseUrl.length() - 1) + link;
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

	//unused
	std::string serializeState() {
		std::ostringstream oss;

		// Serialize visited URLs
		oss << m_visitedUrls.size() << "\n";
		for (const auto& url : m_visitedUrls) {
			oss << url << "\n";
		}

		return oss.str();
	}

	//unused
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
	std::unordered_set<std::string> testset;
	ThreadPool m_pool;
	ix::WebSocket* m_webSocket;
	bool m_shouldStop = false;
};