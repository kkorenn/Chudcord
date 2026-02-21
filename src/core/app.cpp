#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "app.hpp"

#include <fstream>
#include <iostream>
#include <set>

namespace discord {

    App::App() : m_running(false) {}

    App::~App() {
        if (m_gateway) m_gateway->close();
        if (m_ui) m_ui->shutdown();
    }

    bool App::init(const std::string& config_path) {
        load_config(config_path);
        if (m_config.token.empty()) {
            std::cerr << "Token not found in config.json" << std::endl;
            return false;
        }

        m_ui = std::make_unique<UI>();
        if (!m_ui->init()) {
            std::cerr << "Failed to initialize UI" << std::endl;
            return false;
        }

        m_rest = std::make_unique<Rest>(m_config.token);

        static std::set<std::string> requested_icons;
        m_ui->on_load_icon = [this](const std::string& guild_id, const std::string& icon_hash) {
            if (requested_icons.count(guild_id)) return;
            requested_icons.insert(guild_id);

            std::string url = "https://cdn.discordapp.com/icons/" + guild_id + "/" + icon_hash + ".png?size=64";
            
            std::thread([this, guild_id, url]() {
                CURL* curl = curl_easy_init();
                if (curl) {
                    std::string buffer;
                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
                        ((std::string*)userp)->append((char*)contents, size * nmemb);
                        return size * nmemb;
                    });
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    
                    CURLcode res = curl_easy_perform(curl);
                    if (res == CURLE_OK) {
                        int width, height, channels;
                        unsigned char* data = stbi_load_from_memory((unsigned char*)buffer.data(), (int)buffer.size(), &width, &height, &channels, 4);
                        if (data) {
                            post_task([this, guild_id, data, width, height]() {
                                m_ui->update_icon_texture(guild_id, data, width, height);
                                stbi_image_free(data);
                            });
                        }
                    }
                    curl_easy_cleanup(curl);
                }
            }).detach();
        };
        
        // UI Callbacks
        m_ui->on_channel_selected = [this](const std::string& channel_id) {
            std::string current_cid;
            {
                std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
                m_state.current_channel_id = channel_id;
                m_state.channel_error = ""; // Clear old error
                m_state.reply_msg_id = ""; // Clear old reply
                current_cid = channel_id;
            }

            // Fetch messages for this channel via REST
            m_rest->get_messages(current_cid, [this, current_cid](bool success, const json& data) {
                post_task([this, current_cid, success, data]() {
                    std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
                    if (success) {
                        std::vector<Message> msgs;
                        if (data.is_array()) {
                            for (auto it = data.rbegin(); it != data.rend(); ++it) {
                                msgs.push_back(it->get<Message>());
                            }
                        }
                        m_state.messages[current_cid] = msgs;
                    } else {
                        m_state.channel_error = "No Access";
                    }
                });
            });
        };

        m_ui->on_reply_selected = [this](const std::string& msg_id, const std::string& username, const std::string& content, const std::string& guild_id) {
            std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
            m_state.reply_msg_id = msg_id;
            m_state.reply_username = username;
            m_state.reply_content = content;
            m_state.reply_guild_id = guild_id;
        };

        m_ui->on_guild_selected = [this](const std::string& guild_id) {
            std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
            m_state.current_guild_id = guild_id;
            m_state.current_channel_id = ""; // Reset channel
            m_state.channel_error = "";
            m_state.reply_msg_id = "";
            m_state.reply_guild_id = "";
            
            // Auto select first text channel
            Guild* g = m_state.get_guild(guild_id);
            if (g) {
                for (const auto& c : g->channels) {
                    if (c.type == 0) {
                        m_state.current_channel_id = c.id;
                        std::string cid = c.id;
                        m_rest->get_messages(cid, [this, cid](bool success, const json& data) {
                            post_task([this, cid, success, data]() {
                                std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
                                if (success) {
                                    std::vector<Message> msgs;
                                    if (data.is_array()) {
                                        for (auto it = data.rbegin(); it != data.rend(); ++it) {
                                            msgs.push_back(it->get<Message>());
                                        }
                                    }
                                    m_state.messages[cid] = msgs;
                                } else {
                                    m_state.channel_error = "No Access";
                                }
                            });
                        });
                        break;
                    }
                }
            }
        };

        m_ui->on_send_message = [this](const std::string& content, const std::string& reply_id) {
            std::string cid, gid;
            {
                std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
                cid = m_state.current_channel_id;
                gid = m_state.reply_guild_id;
                // Clear reply state after sending
                m_state.reply_msg_id = "";
                m_state.reply_username = "";
                m_state.reply_content = "";
                m_state.reply_guild_id = "";
            }
            if (!cid.empty()) {
                m_rest->send_message(cid, content, gid, reply_id, [cid](bool s, const json& d){
                    if (!s) {
                        std::cerr << "[App] Failed to send message to " << cid << ". Response: " << d.dump() << std::endl;
                    }
                });
            }
        };

        static std::set<std::string> requested_attachments;
        m_ui->on_load_attachment = [this](const std::string& att_id, const std::string& url) {
            if (requested_attachments.count(att_id)) return;
            requested_attachments.insert(att_id);

            std::thread([this, att_id, url]() {
                CURL* curl = curl_easy_init();
                if (curl) {
                    std::string buffer;
                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
                        ((std::string*)userp)->append((char*)contents, size * nmemb);
                        return size * nmemb;
                    });
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    
                    CURLcode res = curl_easy_perform(curl);
                    if (res == CURLE_OK) {
                        int width, height, channels;
                        unsigned char* data = stbi_load_from_memory((unsigned char*)buffer.data(), (int)buffer.size(), &width, &height, &channels, 4);
                        if (data) {
                            post_task([this, att_id, data, width, height]() {
                                m_ui->update_attachment_texture(att_id, data, width, height);
                                stbi_image_free(data);
                            });
                        }
                    }
                    curl_easy_cleanup(curl);
                }
            }).detach();
        };

        m_gateway = std::make_unique<Gateway>([this](const std::string& event, const json& data) {
            // Gateway callback runs on gateway thread
            // We copy data to ensure lifetime
            json d = data;
            post_task([this, event, d]() {
                handle_event(event, d);
            });
        });

        m_gateway->connect(m_config.token);

        return true;
    }

    void App::run() {
        m_running = true;
        while (m_running && !m_ui->should_close()) {
            process_main_thread_tasks();
            
            m_ui->new_frame();
            
            {
                std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
                m_ui->render(m_state);
            }
        }
    }

    void App::handle_event(const std::string& event, const json& data) {
        std::lock_guard<std::recursive_mutex> lock(m_state_mutex);
        std::cout << "[App] Event received: " << event << std::endl;
        
        try {
                            if (event == "READY") {
                                std::cout << "[App] READY! Connected as " << data["user"]["username"] << std::endl;
                                if (data.contains("guilds")) {
                                    std::cout << "[App] READY contains " << data["guilds"].size() << " guilds. Parsing..." << std::endl;
                                    for (const auto& g_json : data["guilds"]) {
                                        try {
                                            Guild g = g_json.get<Guild>();
                                            m_state.guilds.push_back(g);
                                        } catch (...) {
                                            // Some guilds in READY might be partial/unavailable
                                        }
                                    }
                                    // Build map
                                    m_state.guild_map.clear();
                                    for (auto& existing : m_state.guilds) {
                                        m_state.guild_map[existing.id] = &existing;
                                    }
                                }
                                        } else if (event == "MESSAGE_CREATE") {
                                            try {
                                                Message m = data.get<Message>();
                                                m_state.messages[m.channel_id].push_back(m);
                                                // std::cout << "[App] Real-time message in channel " << m.channel_id << ": " << m.content << std::endl;
                                            } catch (const std::exception& e) {
                                                std::cerr << "[App] Error parsing Message: " << e.what() << std::endl;
                                            }
                                        } else if (event == "GUILD_CREATE") {
                                            try {
                                                Guild g = data.get<Guild>();
                                                // std::cout << "[App] Real-time Guild Joined/Loaded: " << g.name << std::endl;
                                                bool found = false;
                                                for (auto& eg : m_state.guilds) {
                                                    if (eg.id == g.id) {
                                                        eg = g;
                                                        found = true;
                                                        break;
                                                    }
                                                }
                                                if (!found) {
                                                    m_state.guilds.push_back(g);
                                                }
                                                
                                                m_state.guild_map.clear();
                                                for (auto& existing : m_state.guilds) {
                                                    m_state.guild_map[existing.id] = &existing;
                                                }
                                            } catch (const std::exception& e) {
                                                std::cerr << "[App] Error parsing Guild: " << e.what() << std::endl;
                                            }
                                        }
                                    } catch (const std::exception& e) {
            std::cerr << "[App] Error processing event " << event << ": " << e.what() << std::endl;
        }
    }

    void App::load_config(const std::string& path) {
        std::ifstream file(path);
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                if (j.contains("token")) {
                    m_config.token = j["token"];
                    std::cout << "[App] Config loaded. Token starts with: " << m_config.token.substr(0, 5) << "..." << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[App] Error parsing config: " << e.what() << std::endl;
            }
        } else {
            std::cerr << "[App] Config file not found at: " << path << std::endl;
        }
    }

    void App::post_task(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_task_queue.push(task);
    }

    void App::process_main_thread_tasks() {
        std::vector<std::function<void()>> tasks;
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            while (!m_task_queue.empty()) {
                tasks.push_back(m_task_queue.front());
                m_task_queue.pop();
            }
        }

        for (auto& task : tasks) {
            task();
        }
    }
}
