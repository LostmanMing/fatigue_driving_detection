#ifndef SRC_HEADER_H_
#define SRC_HEADER_H_

#include <string>
#include <vector>
#include "utils.h"

class Header {
private:
	std::string mKey;
	std::string mValues;

public:
	Header(const std::string key) {
		mKey = key;
	}

	Header(const std::string& key, const std::string& value) {
		mKey = key;
        mValues = value;
	}

	std::string getKey() {
		return mKey;
	}

	const std::string& getValue() {
		return mValues;
	}

	void setValue(std::string value) {
        mValues = value;
	}

	bool operator<(const Header& r) const {
		if(toLowerCaseStr(this->mKey).compare(toLowerCaseStr(r.mKey)) < 0) {
			return true;
		} else {
			return false;
		}
	}
	~Header() {
	}
};


#endif /* SRC_HEADER_H_ */
