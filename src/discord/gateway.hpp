#pragma once

#include <nlohmann/json.hpp>

#include <boost/beast/websocket.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <functional>
#include <thread>
#include <atomic>
#include <string>

namespace discord {

using json = nlohmann::json;

using EventCallback =
    std::function<void(const std::string& event_name,
                       const json& data)>;

class Gateway {

public:

    Gateway(EventCallback callback);
    ~Gateway();

    void connect(const std::string& token);
    void close();

private:

    void read_loop();
    void handle_message(const std::string& msg);

    void send_json(const json& j);

    void start_heartbeat(int interval_ms);

private:

    EventCallback m_callback;

    boost::asio::io_context m_ioc;

    boost::asio::ssl::context m_ssl_ctx;

    boost::asio::ip::tcp::resolver m_resolver;

    boost::beast::websocket::stream<
        boost::beast::ssl_stream<
            boost::beast::tcp_stream
        >
    > m_ws;

    std::atomic<bool> m_connected{false};

    std::string m_token;

    std::atomic<int> m_last_sequence{0};

    std::atomic<bool> m_heartbeat_active{false};

    int m_heartbeat_interval{45000};

    std::thread m_heartbeat_thread;

    std::mutex m_write_mutex;
};

}