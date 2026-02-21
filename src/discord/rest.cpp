#include "rest.hpp"
#include <iostream>
#include <thread>
#include <chrono>
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
        std::string token = m_token;
        std::thread([endpoint, method, body, callback, token]() {
            CURL* curl = curl_easy_init();
            if (curl) {
                std::string readBuffer;
                std::string url = "https://discord.com/api/v9" + endpoint;

                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, ("Authorization: " + token).c_str());
                headers = curl_slist_append(headers, "Content-Type: application/json");
                headers = curl_slist_append(headers, "User-Agent: Chudcord/1.0");

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                
                std::string json_str;
                if (method == "POST") {
                    curl_easy_setopt(curl, CURLOPT_POST, 1L);
                    json_str = body.dump();
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
                } else if (method == "GET") {
                    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
                }

                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

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

    void Rest::ack_message(const std::string& channel_id, const std::string& message_id) {
        std::string token = m_token;
        std::thread([this, channel_id, message_id, token]() {
            CURL* curl = curl_easy_init();
            if (curl) {
                std::string url = "https://discord.com/api/v9/channels/" + channel_id + "/messages/" + message_id + "/ack";
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, ("Authorization: " + token).c_str());
                headers = curl_slist_append(headers, "Content-Type: application/json");
                headers = curl_slist_append(headers, "User-Agent: Chudcord/1.0");

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"token\": null}");
                curl_easy_perform(curl);
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
            }
        }).detach();
    }

    void Rest::send_message(const std::string& channel_id, const std::string& content, const std::string& guild_id, const std::string& reply_id, const std::string& file_path, ResponseCallback callback) {
        std::string token = m_token;
        
        if (file_path.empty()) {
            std::thread([this, channel_id, content, guild_id, reply_id, callback, token]() {
                CURL* curl = curl_easy_init();
                if (curl) {
                    std::string readBuffer;
                    std::string url = "https://discord.com/api/v9/channels/" + channel_id + "/messages";
                    struct curl_slist* headers = NULL;
                    headers = curl_slist_append(headers, ("Authorization: " + token).c_str());
                    headers = curl_slist_append(headers, "Content-Type: application/json");
                    headers = curl_slist_append(headers, "User-Agent: Chudcord/1.0");

                    std::string nonce = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                    json payload = {{"content", content}, {"tts", false}, {"nonce", nonce}};
                    if (!reply_id.empty()) {
                        payload["message_reference"] = {{"message_id", reply_id}, {"channel_id", channel_id}};
                        if (!guild_id.empty()) payload["message_reference"]["guild_id"] = guild_id;
                    }

                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                    curl_easy_setopt(curl, CURLOPT_POST, 1L);
                    std::string payload_str = payload.dump();
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

                    CURLcode res = curl_easy_perform(curl);
                    long http_code = 0;
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                    if (callback) {
                        json response_json;
                        try { if (!readBuffer.empty()) response_json = json::parse(readBuffer); } catch(...) {}
                        callback((res == CURLE_OK && http_code >= 200 && http_code < 300), response_json);
                    }
                    curl_slist_free_all(headers);
                    curl_easy_cleanup(curl);
                }
            }).detach();
        } else {
            get_upload_url(channel_id, file_path, [this, channel_id, content, guild_id, reply_id, file_path, callback, token](bool s, UploadInfo info) {
                if (!s) { if (callback) callback(false, json{{"error", "Failed to get upload URL"}}); return; }
                
                upload_to_gcs(info.upload_url, file_path, [this, channel_id, content, guild_id, reply_id, info, callback, token](bool s2) {
                    if (!s2) { if (callback) callback(false, json{{"error", "Failed to upload to GCS"}}); return; }

                    std::thread([this, channel_id, content, guild_id, reply_id, info, callback, token]() {
                        CURL* curl = curl_easy_init();
                        if (curl) {
                            std::string readBuffer;
                            std::string url = "https://discord.com/api/v9/channels/" + channel_id + "/messages";
                            struct curl_slist* headers = NULL;
                            headers = curl_slist_append(headers, ("Authorization: " + token).c_str());
                            headers = curl_slist_append(headers, "Content-Type: application/json");
                            headers = curl_slist_append(headers, "User-Agent: Chudcord/1.0");

                            json payload = {
                                {"content", content}, 
                                {"attachments", json::array({{
                                    {"id", "0"}, 
                                    {"filename", info.upload_filename}, 
                                    {"uploaded_filename", info.upload_filename}
                                }})}
                            };
                            if (!reply_id.empty()) {
                                payload["message_reference"] = {{"message_id", reply_id}, {"channel_id", channel_id}};
                                if (!guild_id.empty()) payload["message_reference"]["guild_id"] = guild_id;
                            }

                            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                            curl_easy_setopt(curl, CURLOPT_POST, 1L);
                            std::string payload_str = payload.dump();
                            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
                            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

                            CURLcode res = curl_easy_perform(curl);
                            if (callback) {
                                json response_json;
                                try { if (!readBuffer.empty()) response_json = json::parse(readBuffer); } catch(...) {}
                                callback(res == CURLE_OK, response_json);
                            }
                            curl_slist_free_all(headers);
                            curl_easy_cleanup(curl);
                        }
                    }).detach();
                });
            });
        }
    }

    void Rest::get_upload_url(const std::string& channel_id, const std::string& file_path, std::function<void(bool, UploadInfo)> callback) {
        std::string token = m_token;
        std::thread([this, channel_id, file_path, callback, token]() {
            CURL* curl = curl_easy_init();
            if (curl) {
                std::string readBuffer;
                std::string url = "https://discord.com/api/v9/channels/" + channel_id + "/attachments";
                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, ("Authorization: " + token).c_str());
                headers = curl_slist_append(headers, "Content-Type: application/json");
                headers = curl_slist_append(headers, "User-Agent: Chudcord/1.0");

                std::string filename = file_path.substr(file_path.find_last_of("/\\") + 1);
                FILE* f = fopen(file_path.c_str(), "rb");
                if (!f) { callback(false, {}); return; }
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fclose(f);

                json body = {{"files", json::array({{{"filename", filename}, {"file_size", size}, {"id", "1"}}})}};
                std::string body_str = body.dump();

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

                CURLcode res = curl_easy_perform(curl);
                if (res == CURLE_OK) {
                    try {
                        json j = json::parse(readBuffer);
                        UploadInfo info;
                        info.upload_url = j["attachments"][0]["upload_url"];
                        info.upload_filename = j["attachments"][0]["upload_filename"];
                        info.id = j["attachments"][0]["id"];
                        callback(true, info);
                    } catch(...) { callback(false, {}); }
                } else { callback(false, {}); }
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
            }
        }).detach();
    }

    void Rest::upload_to_gcs(const std::string& url, const std::string& file_path, std::function<void(bool)> callback) {
        std::thread([url, file_path, callback]() {
            CURL* curl = curl_easy_init();
            if (curl) {
                FILE* f = fopen(file_path.c_str(), "rb");
                if (!f) { callback(false); return; }
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                rewind(f);

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
                curl_easy_setopt(curl, CURLOPT_READDATA, f);
                curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)size);

                CURLcode res = curl_easy_perform(curl);
                fclose(f);
                callback(res == CURLE_OK);
                curl_easy_cleanup(curl);
            }
        }).detach();
    }

}
