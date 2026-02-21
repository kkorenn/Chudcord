#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace discord {

    using json = nlohmann::json;

    struct User {
        std::string id;
        std::string username;
        std::string discriminator;
        std::string avatar;
    };

    inline void from_json(const json& j, User& u) {
        j.at("id").get_to(u.id);
        j.at("username").get_to(u.username);
        if (j.contains("discriminator") && !j["discriminator"].is_null())
            j.at("discriminator").get_to(u.discriminator);
        if (j.contains("avatar") && !j["avatar"].is_null())
            j.at("avatar").get_to(u.avatar);
    }

    inline void to_json(json& j, const User& u) {
        j = json{{"id", u.id}, {"username", u.username}, {"discriminator", u.discriminator}, {"avatar", u.avatar}};
    }

    struct Channel {
        std::string id;
        int type;
        std::string guild_id;
        std::string name;
        int position;
        std::string topic;
        std::string last_message_id;
        std::string parent_id; // Category ID

        // Helper for partial updates
        void update_from(const Channel& other) {
            if (!other.name.empty()) name = other.name;
            if (!other.topic.empty()) topic = other.topic;
            if (!other.last_message_id.empty()) last_message_id = other.last_message_id;
            if (!other.parent_id.empty()) parent_id = other.parent_id;
        }
    };

    inline void to_json(json& j, const Channel& c) {
        j = json{{"id", c.id}, {"type", c.type}, {"name", c.name}, {"position", c.position}, {"topic", c.topic}, {"last_message_id", c.last_message_id}, {"parent_id", c.parent_id}};
        if (!c.guild_id.empty()) j["guild_id"] = c.guild_id;
    }

    inline void from_json(const json& j, Channel& c) {
        j.at("id").get_to(c.id);
        j.at("type").get_to(c.type);
        if (j.contains("guild_id")) j.at("guild_id").get_to(c.guild_id);
        if (j.contains("name")) j.at("name").get_to(c.name);
        if (j.contains("position")) j.at("position").get_to(c.position);
        if (j.contains("topic") && !j["topic"].is_null()) j.at("topic").get_to(c.topic);
        if (j.contains("last_message_id") && !j["last_message_id"].is_null()) j.at("last_message_id").get_to(c.last_message_id);
        if (j.contains("parent_id") && !j["parent_id"].is_null()) j.at("parent_id").get_to(c.parent_id);
    }

    struct MessageReference {
        std::string message_id;
        std::string channel_id;
        std::string guild_id;
    };

    inline void from_json(const json& j, MessageReference& mr) {
        if (j.contains("message_id")) j.at("message_id").get_to(mr.message_id);
        if (j.contains("channel_id")) j.at("channel_id").get_to(mr.channel_id);
        if (j.contains("guild_id")) j.at("guild_id").get_to(mr.guild_id);
    }

    struct Message {
        std::string id;
        std::string channel_id;
        std::string guild_id;
        User author;
        std::string content;
        std::string timestamp;
        std::optional<MessageReference> message_reference;
    };

    inline void from_json(const json& j, Message& m) {
        j.at("id").get_to(m.id);
        j.at("channel_id").get_to(m.channel_id);
        if (j.contains("guild_id")) j.at("guild_id").get_to(m.guild_id);
        j.at("author").get_to(m.author);
        j.at("content").get_to(m.content);
        if (j.contains("timestamp") && !j["timestamp"].is_null())
            j.at("timestamp").get_to(m.timestamp);
        if (j.contains("message_reference") && !j["message_reference"].is_null()) {
            m.message_reference = j["message_reference"].get<MessageReference>();
        }
    }

    inline void to_json(json& j, const Message& m) {
        j = json{{"id", m.id}, {"channel_id", m.channel_id}, {"author", m.author}, {"content", m.content}, {"timestamp", m.timestamp}};
        if (!m.guild_id.empty()) j["guild_id"] = m.guild_id;
    }

    struct Guild {
        std::string id;
        std::string name;
        std::string icon;
        std::vector<Channel> channels;
    };

    inline void from_json(const json& j, Guild& g) {
        j.at("id").get_to(g.id);
        if (j.contains("name")) j.at("name").get_to(g.name);
        if (j.contains("icon") && !j["icon"].is_null()) j.at("icon").get_to(g.icon);
        if (j.contains("channels") && j["channels"].is_array()) {
            g.channels.clear();
            for (const auto& c_json : j["channels"]) {
                g.channels.push_back(c_json.get<Channel>());
            }
        }
    }

    inline void to_json(json& j, const Guild& g) {
        j = json{{"id", g.id}, {"name", g.name}, {"icon", g.icon}, {"channels", g.channels}};
    }

    // Gateway Payloads
    struct HelloPayload {
        int heartbeat_interval;
    };
    
    inline void from_json(const json& j, HelloPayload& p) {
        j.at("heartbeat_interval").get_to(p.heartbeat_interval);
    }

    struct IdentifyProperties {
        std::string os;
        std::string browser;
        std::string device;
    };

    inline void to_json(json& j, const IdentifyProperties& p) {
        j = json{{"$os", p.os}, {"$browser", p.browser}, {"$device", p.device}};
    }

    struct IdentifyPayload {
        std::string token;
        int intents;
        IdentifyProperties properties;
    };

    inline void to_json(json& j, const IdentifyPayload& p) {
        j = json{{"token", p.token}, {"intents", p.intents}, {"properties", p.properties}};
    }

}
