#include "jcftp/socket.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace jcftp {

namespace {

constexpr SocketHandle invalid_socket = -1;

} // namespace

Socket::Socket(SocketHandle fd) : fd_(fd) {}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = invalid_socket;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = invalid_socket;
    }
    return *this;
}

Socket::~Socket() {
    close();
}

SocketHandle Socket::get() const {
    return fd_;
}

bool Socket::valid() const {
    return is_valid_socket(fd_);
}

void Socket::close() {
    if (valid()) {
        ::close(fd_);
        fd_ = invalid_socket;
    }
}

void initialize_network() {}

void cleanup_network() {}

bool is_valid_socket(SocketHandle fd) {
    return fd >= 0;
}

std::string socket_error_message() {
    return std::strerror(errno);
}

Socket make_tcp_socket() {
    Socket socket(::socket(AF_INET, SOCK_STREAM, 0));
    if (!socket.valid()) {
        throw std::runtime_error("socket() falhou: " + socket_error_message());
    }

    int enabled = 1;
    if (setsockopt(socket.get(), SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
        throw std::runtime_error("setsockopt(SO_REUSEADDR) falhou: " + socket_error_message());
    }

    return socket;
}

sockaddr_in make_address(const std::string& address, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("Endereco IPv4 invalido: " + address);
    }
    return addr;
}

std::string sockaddr_to_ip(const sockaddr_in& address) {
    char buffer[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &address.sin_addr, buffer, sizeof(buffer));
    return buffer;
}

bool send_all(SocketHandle fd, const char* data, std::size_t size) {
    const char* current = data;
    std::size_t remaining = size;
    while (remaining > 0) {
        ssize_t sent = ::send(fd, current, remaining, MSG_NOSIGNAL);
        if (sent <= 0) {
            return false;
        }
        current += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
    return true;
}

bool send_all(SocketHandle fd, const std::string& data) {
    return send_all(fd, data.data(), data.size());
}

int recv_some(SocketHandle fd, char* buffer, std::size_t size) {
    return static_cast<int>(::recv(fd, buffer, size, 0));
}

bool recv_line(SocketHandle fd, std::string& line) {
    line.clear();
    char ch = 0;
    while (true) {
        int received = recv_some(fd, &ch, 1);
        if (received <= 0) {
            return false;
        }
        if (ch == '\n') {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return true;
        }
        line.push_back(ch);
        if (line.size() > 8192) {
            return false;
        }
    }
}

} // namespace jcftp
