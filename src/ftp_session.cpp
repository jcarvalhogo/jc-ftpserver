#include "jcftp/ftp_session.h"

#include "jcftp/ftp_server.h"
#include "jcftp/string_utils.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace jcftp {

namespace {

std::string format_time(const fs::file_time_type& file_time) {
    auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t time = std::chrono::system_clock::to_time_t(system_time);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream output;
    output << std::put_time(&tm, "%b %d %H:%M");
    return output.str();
}

} // namespace

std::string unix_path(fs::path path) {
    auto value = path.generic_string();
    return value.empty() ? "/" : value;
}

FtpSession::FtpSession(Socket control, ServerConfig config, sockaddr_in client_address)
    : control_(std::move(control)),
      config_(std::move(config)),
      client_ip_(sockaddr_to_ip(client_address)),
      session_root_dir_(config_.root_dir) {}

void FtpSession::run() {
    log_line("Cliente conectado: " + client_ip_);
    reply(220, "jc-ftpserver pronto");

    std::string line;
    while (recv_line(control_.get(), line)) {
        if (!handle_command(line)) {
            break;
        }
    }

    passive_listener_.close();
    log_line("Cliente desconectado: " + client_ip_);
}

bool FtpSession::reply(int code, const std::string& message) {
    return send_all(control_.get(), std::to_string(code) + " " + message + "\r\n");
}

bool FtpSession::handle_command(const std::string& line) {
    std::string command;
    std::string argument;
    auto space = line.find(' ');
    if (space == std::string::npos) {
        command = upper_copy(line);
    } else {
        command = upper_copy(line.substr(0, space));
        argument = trim(line.substr(space + 1));
    }

    if (command == "USER") {
        requested_root_alias_.clear();
        auto separator = argument.find('@');
        auto username = separator == std::string::npos ? argument : argument.substr(0, separator);
        if (separator != std::string::npos) {
            requested_root_alias_ = argument.substr(separator + 1);
        }

        user_ok_ = username == config_.username &&
                   (requested_root_alias_.empty() ||
                    config_.root_aliases.find(requested_root_alias_) != config_.root_aliases.end());
        logged_in_ = false;
        reply(331, "Senha requerida");
        return true;
    }
    if (command == "PASS") {
        logged_in_ = user_ok_ && argument == config_.password;
        if (logged_in_) {
            auto alias = config_.root_aliases.find(requested_root_alias_);
            session_root_dir_ = alias == config_.root_aliases.end() ? config_.root_dir : alias->second;
            cwd_ = "/";
        }
        reply(logged_in_ ? 230 : 530, logged_in_ ? "Login efetuado" : "Login invalido");
        return true;
    }
    if (command == "QUIT") {
        reply(221, "Ate logo");
        return false;
    }
    if (command == "NOOP") {
        reply(200, "OK");
        return true;
    }
    if (command == "SYST") {
        reply(215, "UNIX Type: L8");
        return true;
    }
    if (command == "FEAT") {
        send_all(control_.get(), "211-Recursos\r\n EPSV\r\n PASV\r\n UTF8\r\n211 Fim\r\n");
        return true;
    }
    if (command == "OPTS") {
        reply(200, "Opcao aceita");
        return true;
    }

    if (!logged_in_) {
        reply(530, "Faca login primeiro");
        return true;
    }

    if (command == "PWD" || command == "XPWD") {
        reply(257, "\"" + unix_path(cwd_) + "\"");
    } else if (command == "CWD") {
        change_directory(argument);
    } else if (command == "CDUP") {
        change_directory("..");
    } else if (command == "TYPE") {
        reply(200, "Tipo definido para " + argument);
    } else if (command == "PASV") {
        enter_passive_mode(false);
    } else if (command == "EPSV") {
        enter_passive_mode(true);
    } else if (command == "LIST" || command == "NLST") {
        list_directory(command == "NLST", argument);
    } else if (command == "RETR") {
        retrieve_file(argument);
    } else if (command == "STOR") {
        store_file(argument);
    } else if (command == "DELE") {
        delete_file(argument);
    } else if (command == "MKD" || command == "XMKD") {
        make_directory(argument);
    } else if (command == "RMD" || command == "XRMD") {
        remove_directory(argument);
    } else {
        reply(502, "Comando nao implementado");
    }

    return true;
}

std::optional<fs::path> FtpSession::resolve_path(const std::string& ftp_path, bool must_exist) {
    fs::path requested = ftp_path.empty() ? cwd_ : fs::path(ftp_path);
    fs::path normalized = requested.is_absolute() ? requested.lexically_normal()
                                                   : (cwd_ / requested).lexically_normal();
    fs::path relative = normalized.lexically_relative("/");
    fs::path full = (session_root_dir_ / relative).lexically_normal();

    std::error_code error;
    fs::path check_path = must_exist ? fs::canonical(full, error) : fs::weakly_canonical(full.parent_path(), error);
    if (error) {
        return std::nullopt;
    }

    if (!must_exist) {
        check_path /= full.filename();
        check_path = check_path.lexically_normal();
    }

    auto relative_to_root = fs::relative(check_path, session_root_dir_, error);
    if (error || (!relative_to_root.empty() && *relative_to_root.begin() == "..")) {
        return std::nullopt;
    }

    return check_path;
}

fs::path FtpSession::to_ftp_path(const fs::path& full_path) const {
    auto relative = fs::relative(full_path, session_root_dir_);
    if (relative.empty() || relative == ".") {
        return "/";
    }
    return "/" / relative;
}

void FtpSession::change_directory(const std::string& path) {
    auto full = resolve_path(path, true);
    if (!full || !fs::is_directory(*full)) {
        reply(550, "Diretorio nao encontrado");
        return;
    }

    cwd_ = to_ftp_path(*full);
    reply(250, "Diretorio alterado para " + unix_path(cwd_));
}

void FtpSession::enter_passive_mode(bool extended) {
    passive_listener_.close();

    for (int port = config_.passive_port_min; port <= config_.passive_port_max; ++port) {
        try {
            auto listener = make_tcp_socket();
            auto addr = make_address(config_.bind_address, port);
            if (::bind(listener.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 &&
                ::listen(listener.get(), 1) == 0) {
                passive_listener_ = std::move(listener);

                if (extended) {
                    reply(229, "Entering Extended Passive Mode (|||" + std::to_string(port) + "|)");
                } else {
                    std::string ip = config_.bind_address == "0.0.0.0" ? local_control_ip() : config_.bind_address;
                    std::replace(ip.begin(), ip.end(), '.', ',');
                    int p1 = port / 256;
                    int p2 = port % 256;
                    reply(227, "Entering Passive Mode (" + ip + "," + std::to_string(p1) + "," +
                               std::to_string(p2) + ")");
                }
                return;
            }
        } catch (const std::exception&) {
            continue;
        }
    }

    reply(425, "Nao foi possivel abrir uma porta passiva");
}

std::string FtpSession::local_control_ip() const {
    sockaddr_in addr{};
    SocketAddressLength length = sizeof(addr);
    if (getsockname(control_.get(), reinterpret_cast<sockaddr*>(&addr), &length) == 0) {
        auto ip = sockaddr_to_ip(addr);
        if (ip != "0.0.0.0") {
            return ip;
        }
    }
    return "127.0.0.1";
}

std::optional<Socket> FtpSession::accept_data_connection() {
    if (!passive_listener_.valid()) {
        reply(425, "Use PASV primeiro");
        return std::nullopt;
    }

    sockaddr_in client{};
    SocketAddressLength length = sizeof(client);
    SocketHandle fd = ::accept(passive_listener_.get(), reinterpret_cast<sockaddr*>(&client), &length);
    passive_listener_.close();
    if (!is_valid_socket(fd)) {
        reply(425, "Falha ao aceitar conexao de dados");
        return std::nullopt;
    }

    return Socket(fd);
}

void FtpSession::list_directory(bool names_only, const std::string& path) {
    auto full = resolve_path(path, true);
    if (!full || !fs::is_directory(*full)) {
        reply(550, "Diretorio nao encontrado");
        return;
    }

    reply(150, "Abrindo conexao de dados para listagem");
    auto data = accept_data_connection();
    if (!data) {
        return;
    }

    std::ostringstream listing;
    for (const auto& entry : fs::directory_iterator(*full)) {
        if (names_only) {
            listing << entry.path().filename().string() << "\r\n";
            continue;
        }

        auto status = entry.symlink_status();
        auto perms = status.permissions();
        bool dir = fs::is_directory(status);
        listing << (dir ? 'd' : '-')
                << ((perms & fs::perms::owner_read) != fs::perms::none ? 'r' : '-')
                << ((perms & fs::perms::owner_write) != fs::perms::none ? 'w' : '-')
                << ((perms & fs::perms::owner_exec) != fs::perms::none ? 'x' : '-')
                << ((perms & fs::perms::group_read) != fs::perms::none ? 'r' : '-')
                << ((perms & fs::perms::group_write) != fs::perms::none ? 'w' : '-')
                << ((perms & fs::perms::group_exec) != fs::perms::none ? 'x' : '-')
                << ((perms & fs::perms::others_read) != fs::perms::none ? 'r' : '-')
                << ((perms & fs::perms::others_write) != fs::perms::none ? 'w' : '-')
                << ((perms & fs::perms::others_exec) != fs::perms::none ? 'x' : '-')
                << " 1 owner group " << std::setw(10)
                << (dir ? 0 : static_cast<long long>(fs::file_size(entry.path())))
                << ' ' << format_time(entry.last_write_time())
                << ' ' << entry.path().filename().string() << "\r\n";
    }

    send_all(data->get(), listing.str());
    reply(226, "Transferencia concluida");
}

void FtpSession::retrieve_file(const std::string& path) {
    auto full = resolve_path(path, true);
    if (!full || !fs::is_regular_file(*full)) {
        reply(550, "Arquivo nao encontrado");
        return;
    }

    std::ifstream input(*full, std::ios::binary);
    if (!input) {
        reply(550, "Nao foi possivel abrir o arquivo");
        return;
    }

    reply(150, "Abrindo conexao de dados para download");
    auto data = accept_data_connection();
    if (!data) {
        return;
    }

    char buffer[64 * 1024];
    while (input) {
        input.read(buffer, sizeof(buffer));
        auto count = input.gcount();
        if (count > 0 && !send_all(data->get(), buffer, static_cast<std::size_t>(count))) {
            reply(426, "Conexao interrompida");
            return;
        }
    }

    reply(226, "Transferencia concluida");
}

void FtpSession::store_file(const std::string& path) {
    if (path.empty()) {
        reply(501, "Informe o nome do arquivo");
        return;
    }

    auto full = resolve_path(path, false);
    if (!full) {
        reply(550, "Caminho invalido");
        return;
    }

    std::ofstream output(*full, std::ios::binary | std::ios::trunc);
    if (!output) {
        reply(550, "Nao foi possivel criar o arquivo");
        return;
    }

    reply(150, "Abrindo conexao de dados para upload");
    auto data = accept_data_connection();
    if (!data) {
        return;
    }

    char buffer[64 * 1024];
    while (true) {
        int received = recv_some(data->get(), buffer, sizeof(buffer));
        if (received < 0) {
            reply(426, "Conexao interrompida");
            return;
        }
        if (received == 0) {
            break;
        }
        output.write(buffer, received);
    }

    reply(226, "Transferencia concluida");
}

void FtpSession::delete_file(const std::string& path) {
    auto full = resolve_path(path, true);
    if (!full || !fs::is_regular_file(*full)) {
        reply(550, "Arquivo nao encontrado");
        return;
    }

    fs::remove(*full);
    reply(250, "Arquivo removido");
}

void FtpSession::make_directory(const std::string& path) {
    auto full = resolve_path(path, false);
    if (!full) {
        reply(550, "Caminho invalido");
        return;
    }

    if (fs::create_directories(*full)) {
        reply(257, "\"" + unix_path(to_ftp_path(*full)) + "\" criado");
    } else {
        reply(550, "Nao foi possivel criar o diretorio");
    }
}

void FtpSession::remove_directory(const std::string& path) {
    auto full = resolve_path(path, true);
    if (!full || !fs::is_directory(*full)) {
        reply(550, "Diretorio nao encontrado");
        return;
    }

    if (*full == session_root_dir_) {
        reply(550, "Nao e permitido remover a raiz");
        return;
    }

    fs::remove(*full);
    reply(250, "Diretorio removido");
}

} // namespace jcftp
