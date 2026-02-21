#pragma once

#include <string>
#include <functional>
#include <vector>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

namespace discord {

    using json = nlohmann::json;

    class Rest {
    public:
        Rest(const std::string& token);
        ~Rest();

        using ResponseCallback = std::function<void(bool success, const json& data)>;

        void get_guilds(ResponseCallback callback);
        void get_channels(const std::string& guild_id, ResponseCallback callback);
        void get_messages(const std::string& channel_id, ResponseCallback callback);
        void send_message(const std::string& channel_id, const std::string& content, const std::string& guild_id = "", const std::string& reply_id = "", ResponseCallback callback = nullptr);

    private:
        static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
        void perform_request(const std::string& endpoint, const std::string& method, const json& body, ResponseCallback callback);

        std::string m_token;
        CURL* m_curl;
    };

}
