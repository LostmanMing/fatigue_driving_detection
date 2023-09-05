#ifndef SRC_HASHER_H_
#define SRC_HASHER_H_

#include <string>
#include "constants.h"
//#include "openssl/sha.h"

class Hasher {
public:
	Hasher();
	const std::string hexEncode(unsigned char * md, size_t len);
	int hashSHA256(const std::string str, unsigned char* hash);
	unsigned char * hmac(void* key, unsigned int keyLength, std::string data);
};

#endif /* SRC_HASHER_H_ */
