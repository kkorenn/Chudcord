#include "gateway.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <iostream>
#include <thread>
#include <chrono>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace discord {

Gateway::Gateway(EventCallback callback)
    : m_callback(std::move(callback)),
      m_ssl_ctx(ssl::context::tlsv12_client),
      m_resolver(m_ioc),
      m_ws(m_ioc, m_ssl_ctx)
{
    m_ssl_ctx.set_default_verify_paths();
}

Gateway::~Gateway() {
    close();
}

void Gateway::connect(const std::string& token) {
    m_token = token;

    std::thread([this]() {
        try {
            auto const host = "gateway.discord.gg";
            auto const port = "443";
            auto const target = "/?v=10&encoding=json";

            auto const results = m_resolver.resolve(host, port);

            beast::get_lowest_layer(m_ws).connect(results);

            if(!SSL_set_tlsext_host_name(m_ws.next_layer().native_handle(), host)) {
                throw beast::system_error(
                    beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()),
                    "Failed to set SNI Hostname");
            }

            m_ws.next_layer().handshake(ssl::stream_base::client);
            m_ws.handshake(host, target);

            m_connected = true;
            std::cout << "[Gateway] Handshake complete and connected.\n";

            read_loop();

        } catch (const std::exception& e) {
            std::cerr << "[Gateway] Connect error: " << e.what() << "\n";
            m_connected = false;
        }
    }).detach();
}

void Gateway::close() {
    bool was_connected = m_connected.exchange(false);
    m_heartbeat_active = false;

    if (was_connected) {
        try {
            std::lock_guard<std::mutex> lock(m_write_mutex);
            m_ws.close(websocket::close_code::normal);
        } catch (...) {}
    }
}

void Gateway::read_loop() {
    beast::flat_buffer buffer;

    while (m_connected) {
        try {
            m_ws.read(buffer);

            std::string message =
                beast::buffers_to_string(buffer.data());

            buffer.consume(buffer.size());

            handle_message(message);

        } catch (const std::exception& e) {
            if (m_connected) {
                std::cerr << "[Gateway] Read error: " << e.what() << "\n";
            }
            m_connected = false;
        }
    }
}

void Gateway::handle_message(const std::string& msg) {
    try {
        json payload = json::parse(msg);

        if (!payload.contains("op")) return;
        int op = payload["op"];

        if (payload.contains("s") && !payload["s"].is_null()) {
            m_last_sequence = payload["s"];
        }

        switch (op) {

        case 10: { // HELLO
            int interval = payload["d"]["heartbeat_interval"];
            std::cout << "[Gateway] Hello! Heartbeat interval: " << interval << "ms" << std::endl;
            start_heartbeat(interval);

            json identify = {
                {"op", 2},
                {"d", {
                    {"token", m_token},
                    {"intents", 33280},
                    {"properties", {
                        {"$os", "macos"},
                        {"$browser", "chudcord"},
                        {"$device", "chudcord"}
                    }}
                }}
            };

            send_json(identify);
            break;
        }

        case 11: // HEARTBEAT_ACK
            // std::cout << "[Gateway] Heartbeat ACK" << std::endl;
            break;

        case 0: { // DISPATCH
            if (payload.contains("t") && payload.contains("d")) {
                std::string event = payload["t"];
                // std::cout << "[Gateway] Dispatch: " << event << std::endl;
                m_callback(event, payload["d"]);
            }
            break;
        }

        default:
            break;
        }

    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
    }
}

void Gateway::send_json(const json& j) {
    if (!m_connected) return;
    try {
        std::lock_guard<std::mutex> lock(m_write_mutex);
        m_ws.write(net::buffer(j.dump()));
    } catch (const std::exception& e) {
        if (m_connected) {
            std::cerr << "[Gateway] Send error: " << e.what() << "\n";
        }
    }
}

void Gateway::start_heartbeat(int interval_ms) {
    m_heartbeat_interval = interval_ms;
    m_heartbeat_active = true;

    m_heartbeat_thread = std::thread([this]() {
        while (m_heartbeat_active) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(m_heartbeat_interval));

            if (!m_heartbeat_active || !m_connected) break;

            int seq = m_last_sequence.load();
            json payload = {
                {"op", 1},
                {"d", (seq == 0) ? nullptr : json(seq)}
            };

            send_json(payload);
        }
    });

    m_heartbeat_thread.detach();
}

}
