#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <queue>
#include <functional>

#include "../discord/models.hpp"
#include "../discord/gateway.hpp"
#include "../discord/rest.hpp"
#include "../ui/ui.hpp"

namespace discord {

    struct State {
        std::string current_guild_id;
        std::string current_channel_id;
        std::string channel_error; // Error message if channel fails to load
        
        // Reply state
        std::string reply_msg_id;
        std::string reply_username;
        std::string reply_content;
        std::string reply_guild_id;
        
        // Use map for easier lookup by ID
        std::vector<Guild> guilds; // Vector for ordered display, or map for lookups? UI needs order. Vector is better for UI.
        std::unordered_map<std::string, Guild*> guild_map; // Helper for fast lookup
        
        std::unordered_map<std::string, std::vector<Message>> messages; // channel_id -> messages
        std::unordered_map<std::string, User> users;

        // Helpers
        Guild* get_guild(const std::string& id) {
            if (guild_map.find(id) != guild_map.end()) return guild_map[id];
            return nullptr;
        }
        
        Channel* get_channel(const std::string& guild_id, const std::string& channel_id) {
            Guild* g = get_guild(guild_id);
            if (!g) return nullptr;
            for (auto& c : g->channels) {
                if (c.id == channel_id) return &c;
            }
            return nullptr;
        }
    };

    struct Config {
        std::string token;
    };

    class App {
    public:
        App();
        ~App();

        bool init(const std::string& config_path);
        void run();

    private:
        void load_config(const std::string& path);
        void handle_event(const std::string& event, const json& data);
        
        // Thread-safe event queue processing
        void process_main_thread_tasks();
        void post_task(std::function<void()> task);

        Config m_config;
        State m_state;
        std::recursive_mutex m_state_mutex;
        
        std::unique_ptr<Gateway> m_gateway;
        std::unique_ptr<Rest> m_rest;
        std::unique_ptr<UI> m_ui;

        std::queue<std::function<void()>> m_task_queue;
        std::mutex m_queue_mutex;
        bool m_running;
    };

}
