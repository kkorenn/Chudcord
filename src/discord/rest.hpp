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
        void send_message(const std::string& channel_id, const std::string& content, const std::string& guild_id = "", const std::string& reply_id = "", const std::string& file_path = "", ResponseCallback callback = nullptr);
        void ack_message(const std::string& channel_id, const std::string& message_id);

    private:
        struct UploadInfo {
            std::string upload_url;
            std::string upload_filename;
            std::string id;
        };
        
        void get_upload_url(const std::string& channel_id, const std::string& file_path, std::function<void(bool, UploadInfo)> callback);
        void upload_to_gcs(const std::string& url, const std::string& file_path, std::function<void(bool)> callback);
        static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
        void perform_request(const std::string& endpoint, const std::string& method, const json& body, ResponseCallback callback);

        std::string m_token;
        CURL* m_curl;
    };

}
