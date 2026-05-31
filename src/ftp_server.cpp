#include "jcftp/ftp_server.h"

#include "jcftp/ftp_session.h"

#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace jcftp {

namespace {

std::mutex log_mutex;

} // namespace

void log_line(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << message << std::endl;
}

Socket create_control_listener(const ServerConfig& config) {
    auto listener = make_tcp_socket();
    auto addr = make_address(config.bind_address, config.port);
    if (::bind(listener.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("bind() falhou: " + socket_error_message());
    }
    if (::listen(listener.get(), config.backlog) < 0) {
        throw std::runtime_error("listen() falhou: " + socket_error_message());
    }
    return listener;
}

void run_server(const ServerConfig& config) {
    auto listener = create_control_listener(config);

    log_line("Servidor FTP iniciado em " + config.bind_address + ":" + std::to_string(config.port));
    log_line("Raiz FTP: " + config.root_dir.string());
    for (const auto& [alias, root_dir] : config.root_aliases) {
        log_line("Raiz FTP @" + alias + ": " + root_dir.string());
    }

    while (true) {
        sockaddr_in client{};
        SocketAddressLength length = sizeof(client);
        SocketHandle client_fd = ::accept(listener.get(), reinterpret_cast<sockaddr*>(&client), &length);
        if (!is_valid_socket(client_fd)) {
            log_line("accept() falhou: " + socket_error_message());
            continue;
        }

        std::thread([client_fd, config, client]() mutable {
            FtpSession(Socket(client_fd), config, client).run();
        }).detach();
    }
}

} // namespace jcftp
