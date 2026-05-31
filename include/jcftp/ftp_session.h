#pragma once

#include "jcftp/config.h"
#include "jcftp/socket.h"

#include <filesystem>
#include <netinet/in.h>
#include <optional>
#include <string>

namespace jcftp {

class FtpSession {
public:
    FtpSession(Socket control, ServerConfig config, sockaddr_in client_address);
    void run();

private:
    bool reply(int code, const std::string& message);
    bool handle_command(const std::string& line);
    std::optional<fs::path> resolve_path(const std::string& ftp_path, bool must_exist);
    fs::path to_ftp_path(const fs::path& full_path) const;
    void change_directory(const std::string& path);
    void enter_passive_mode(bool extended);
    std::string local_control_ip() const;
    std::optional<Socket> accept_data_connection();
    void list_directory(bool names_only, const std::string& path);
    void retrieve_file(const std::string& path);
    void store_file(const std::string& path);
    void delete_file(const std::string& path);
    void make_directory(const std::string& path);
    void remove_directory(const std::string& path);

    Socket control_;
    ServerConfig config_;
    std::string client_ip_;
    std::string requested_root_alias_;
    fs::path session_root_dir_;
    bool user_ok_ = false;
    bool logged_in_ = false;
    fs::path cwd_ = "/";
    Socket passive_listener_;
};

std::string unix_path(fs::path path);

} // namespace jcftp
