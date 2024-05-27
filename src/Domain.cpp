#pragma once

#include <queue>
#include <iostream>
#include <mutex>
#include "Robots.cpp"
#include "Link.cpp"
#include <cpr/cpr.h>

class Domain {
public:
	Domain(const Link& startLink, int crawlDelay = 0) : m_link(startLink), m_crawlDelay(crawlDelay) {
		m_bfsQueue.push({ startLink, 0 });
		//try to fetch robots.txt
		cpr::Response response = cpr::Get(cpr::Url(startLink.getDomain() + "/robots.txt"));
		std::cout << startLink.getDomain() + "/robots.txt" << std::endl;
		if (response.status_code == 200) {
			std::cout << "Successfully fetched robots.txt for domain: " << startLink.getDomain() << std::endl;
			//store a robots parser object
			m_parser = std::make_unique<robots::Parser>(response.text, startLink.getDomain());
		}
		else {
			//should be handled by the crawler
			std::cerr << "Failed to fetch robots.txt for domain: " << startLink.getDomain() << std::endl;
		}
	}

	//

	bool hasLinks() const {
		return !m_bfsQueue.empty();
	}

	//get next link in queue
	std::pair<Link, int> getNextLink() {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto link = m_bfsQueue.front();
		m_bfsQueue.pop();
		return link;
	}

	// add link to queue
	void addLink(const Link& link, int depth) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_bfsQueue.push({ link, depth });
	}

	//get crawl delay for parser, should change this to a field
	int getCrawlDelay() {
		if (m_parser) {
			int delay = m_parser->getDelay();
			std::cout << delay;
			return delay;
		}
		return m_crawlDelay;
	}

	// check if a link can be crawler according to robots.txt
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


	//unused
	std::string serialize() {
		std::lock_guard<std::mutex> lock(m_mutex);
		std::ostringstream oss;

		oss << m_link.getFullLink() << "\n";

		oss << m_bfsQueue.size() << "\n";
		while (!m_bfsQueue.empty()) {
			oss << m_bfsQueue.front().first.getFullLink() << " " << m_bfsQueue.front().second << "\n";
			m_bfsQueue.pop();
		}
		oss << m_crawlDelay << "\n";

		oss << (m_parser ? "true" : "false") << "\n";

		return oss.str();
	}

	Link getLink() {
		return m_link;
	}

private:
	Link m_link;
	std::queue<std::pair<Link, int>> m_bfsQueue;
	std::mutex m_mutex;
	int m_crawlDelay;
	std::unique_ptr<robots::Parser> m_parser;
};