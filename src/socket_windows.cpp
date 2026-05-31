#include "jcftp/socket.h"

#include <windows.h>

#include <climits>
#include <stdexcept>

namespace jcftp {

namespace {

constexpr SocketHandle invalid_socket = INVALID_SOCKET;

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
        ::closesocket(fd_);
        fd_ = invalid_socket;
    }
}

void initialize_network() {
    WSADATA data{};
    int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        throw std::runtime_error("WSAStartup() falhou: " + std::to_string(result));
    }
}

void cleanup_network() {
    WSACleanup();
}

bool is_valid_socket(SocketHandle fd) {
    return fd != INVALID_SOCKET;
}

std::string socket_error_message() {
    int code = WSAGetLastError();
    char* message = nullptr;
    DWORD size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                    FORMAT_MESSAGE_IGNORE_INSERTS,
                                nullptr,
                                static_cast<DWORD>(code),
                                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                reinterpret_cast<LPSTR>(&message),
                                0,
                                nullptr);

    std::string text = size > 0 && message != nullptr ? std::string(message, size) : "erro desconhecido";
    if (message != nullptr) {
        LocalFree(message);
    }
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) {
        text.pop_back();
    }
    return std::to_string(code) + " (" + text + ")";
}

Socket make_tcp_socket() {
    Socket socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!socket.valid()) {
        throw std::runtime_error("socket() falhou: " + socket_error_message());
    }

    BOOL enabled = TRUE;
    if (setsockopt(socket.get(),
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   reinterpret_cast<const char*>(&enabled),
                   sizeof(enabled)) == SOCKET_ERROR) {
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
    inet_ntop(AF_INET, const_cast<in_addr*>(&address.sin_addr), buffer, sizeof(buffer));
    return buffer;
}

bool send_all(SocketHandle fd, const char* data, std::size_t size) {
    const char* current = data;
    std::size_t remaining = size;
    while (remaining > 0) {
        int chunk = remaining > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(remaining);
        int sent = ::send(fd, current, chunk, 0);
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
    int chunk = size > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(size);
    return ::recv(fd, buffer, chunk, 0);
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
