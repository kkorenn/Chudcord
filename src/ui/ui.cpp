#include "ui.hpp"
#include "../core/app.hpp"
#include <iostream>

namespace discord {

    UI::UI() : m_window(nullptr) {
        memset(m_input_buffer, 0, sizeof(m_input_buffer));
    }

    UI::~UI() {
        shutdown();
    }

    bool UI::init() {
        glfwSetErrorCallback([](int error, const char* description) {
            fprintf(stderr, "GLFW Error %d: %s\n", error, description);
        });

        if (!glfwInit()) return false;

        // GL 3.2 + GLSL 150 (Standard for macOS Core Profile)
        const char* glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ on macOS requires core profile
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on macOS

        m_window = glfwCreateWindow(1280, 720, "Native Discord Client", NULL, NULL);
        if (!m_window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            return false;
        }

        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1); // Enable vsync

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);

        return true;
    }

    void UI::shutdown() {
        // Only shutdown if window was created to avoid assertion failures
        if (m_window) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();
    }

    bool UI::should_close() {
        return glfwWindowShouldClose(m_window);
    }

    void UI::new_frame() {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void UI::update_icon_texture(const std::string& guild_id, unsigned char* data, int width, int height) {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        if (m_guild_icons.count(guild_id)) {
            glDeleteTextures(1, &m_guild_icons[guild_id]);
        }
        m_guild_icons[guild_id] = texture;
    }

    void UI::update_attachment_texture(const std::string& att_id, unsigned char* data, int width, int height) {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        m_attachments[att_id] = texture;
    }

    void UI::render(const State& state) {
        // Create a full-screen window for the layout
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

        // --- Left Panel: Server List ---
        ImGui::BeginChild("Servers", ImVec2(70, 0), true);
        if (state.guilds.empty()) {
            ImGui::Text("...");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Waiting for guilds...");
        }
        for (const auto& guild : state.guilds) {
            std::string label = guild.name.substr(0, 1); // Single initial
            
            ImGui::PushID(guild.id.c_str());
            
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            float radius = 22.0f;
            ImVec2 center = ImVec2(p.x + 25.0f, p.y + 25.0f);
            
            auto it = m_guild_icons.find(guild.id);
            if (it != m_guild_icons.end()) {
                // Draw icon texture
                draw_list->AddImage((void*)(intptr_t)it->second, ImVec2(p.x + 5.0f, p.y + 5.0f), ImVec2(p.x + 45.0f, p.y + 45.0f));
            } else {
                // No texture yet, check if we should trigger download
                if (!guild.icon.empty() && on_load_icon) {
                    on_load_icon(guild.id, guild.icon);
                }

                size_t hash = std::hash<std::string>{}(guild.id);
                ImU32 col = IM_COL32((hash & 0x7F) + 100, ((hash >> 8) & 0x7F) + 100, ((hash >> 16) & 0x7F) + 100, 255);
                
                draw_list->AddCircleFilled(center, radius, col);
                
                ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
                draw_list->AddText(ImVec2(center.x - text_size.x/2, center.y - text_size.y/2), IM_COL32_WHITE, label.c_str());
            }

            if (ImGui::InvisibleButton("##btn", ImVec2(50, 50))) {
                if (on_guild_selected) on_guild_selected(guild.id);
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", guild.name.c_str());
            }
            
            ImGui::PopID();
            ImGui::Spacing();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // --- Middle Panel: Channel List ---
        ImGui::BeginChild("Channels", ImVec2(200, 0), true);
        ImGui::Text("Channels");
        ImGui::Separator();
        
        if (!state.current_guild_id.empty()) {
            // Find current guild
            const Guild* current_guild = nullptr;
            for (const auto& g : state.guilds) {
                if (g.id == state.current_guild_id) {
                    current_guild = &g;
                    break;
                }
            }

            if (current_guild) {
                // First, list channels with no category
                for (const auto& channel : current_guild->channels) {
                    if (channel.type == 0 && channel.parent_id.empty()) {
                        bool is_selected = (state.current_channel_id == channel.id);
                        if (ImGui::Selectable(("# " + channel.name).c_str(), is_selected)) {
                            if (on_channel_selected) on_channel_selected(channel.id);
                        }
                    }
                }

                // Then, list by category
                for (const auto& category : current_guild->channels) {
                    if (category.type == 4) { // Category type
                        ImGui::Spacing();
                        ImGui::TextDisabled("%s", category.name.c_str());
                        
                        for (const auto& channel : current_guild->channels) {
                            if (channel.type == 0 && channel.parent_id == category.id) {
                                ImGui::Indent(10.0f);
                                bool is_selected = (state.current_channel_id == channel.id);
                                if (ImGui::Selectable(("# " + channel.name).c_str(), is_selected)) {
                                    if (on_channel_selected) on_channel_selected(channel.id);
                                }
                                ImGui::Unindent(10.0f);
                            }
                        }
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // --- Right Panel: Chat ---
        ImGui::BeginChild("Chat", ImVec2(0, 0), true);
        
        // Chat History Area
        float footer_height = state.reply_msg_id.empty() ? 50.0f : 80.0f;
        ImGui::BeginChild("Messages", ImVec2(0, -footer_height), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        
        if (!state.current_channel_id.empty()) {
            if (!state.channel_error.empty()) {
                ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() / 2 - 100, ImGui::GetWindowHeight() / 2));
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No access to this channel.");
                ImGui::TextDisabled(" (Maybe check your permissions?)");
            } else {
                auto it = state.messages.find(state.current_channel_id);
                if (it != state.messages.end()) {
                    for (const auto& msg : it->second) {
                        ImGui::PushID(msg.id.c_str());
                        
                        // If this is a reply, show a small context bar
                        if (msg.message_reference.has_value() && !msg.message_reference->message_id.empty()) {
                            // Find the author of the message we are replying to if it's in history
                            std::string reply_to_author = "someone";
                            auto it_msg = state.messages.find(state.current_channel_id);
                            if (it_msg != state.messages.end()) {
                                for (const auto& m : it_msg->second) {
                                    if (m.id == msg.message_reference->message_id) {
                                        reply_to_author = m.author.username;
                                        break;
                                    }
                                }
                            }
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  ^ Replying to @%s", reply_to_author.c_str());
                        }

                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", msg.author.username.c_str());
                        ImGui::SameLine();
                        ImGui::TextDisabled(" [%s]", msg.timestamp.c_str());
                        
                        // Reply button on right
                        ImGui::SameLine(ImGui::GetWindowWidth() - 70);
                        if (ImGui::SmallButton("Reply")) {
                            if (on_reply_selected) on_reply_selected(msg.id, msg.author.username, msg.content, msg.guild_id);
                        }

                        if (!msg.content.empty()) {
                            ImGui::TextWrapped("%s", msg.content.c_str());
                        }

                        // Render Attachments
                        for (const auto& att : msg.attachments) {
                            if (att.content_type.starts_with("image/")) {
                                auto it_att = m_attachments.find(att.id);
                                if (it_att != m_attachments.end()) {
                                    float aspect = (float)att.height / (float)att.width;
                                    float draw_w = std::min(400.0f, (float)att.width);
                                    float draw_h = draw_w * aspect;
                                    ImGui::Image((void*)(intptr_t)it_att->second, ImVec2(draw_w, draw_h));
                                } else {
                                    if (on_load_attachment) on_load_attachment(att.id, att.url);
                                    ImGui::TextDisabled("[Loading Image: %s]", att.filename.c_str());
                                }
                            } else {
                                ImGui::TextColored(ImVec4(0.4f, 0.4f, 1.0f, 1.0f), "[Attachment: %s]", att.filename.c_str());
                            }
                        }

                        ImGui::Separator();
                        ImGui::PopID();
                    }
                    
                    // Smart auto-scroll logic
                    if (m_scroll_to_bottom) {
                        ImGui::SetScrollHereY(1.0f);
                        m_scroll_to_bottom = false;
                    } else if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) {
                        ImGui::SetScrollHereY(1.0f);
                    }
                }
            }
        }
        ImGui::EndChild();

        // Input Area
        ImGui::Separator();

        // Reply preview if active
        if (!state.reply_msg_id.empty()) {
            std::string preview = state.reply_content;
            if (preview.length() > 50) preview = preview.substr(0, 47) + "...";
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Replying to: %s", preview.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel Reply")) {
                if (on_reply_selected) on_reply_selected("", "", "", "");
            }
        }

        ImGui::PushItemWidth(-1);
        bool reclaim_focus = false;
        
        if (ImGui::InputText("##Input", m_input_buffer, IM_ARRAYSIZE(m_input_buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string content(m_input_buffer);
            if (!content.empty() && !state.current_channel_id.empty()) {
                if (on_send_message) on_send_message(content, state.reply_msg_id);
                memset(m_input_buffer, 0, sizeof(m_input_buffer));
                m_scroll_to_bottom = true; // Force scroll on self-send
                reclaim_focus = true;
            }
        }
        ImGui::PopItemWidth();
        if (reclaim_focus) {
            ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
        }

        ImGui::EndChild(); // Chat

        ImGui::End(); // Main
        ImGui::PopStyleVar();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(m_window);
    }

}
