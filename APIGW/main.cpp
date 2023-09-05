include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "signer.h"

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    printf("%.*s", size * nmemb, (char*)contents);
    return size * nmemb;
}

int main(void)
{
    //The following example shows how to set the request URL and parameters to query a VPC list.

    //Specify a request method, such as GET, PUT, POST, DELETE, HEAD, and PATCH.
    //Set a request URL.
    //Set parameters for the request URL.
    //Add a body if you have specified the PUT or POST method. Special characters, such as the double quotation mark ("), contained in the body must be escaped.
    RequestParams* request = new RequestParams("GET", "endpoint.example.com", "/v1/77b6a44cba5143ab91d13ab9a8ff44fd/vpcs",
        "limit=2", "");
    //Add header parameters, for example, x-domain-id for invoking a global service and x-project-id for invoking a project-level service.
    request->addHeader("content-type", "application/json");
    //Set the AK/SK to sign and authenticate the request.
    Signer signer("QTWAOYTTINDUT2QVKYUC", "MFyfvK41ba2giqM7**********KGpownRZlmVmHc");

    //generate signature. generated Authorization header is inserted into params.headers
    signer.createSignature(request);

    //send http request using curl library
    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();
    CURLcode res;

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request->getMethod().c_str());
    std::string url = "https://" + request->getHost() + request->getUri() + "?" + request->getQueryParams();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    struct curl_slist *chunk = NULL;
    std::set<Header>::iterator it;
    for (auto header : *request->getHeaders()) {
        std::string headerEntry = header.getKey() + ": " + header.getValue();
        printf("%s\n", headerEntry.c_str());
        chunk = curl_slist_append(chunk, headerEntry.c_str());
    }
    printf("-------------\n");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, request->getPayload().c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }
    else {
        long status;
        curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status);
        printf("\nstatus %d\n", status);
    }
    curl_easy_cleanup(curl);

    curl_global_cleanup();

    return 0;
}

