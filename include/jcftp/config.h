#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace jcftp {

namespace fs = std::filesystem;

struct ServerConfig {
    std::string bind_address = "0.0.0.0";
    int port = 2121;
    std::string username = "admin";
    std::string password = "admin";
#ifdef _WIN32
    fs::path root_dir = "ftp-files";
#else
    fs::path root_dir = "/home/jcarvalho/Documentos/ftp-files";
#endif
    std::map<std::string, fs::path> root_aliases;
    int passive_port_min = 40000;
    int passive_port_max = 40100;
    int backlog = 16;
};

ServerConfig load_config(const fs::path& path);

} // namespace jcftp
