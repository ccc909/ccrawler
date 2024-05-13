#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <regex>
#include <algorithm>
#include <cctype>

typedef std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::regex>>> directiveStore;
typedef std::pair<std::string, std::string> stringPair;

//globals
constexpr const char* DISALLOW = "disallow";
constexpr const char* ALLOW = "allow";
constexpr const char* SITEMAP = "sitemap";
constexpr const char* USERAGENT = "user-agent";
constexpr const char* CRAWLDELAY = "crawl-delay";
constexpr char DELIMITER = ':';
constexpr const char* DESIRED_USERAGENT = "*";

namespace robots {
	class Parser {
	public:
		Parser(const std::string& robotsTxtContent, const std::string& domain) : domain(domain) {
			//initialize map
			uaDirectiveMap["*"][DISALLOW] = {};
			uaDirectiveMap["*"][ALLOW] = {};
			tokenizeInput(robotsTxtContent);
		}

		directiveStore getDirectiveMap() const {
			return uaDirectiveMap;
		}

		bool checkUrl(const std::string& path) const {
			bool explicitlyDisallow = false;
			//first check disallow rules, since they can be overwritten by allow rules
			for (const std::regex& matcher : uaDirectiveMap.at(DESIRED_USERAGENT).at(DISALLOW)) {
				if (std::regex_search(path, matcher)) {
					explicitlyDisallow = true;
					break;
				}
			}

			//if no disallow rules are matched, check allow rules
			if (!explicitlyDisallow) {
				return true;
			}

			//check allow rules
			for (const std::regex& matcher : uaDirectiveMap.at(DESIRED_USERAGENT).at(ALLOW)) {
				if (std::regex_search(path, matcher)) {
					return true;
				}
			}

		//else disallowed
			return false;
		}

		int getDelay() const {
			return crawlDelay;
		}

	private:

		int crawlDelay = 0;

		//ensure only ascii lines are parsed
		bool isValidAscii(const std::string& str) const {
			return std::all_of(str.begin(), str.end(), [](char c) { return c <= 127 && c >= 0; });
		}

		void tokenizeInput(std::string robotsTxtContent) {
			std::istringstream iss(std::move(robotsTxtContent));
			std::string currentAgent = "*";

			for (std::string line; std::getline(iss, line);) {
				// remove whitespace

				if (line.empty() || line[0] == '#' || !isValidAscii(line)) {
					continue; // sip empty, comment, or non-ASCII lines
				}

				line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

				auto [key, val] = splitString(line, DELIMITER);

				if (val.empty())
					continue;

				std::transform(key.begin(), key.end(), key.begin(), ::tolower); // Hopefully all utf8 are gone or it will break later on

				if (key == USERAGENT) {
					//set user agent
					currentAgent = val;
				}
				else if (key == DISALLOW || key == ALLOW) {
					//store disallow/allow rules
					uaDirectiveMap[currentAgent][key].push_back(std::regex(generateRegex(val))); // Store regex for each path
				}
				else if (key == CRAWLDELAY) {
					//store crawl delay
					crawlDelay = std::stoi(val);
				}
				else {
					std::cerr << "Malformed key: " << key << std::endl;
				}
			}
		}

		stringPair splitString(const std::string& str, char delimiter) const {
			size_t pos = str.find(delimiter);
			if (pos != std::string::npos) {
				return { str.substr(0, pos), str.substr(pos + 1) };
			}
			else {
				return { str, "" };
			}
		}

		//this seems to work, but I'm not sure if it's the best way to do it
		std::string generateRegex(const std::string& value) const {
			std::ostringstream regexPattern;
			bool endWildcard = false;

			for (char c : value) {
				if (c == '*') {
					endWildcard = true;
					regexPattern << ".*";
				}
				else if (c == '$') {
					break;
				}
				else {
					regexPattern << c;
				}
			}

			if (!endWildcard) {
				regexPattern << ".*";
			}

			std::cout << regexPattern.str() << std::endl;
			return regexPattern.str();
		}

		directiveStore uaDirectiveMap;
		std::string domain;
	};
}
