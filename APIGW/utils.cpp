#include "utils.h"
#include <cctype>


// Remove leading and tailing spaces
std::string trim(std::string str) {
	if(str.empty()) {
		return "";
	}
	std::size_t i = 0;
	while(str[i] == ' ') {
		i++;
		if(i >= str.length()) {
			break;
		}
	}

	std::size_t j = str.length() - 1;
	while(str[j] == ' ') {
		j--;
		if(j < 0) {
			break;
		}
	}
	return str.substr(i, j - i + 1);
}

std::string toLowerCaseStr(std::string str) {
	for(unsigned int i = 0; i < str.length(); i++){
		str[i] = tolower(str[i]);
	}
	return str;
}


std::string toISO8601Time(std::time_t& time) {
	char buffer[32];
	strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", gmtime(&time));
	std::string str(buffer);
	return str;
}

std::string uriDecode(std::string& str) {
    char ch, c, decoded;
	std::string encodedStr = "";
    enum {
        sw_usual = 0,
        sw_quoted,
        sw_quoted_second
    } state;

    state = sw_usual;
    decoded = 0;
	for (unsigned int i = 0; i < str.length(); i++) {

        ch = str[i];

        switch (state) {
        case sw_usual:

            if (ch == '%') {
                state = sw_quoted;
                break;
            }

            if (ch == '+') {
				encodedStr += ' ';
                break;
            }

			encodedStr += ch;
            break;

        case sw_quoted:

            if (ch >= '0' && ch <= '9') {
                decoded = (unsigned char)(ch - '0');
                state = sw_quoted_second;
                break;
            }

            c = (unsigned char)(ch | 0x20);
            if (c >= 'a' && c <= 'f') {
                decoded = (unsigned char)(c - 'a' + 10);
                state = sw_quoted_second;
                break;
            }

            /* the invalid quoted character */

            state = sw_usual;

			encodedStr += ch;

            break;

        case sw_quoted_second:

            state = sw_usual;

            if (ch >= '0' && ch <= '9') {
                ch = (unsigned char)((decoded << 4) + ch - '0');


				encodedStr += ch;

                break;
            }

            c = (unsigned char)(ch | 0x20);
            if (c >= 'a' && c <= 'f') {
                ch = (unsigned char)((decoded << 4) + c - 'a' + 10);

				encodedStr += ch;

                break;
            }

            /* the invalid quoted character */

            break;
        }
    }
    return encodedStr;

}


std::string uriEncode(std::string& str, bool path) {
    static unsigned char hex[] = "0123456789ABCDEF";
    std::string encodedStr = "";
    if (str.empty()) {
        return encodedStr;
    }
    for (unsigned int i = 0; i < str.length(); i++) {
        unsigned char c = str[i];
        if (isalnum(c) || c == '.' || c == '~' || c == '_' || c == '-' || (path && c == '/')) {
            encodedStr += str[i];
        }
        else {
            encodedStr += '%';
            encodedStr += hex[c >> 4];
            encodedStr += hex[c & 0xf];
        }
    }
    return encodedStr;
}
