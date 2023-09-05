#include "signer.h"
#include <sstream>
#include <cctype>
#include <iterator>
#include "utils.h"
#include "constants.h"
#include <assert.h>
#include <cstring>
#include <algorithm>
#include "openssl/sha.h"
/* Helpers
*
*/

/* Signer's
*
*/

Signer::Signer(std::string appKey, std::string appSecret) {
    mAppKey = appKey;
    mAppSecret = appSecret;
    hasher = new Hasher();
}

Signer::Signer() {
    hasher = new Hasher();
}

Signer::~Signer() {
    delete hasher;
}


/*
* Create signature in one stroke
*
*/
const std::string Signer::createSignature(RequestParams * request) {
    std::string datetime = request->initHeaders();
    std::string signedHeaders = getSignedHeaders(request->getHeaders());
    std::string canonRequest = getCanonicalRequest(signedHeaders, request->getMethod(),
        request->getUri(), request->getQueryParams(), request->getHeaders(),
        request->getPayload());
    std::string stringToSign = getStringToSign(defaults::sdk_signing_algorithm,
        datetime, canonRequest);
    std::string signature = getSignature((char *)mAppSecret.c_str(), stringToSign);
    std::string algoStr;
    std::string key = "Authorization";
    std::string value = "SDK-HMAC-SHA256 Access=" + mAppKey + ", SignedHeaders="
        + signedHeaders + ", Signature=" + signature;
    request->addHeader(key, value);
    return signature;
}


/*
*
*Task 1: To create canonicalized request string
*
*
*/
const std::string Signer::getCanonicalRequest(std::string signedHeaders, std::string method, std::string uri,
    std::string queryParams, const std::set<Header>* headers, std::string payload) {
    std::string hexencode;
    std::set<Header>::iterator it = headers->find(Header("X-Sdk-Content-Sha256", ""));
    if (it != headers->end()) {
        Header entry = *it;
        hexencode = entry.getValue();
    }
    else {
        hexencode = getHexHash(payload);
    }

    return method + "\n" + getCanonicalURI(uri) + "\n" + getCanonicalQueryString(queryParams) + "\n"
        + getCanonicalHeaders(headers) + "\n" + signedHeaders + "\n" + hexencode;
}


const std::string Signer::getCanonicalURI(std::string& uri) {
    if (uri.empty()) {
        return std::string("/");
    }
    if (uri[uri.length() - 1] != '/') {
        return  uriEncode(uri, true) + "/";

    }
    else {
        return uriEncode(uri, true);
    }
}

/*
* Canonicalize all the query parameters
* Take in the entire string of query parameters and parse them into a map, which
* sorts all parameters alphabetically
* 1. Parameters cannot have the same name, the parameter will be overwritten by its last value
* 2. Paramters without "=" sign means it does not have a value, which not be saved
*/
const std::string Signer::getCanonicalQueryString(std::string& queryParams) {
    std::map<std::string, std::vector<std::string>> paramsMap;
    std::stringstream ss(queryParams);
    std::string param;
    while (std::getline(ss, param, '&')) {
        std::size_t pos = param.find('=');
        std::string key;
        std::string value;
        if (pos == std::string::npos) {
            key = uriDecode(param);
            value = "";
        }
        else {
            key = param.substr(0, pos);
            value = param.substr(pos + 1);
            key = uriDecode(key);
            value = uriDecode(value);
        }
        key = uriEncode(key);
        value = uriEncode(value);
        std::map<std::string, std::vector<std::string>>::iterator it = paramsMap.find(key);
        if (it == paramsMap.end()) {
            std::vector<std::string> values;
            values.push_back(value);
            paramsMap[key] = values;
        }
        else {
            it->second.push_back(value);
        }
    }
    return getCanonicalQueryString(paramsMap);
}

/*
* Helper function for getting canonicalized query string
*
* Query entries are sorted by the map internally
*/
const std::string Signer::getCanonicalQueryString(std::map<std::string, std::string>& queryParams) { // @suppress("Unused function declaration")
    std::string canonQueryStr = "";
    if (queryParams.empty()) {
        return canonQueryStr;
    }
    std::map<std::string, std::string>::iterator it;
    for (it = queryParams.begin(); it != queryParams.end(); it++) {
        canonQueryStr.append(it->first);
        canonQueryStr.append("=");
        canonQueryStr.append(it->second);
        if (std::next(it) != queryParams.end()) {
            canonQueryStr.append("&");
        }
    }
    return canonQueryStr;
}

const std::string Signer::getCanonicalQueryString(std::map<std::string, std::vector<std::string>>& queryParams) {
    std::string canonQueryStr = "";
    if (queryParams.empty()) {
        return canonQueryStr;
    }
    std::map<std::string, std::vector<std::string>>::iterator it;
    for (it = queryParams.begin(); it != queryParams.end(); it++) {
        std::string key = it->first;
        std::vector<std::string> values = it->second;
        std::sort(values.begin(), values.end());
        for (std::vector<std::string>::iterator vit = values.begin(); vit != values.end(); vit++) {
            canonQueryStr.append(key);
            canonQueryStr.append("=");
            canonQueryStr.append(*vit);
            canonQueryStr.append("&");
        }
    }
    return canonQueryStr.substr(0, canonQueryStr.length() - 1);
}

/* Get canonicalized headers
* Headers are sorted by their names in the set internally
*/
const std::string Signer::getCanonicalHeaders(const std::set<Header>* headers) {
    std::string canonicalHeaders = "";
    for (auto header : *headers) {
        std::string headerEntry = toLowerCaseStr(header.getKey()) + ":" + trim(header.getValue()) + "\n";
        canonicalHeaders += headerEntry;
    }
    return canonicalHeaders;
}


/*
* Get signed headers
* Take in a set of header objects and output a string
*
*/
const std::string Signer::getSignedHeaders(const std::set<Header>* headers) {
    std::string signedHeaders = "";
    std::set<Header>::iterator it;
    for (it = headers->begin(); it != headers->end(); it++) {
        Header entry = *it;
        signedHeaders += toLowerCaseStr(entry.getKey());
        if (std::next(it) != headers->end()) {
            signedHeaders += ";";
        }
    }
    return signedHeaders;
}


const std::string Signer::getHexHash(std::string& payload) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    hasher->hashSHA256(payload, hash);
    return hasher->hexEncode(hash, SHA256_DIGEST_LENGTH);
}

const std::string Signer::getStringToSign(std::string algorithm,
    std::string datetime, std::string canonicalRequest) {
    return algorithm + "\n" + datetime + "\n" 
        + getHexHash(canonicalRequest);
}

const std::string Signer::getSignature(char * signingKey, const std::string& stringToSign) {
    unsigned int digestLength = SHA256_DIGEST_LENGTH;
    unsigned char * signatureChar = hasher->hmac(signingKey, strlen(signingKey),
        stringToSign);
    std::string signature = hasher->hexEncode(signatureChar, digestLength);
    delete[] signatureChar;
    return signature;
}


