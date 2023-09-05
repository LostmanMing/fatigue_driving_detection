/*
* RequestParams.h
*
*  Created on: 2018Äê8ÔÂ1ÈÕ
*      Author: h84106315
*/

#ifndef SRC_REQUESTPARAMS_H_
#define SRC_REQUESTPARAMS_H_
#include <string>
#include <set>
#include "header.h"

class RequestParams {
private:
    /* HTTP Request Parameters */
    std::string mMethod;
    std::string mHost;
    std::string mUri;
    std::string mQueryParams;
    std::string mPayload;

    std::set<Header> mHeaders;

public:
    RequestParams();
    RequestParams(std::string method, std::string host, std::string uri,
        std::string queryParams);
    RequestParams(std::string method, std::string host, std::string uri,
        std::string queryParams, std::string payload);
    virtual ~RequestParams();

    const std::string getMethod();
    const std::string getHost();
    const std::string getUri();
    const std::string getQueryParams();
    const std::string getPayload();
    const std::set<Header>* getHeaders();

    void addHeader(Header& header);
    void addHeader(std::string key, std::string value);
    std::string initHeaders();
};

#endif /* SRC_REQUESTPARAMS_H_ */
