#ifndef SRC_UTILS_H_
#define SRC_UTILS_H_

#include <string>
#include <ctime>

std::string trim(std::string str);
std::string toLowerCaseStr(std::string str);
std::string toISO8601Time(std::time_t& time);
std::string uriDecode(std::string& str);
std::string uriEncode(std::string& str, bool path=false);

#endif /* SRC_UTILS_H_ */
