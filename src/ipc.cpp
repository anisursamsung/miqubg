#include "ipc.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <vector>
#include <poll.h>

namespace miqubg {

std::string IPC::get_socket_path() {
    const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
    std::string display_name = (wayland_display && *wayland_display) ? wayland_display : "wayland-0";

    const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
    if (runtime_dir && *runtime_dir) {
        return std::string(runtime_dir) + "/miqubg-" + display_name + ".sock";
    }

    return "/tmp/miqubg-" + std::to_string(getuid()) + "-" + display_name + ".sock";
}

bool IPC::send_command(const std::string& command, std::string& response) {
    std::string sock_path = get_socket_path();

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return false;
    }

    std::string msg = command;
    if (msg.empty() || msg.back() != '\n') {
        msg += '\n';
    }

    ssize_t sent = write(fd, msg.c_str(), msg.size());
    if (sent < 0) {
        close(fd);
        return false;
    }

    char buf[1024];
    std::memset(buf, 0, sizeof(buf));
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        response = std::string(buf, n);
        while (!response.empty() && (response.back() == '\n' || response.back() == '\r')) {
            response.pop_back();
        }
    }

    close(fd);
    return true;
}

IPCServer::IPCServer(CommandHandler handler)
    : m_handler(std::move(handler))
    , m_socket_path(IPC::get_socket_path())
{
}

IPCServer::~IPCServer() {
    stop();
}

bool IPCServer::start() {
    if (m_running) return true;

    // Check if another instance is already actively listening
    std::string test_resp;
    if (IPC::send_command("PING", test_resp)) {
        std::cerr << "[miqubg] Another daemon is already running on " << m_socket_path << std::endl;
        return false;
    }

    // Unlink any stale socket file
    unlink(m_socket_path.c_str());

    m_server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (m_server_fd < 0) {
        std::cerr << "[miqubg] Failed to create IPC socket: " << strerror(errno) << std::endl;
        return false;
    }

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(m_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[miqubg] Failed to bind IPC socket to " << m_socket_path << ": " << strerror(errno) << std::endl;
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    if (listen(m_server_fd, 16) < 0) {
        std::cerr << "[miqubg] Failed to listen on IPC socket: " << strerror(errno) << std::endl;
        close(m_server_fd);
        m_server_fd = -1;
        unlink(m_socket_path.c_str());
        return false;
    }

    m_running = true;
    m_thread = std::thread(&IPCServer::listen_loop, this);
    return true;
}

void IPCServer::stop() {
    if (!m_running) return;
    m_running = false;

    if (m_server_fd >= 0) {
        shutdown(m_server_fd, SHUT_RDWR);
        close(m_server_fd);
        m_server_fd = -1;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    unlink(m_socket_path.c_str());
}

void IPCServer::listen_loop() {
    while (m_running) {
        struct pollfd pfd;
        pfd.fd = m_server_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = poll(&pfd, 1, 500);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0 || !(pfd.revents & POLLIN)) {
            continue;
        }

        int client_fd = accept4(m_server_fd, nullptr, nullptr, SOCK_CLOEXEC);
        if (client_fd < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            break;
        }

        char buf[2048];
        std::memset(buf, 0, sizeof(buf));
        ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            std::string cmd(buf, n);
            while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) {
                cmd.pop_back();
            }

            std::string resp = "OK";
            if (cmd == "PING") {
                resp = "PONG";
            } else if (m_handler) {
                resp = m_handler(cmd);
            }

            if (resp.empty() || resp.back() != '\n') {
                resp += '\n';
            }
            write(client_fd, resp.c_str(), resp.size());
        }

        close(client_fd);
    }
}

} // namespace miqubg
