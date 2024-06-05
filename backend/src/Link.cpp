#pragma once

class Link {
public:
	Link(const std::string& fullLink) : fullLink(fullLink) {
		extractComponents(fullLink);
	}
	//getters
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

	//setter, works only for lniks that have a protocol for now
	void extractComponents(const std::string& url) {
		size_t start = url.find("://");
		if (start == std::string::npos)
			return; // Invalid URL
		start += 3;
		size_t end = url.find_first_of("/?", start);
		domain = url.substr(start, end - start);

		//remove subdomain
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