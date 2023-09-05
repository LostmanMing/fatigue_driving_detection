#include "hasher.h"
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include "utils.h"
#include <cstring>
#include <assert.h>

Hasher::Hasher() {
}

const std::string Hasher::hexEncode(unsigned char* md, size_t length) {
    if (length <= 0) {
        return "";
    }
	char * buffer = new char[length * 2 + 1];

	for (size_t i = 0; i < length; i++) {
		sprintf(buffer + i * 2, "%02x", md[i]);
	}

	std::string str(buffer);
	delete[] buffer;
	return str;
}


int Hasher::hashSHA256(const std::string str, unsigned char *hash) {
	// Hash the input string
	char * cbuffer = new char[str.length() + 1];
	strcpy(cbuffer, str.c_str());

	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, cbuffer, str.length());
	SHA256_Final(hash, &sha256);

	delete[] cbuffer;
    return SHA256_DIGEST_LENGTH;
}

unsigned char * Hasher::hmac(void* key, unsigned int keyLength, std::string data) {
	const EVP_MD * engine = EVP_sha256();
	unsigned int maxDigestLength = SHA256_DIGEST_LENGTH;

	unsigned char *md = new unsigned char[maxDigestLength];
	unsigned int mdLength;

	char * charData = new char[data.length()+1];
	strcpy(charData, data.c_str());

	HMAC(engine, key, keyLength, (unsigned char *)charData, strlen(charData), md, &mdLength);

	delete[] charData;
	return md;
}


