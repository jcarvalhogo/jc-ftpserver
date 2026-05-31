#pragma once

#include "jcftp/config.h"
#include "jcftp/socket.h"

namespace jcftp {

Socket create_control_listener(const ServerConfig& config);
void run_server(const ServerConfig& config);
void log_line(const std::string& message);

} // namespace jcftp
