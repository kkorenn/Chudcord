#include "rest.hpp"
#include <iostream>
#include <thread>
#include <curl/curl.h>

namespace discord {

    Rest::Rest(const std::string& token) : m_token(token) {
        curl_global_init(CURL_GLOBAL_ALL);
    }

    Rest::~Rest() {
        curl_global_cleanup();
    }

    size_t Rest::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    void Rest::perform_request(const std::string& endpoint, const std::string& method, const json& body, ResponseCallback callback) {
        std::string token = m_token; // Capture by value
        std::thread([endpoint, method, body, callback, token]() {
            CURL* curl = curl_easy_init();
            if (curl) {
                std::string readBuffer;
                std::string url = "https://discord.com/api/v10" + endpoint;

                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, ("Authorization: " + token).c_str());
                headers = curl_slist_append(headers, "Content-Type: application/json");
                headers = curl_slist_append(headers, "User-Agent: NativeDiscordClient/1.0");

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

                if (method == "POST") {
                    curl_easy_setopt(curl, CURLOPT_POST, 1L);
                    std::string json_str = body.dump();
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)json_str.length());
                    
                    // We must perform the request while json_str is still in scope.
                    // The thread blocks here during perform, so this is safe.
                    CURLcode res = curl_easy_perform(curl);
                    long http_code = 0;
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                    
                    bool success = (res == CURLE_OK) && (http_code >= 200 && http_code < 300);
                    if (callback) {
                        json response_json;
                        try {
                            if (!readBuffer.empty()) response_json = json::parse(readBuffer);
                        } catch(...) {}
                        callback(success, response_json);
                    }
                } else {
                    CURLcode res = curl_easy_perform(curl);
                    long http_code = 0;
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                    
                    bool success = (res == CURLE_OK) && (http_code >= 200 && http_code < 300);
                    if (callback) {
                        json response_json;
                        try {
                            if (!readBuffer.empty()) response_json = json::parse(readBuffer);
                        } catch(...) {}
                        callback(success, response_json);
                    }
                }

                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
            }
        }).detach();
    }

    void Rest::get_guilds(ResponseCallback callback) {
        perform_request("/users/@me/guilds", "GET", json(), callback);
    }

    void Rest::get_channels(const std::string& guild_id, ResponseCallback callback) {
        perform_request("/guilds/" + guild_id + "/channels", "GET", json(), callback);
    }

    void Rest::get_messages(const std::string& channel_id, ResponseCallback callback) {
        perform_request("/channels/" + channel_id + "/messages?limit=50", "GET", json(), callback);
    }

    void Rest::send_message(const std::string& channel_id, const std::string& content, const std::string& guild_id, const std::string& reply_id, ResponseCallback callback) {
        json body = {{"content", content}, {"tts", false}};
        if (!reply_id.empty()) {
            body["message_reference"] = {
                {"message_id", reply_id},
                {"channel_id", channel_id}
            };
            if (!guild_id.empty()) {
                body["message_reference"]["guild_id"] = guild_id;
            }
        }
        perform_request("/channels/" + channel_id + "/messages", "POST", body, callback);
    }

}
