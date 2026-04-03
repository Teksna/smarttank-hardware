#pragma once
#include <HTTPClient.h>

class HttpClientWrapper {
public:
    static int get(String url, String& response, String apiKey);
    static int post(String url, String payload, String apiKey);
    static int patch(String url, String payload, String apiKey);
};