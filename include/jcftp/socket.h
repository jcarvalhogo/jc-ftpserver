#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <cstddef>
#include <cstdint>
#include <string>

namespace jcftp {

#ifdef _WIN32
using SocketHandle = SOCKET;
using SocketAddressLength = int;
#else
using SocketHandle = int;
using SocketAddressLength = socklen_t;
#endif

class Socket {
public:
    Socket() = default;
    explicit Socket(SocketHandle fd);
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    ~Socket();

    SocketHandle get() const;
    bool valid() const;
    void close();

private:
#ifdef _WIN32
    SocketHandle fd_ = INVALID_SOCKET;
#else
    SocketHandle fd_ = -1;
#endif
};

void initialize_network();
void cleanup_network();
bool is_valid_socket(SocketHandle fd);
std::string socket_error_message();
Socket make_tcp_socket();
sockaddr_in make_address(const std::string& address, int port);
std::string sockaddr_to_ip(const sockaddr_in& address);
bool send_all(SocketHandle fd, const char* data, std::size_t size);
bool send_all(SocketHandle fd, const std::string& data);
int recv_some(SocketHandle fd, char* buffer, std::size_t size);
bool recv_line(SocketHandle fd, std::string& line);

} // namespace jcftp
