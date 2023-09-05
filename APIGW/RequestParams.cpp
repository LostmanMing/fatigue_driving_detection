/*
* RequestParams.cpp
*
*  Created on: 2018Äê8ÔÂ1ÈÕ
*      Author: h84106315
*/

#include "RequestParams.h"
#include "constants.h"
#include <ctime>

RequestParams::RequestParams() {
}

RequestParams::~RequestParams() {
}

RequestParams::RequestParams(std::string method, std::string host, std::string uri,
    std::string queryParams) {
    mMethod = method;
    mHost = host;
    mUri = uri;
    mQueryParams = queryParams;
    mPayload = "";
}

RequestParams::RequestParams(std::string method, std::string host, std::string uri,
    std::string queryParams, std::string payload) {
    mMethod = method;
    mHost = host;
    mUri = uri;
    mQueryParams = queryParams;
    mPayload = payload;
}

const std::string RequestParams::getMethod() {
    return mMethod;
}

const std::string RequestParams::getHost() {
    return mHost;
}

const std::string RequestParams::getUri() {
    return mUri;
}

const std::string RequestParams::getQueryParams() {
    return mQueryParams;
}

const std::string RequestParams::getPayload() {
    return mPayload;
}

const std::set<Header>* RequestParams::getHeaders() {
    return &mHeaders;
}

/*
* Add a new header
*/
void RequestParams::addHeader(Header& header) {
    mHeaders.insert(header);
}


/*
* Add a value to an existing header. If the header's key does not exsit, create
* a new header with the key and the value passed in.
*/
void RequestParams::addHeader(std::string key, std::string value) {
    mHeaders.insert(Header(key, value));
}

/*
* Initialize host and x-sdk-date headers
*/
std::string RequestParams::initHeaders() {
    std::set<Header>::iterator it = mHeaders.find(Header(defaults::host, ""));
    if (it == mHeaders.end()) {
        mHeaders.insert(Header(defaults::host, mHost));
    }
    std::set<Header>::iterator it2 = mHeaders.find(Header(defaults::dateFormat, ""));
    if (it2 != mHeaders.end()) {
        Header* h = (Header *)(&(*it2));
        return h->getValue();
    }
    else {
        time_t now;
        time(&now);
        std::string datetime = toISO8601Time(now);
        mHeaders.insert(Header(defaults::dateFormat, datetime));
        return datetime;
    }
}