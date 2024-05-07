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

constexpr std::string_view DISALLOW = "disallow";
constexpr std::string_view ALLOW = "allow";
constexpr std::string_view SITEMAP = "sitemap";
constexpr std::string_view USERAGENT = "user-agent";
constexpr std::string_view CRAWLDELAY = "crawl-delay";
constexpr char DELIMITER = ':';
constexpr std::string_view DESIRED_USERAGENT = "*";

namespace robots {
    class Parser {
    public:
        Parser(const std::string& robotsTxtContent, const std::string& domain) : domain(domain) {
            uaDirectiveMap["*"][std::string(DISALLOW)] = {};
            uaDirectiveMap["*"][std::string(ALLOW)] = {};
            tokenizeInput(robotsTxtContent);
        }

        directiveStore getDirectiveMap() const {
            return uaDirectiveMap;
        }

        bool checkUrl(const std::string& path) const {
            bool explicitlyDisallow = false;
            for (const std::regex& matcher : uaDirectiveMap.at(std::string(DESIRED_USERAGENT)).at(std::string(DISALLOW))) {
                if (std::regex_search(path, matcher)) {
                    explicitlyDisallow = true;
                    break;
                }
            }

            if (!explicitlyDisallow) {
                return true;
            }

            for (const std::regex matcher : uaDirectiveMap.at(std::string(DESIRED_USERAGENT)).at(std::string(ALLOW))) {
                if (std::regex_search(path, matcher)) {
                    return true;
                }
            }

            return false;
        }

        int getDelay() {
            return crawlDelay;
        }

    private:

        int crawlDelay = 0;

        bool isValidAscii(const std::string& str) const{
            return std::all_of(str.begin(), str.end(), [](char c) { return c <= 127; });
        }


        void tokenizeInput(std::string robotsTxtContent) {
            std::istringstream iss(std::move(robotsTxtContent));
            std::string currentAgent = "*";

            for (std::string line; std::getline(iss, line);) {
                 // Remove whitespace

                if (line.empty() || line[0] == '#' || !isValidAscii(line)) {
                    continue; // Skip empty, comment, or non-ASCII lines
                }

                line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

                auto [key, val] = splitString(line, DELIMITER);

                if (val.empty())
                    continue;

                std::transform(key.begin(), key.end(), key.begin(), ::tolower); // Hopefully all utf8 are gone or i will break later on

                if (key == USERAGENT) {
                    currentAgent = val;
                }
                else if (key == DISALLOW || key == ALLOW) {
                    uaDirectiveMap[currentAgent][key].push_back(std::regex(generateRegex(val))); // Store regex for each path
                }
                else if (key == CRAWLDELAY) {
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


