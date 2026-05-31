#include "jcftp/config.h"
#include "jcftp/ftp_server.h"
#include "jcftp/socket.h"

#ifndef _WIN32
#include <csignal>
#endif
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

const char* default_config_path() {
#ifdef _WIN32
    return "ftpserver.windows.conf";
#else
    return "ftpserver.conf";
#endif
}

void print_help(const char* program_name) {
    std::cout << "Uso: " << program_name << " [arquivo.conf]\n"
              << "Versao: " << JC_FTPSERVER_VERSION << "\n"
              << "Padrao: " << default_config_path() << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif

    try {
        if (argc > 1) {
            std::string argument = argv[1];
            if (argument == "-h" || argument == "--help") {
                print_help(argv[0]);
                return 0;
            }
            if (argument == "-v" || argument == "--version") {
                std::cout << JC_FTPSERVER_VERSION << '\n';
                return 0;
            }
        }

        fs::path config_path = argc > 1 ? argv[1] : default_config_path();
        auto config = jcftp::load_config(config_path);
        jcftp::initialize_network();

        jcftp::log_line("Configuracao: " + fs::absolute(config_path).string());
        jcftp::run_server(config);
        jcftp::cleanup_network();
    } catch (const std::exception& error) {
        jcftp::cleanup_network();
        std::cerr << "Erro: " << error.what() << std::endl;
        return 1;
    }
}
