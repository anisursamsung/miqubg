#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>

namespace miqubg {

class IPC {
public:
    static std::string get_socket_path();

    // Client: returns true if connected and successfully sent command
    static bool send_command(const std::string& command, std::string& response);
};

class IPCServer {
public:
    using CommandHandler = std::function<std::string(const std::string& command)>;

    explicit IPCServer(CommandHandler handler);
    ~IPCServer();

    bool start();
    void stop();

private:
    void listen_loop();

    CommandHandler m_handler;
    std::string m_socket_path;
    int m_server_fd = -1;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
};

} // namespace miqubg
