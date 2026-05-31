#include "jcftp/config.h"

#include "jcftp/string_utils.h"

#include <fstream>
#include <map>
#include <stdexcept>

namespace jcftp {

namespace {

std::map<std::string, std::string> read_key_values(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Nao foi possivel abrir o arquivo de configuracao: " + path.string());
    }

    std::map<std::string, std::string> values;
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        auto comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw std::runtime_error("Linha invalida no arquivo de configuracao (" +
                                     std::to_string(line_number) + "): " + line);
        }

        auto key = trim(line.substr(0, separator));
        auto value = trim(line.substr(separator + 1));
        if (key.empty()) {
            throw std::runtime_error("Chave vazia no arquivo de configuracao, linha " +
                                     std::to_string(line_number));
        }

        values[key] = value;
    }

    return values;
}

int read_int(const std::map<std::string, std::string>& values, const std::string& key, int fallback) {
    auto it = values.find(key);
    if (it == values.end()) {
        return fallback;
    }

    try {
        return std::stoi(it->second);
    } catch (const std::exception&) {
        throw std::runtime_error("Valor inteiro invalido para '" + key + "': " + it->second);
    }
}

std::string read_string(const std::map<std::string, std::string>& values,
                        const std::string& key,
                        const std::string& fallback) {
    auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

fs::path prepare_root_dir(const fs::path& root_dir) {
    auto prepared = fs::absolute(root_dir).lexically_normal();
    fs::create_directories(prepared);
    return fs::canonical(prepared);
}

} // namespace

ServerConfig load_config(const fs::path& path) {
    ServerConfig config;
    auto values = read_key_values(path);

    config.bind_address = read_string(values, "bind_address", config.bind_address);
    config.port = read_int(values, "port", config.port);
    config.username = read_string(values, "username", config.username);
    config.password = read_string(values, "password", config.password);
    config.root_dir = read_string(values, "root_dir", config.root_dir.string());
    config.passive_port_min = read_int(values, "passive_port_min", config.passive_port_min);
    config.passive_port_max = read_int(values, "passive_port_max", config.passive_port_max);
    config.backlog = read_int(values, "backlog", config.backlog);

    const std::string alias_prefix = "root_dir.";
    for (const auto& [key, value] : values) {
        if (key.rfind(alias_prefix, 0) != 0) {
            continue;
        }

        auto alias = key.substr(alias_prefix.size());
        if (alias.empty()) {
            throw std::runtime_error("Alias vazio para diretorio raiz: " + key);
        }
        if (value.empty()) {
            throw std::runtime_error("Diretorio raiz vazio para alias '" + alias + "'");
        }

        config.root_aliases[alias] = prepare_root_dir(value);
    }

    if (config.port < 1 || config.port > 65535) {
        throw std::runtime_error("A porta de controle deve estar entre 1 e 65535");
    }
    if (config.passive_port_min < 1 || config.passive_port_max > 65535 ||
        config.passive_port_min > config.passive_port_max) {
        throw std::runtime_error("Intervalo de portas passivas invalido");
    }
    if (config.username.empty() || config.password.empty()) {
        throw std::runtime_error("Usuario e senha nao podem ficar vazios");
    }

    config.root_dir = prepare_root_dir(config.root_dir);
    return config;
}

} // namespace jcftp
