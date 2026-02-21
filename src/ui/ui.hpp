#pragma once

#include <string>
#include <vector>
#include <functional>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <unordered_map>

namespace discord {

    struct State; // Forward declaration

    class UI {
    public:
        UI();
        ~UI();

        bool init();
        void shutdown();
        
        bool should_close();
        void new_frame();
        void render(const State& state);
        
        // Input handling
        std::function<void(const std::string&, const std::string&)> on_send_message; // content, reply_id
        std::function<void(const std::string&)> on_channel_selected;
        std::function<void(const std::string&)> on_guild_selected;
        std::function<void(const std::string&, const std::string&)> on_load_icon;
        std::function<void(const std::string&, const std::string&, const std::string&, const std::string&)> on_reply_selected; // msg_id, username, content, guild_id

        // Texture management (call from main thread)
        void update_icon_texture(const std::string& guild_id, unsigned char* data, int width, int height);
        void clear_reply(); // Helper for UI to clear reply state locally if needed

    private:
        GLFWwindow* m_window;
        char m_input_buffer[1024];
        bool m_scroll_to_bottom{false};
        std::string m_last_channel_id;

        std::unordered_map<std::string, unsigned int> m_guild_icons;
    };

}
