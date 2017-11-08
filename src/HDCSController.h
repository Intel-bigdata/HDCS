// Copyright [2017] <Intel>
#ifndef HDCS_CONTROLLER_H
#define HDCS_CONTROLLER_H

#include "common/Config.h"
#include "Network/hdcs_networking.h"
#include "core/HDCSCore.h"
#include "common/HDCS_REQUEST_CTX.h"

namespace hdcs {
  class HDCSController {
  public:
    HDCSController(std::string name, std::string config_name);
    ~HDCSController();
    void handle_request(void* session_id, std::string msg_content);
  private:
    Config *config;
    std::string config_name;
    networking::server *network_service;
    std::map<std::string, core::HDCSCore*> hdcs_core_map;
    std::mutex hdcs_core_map_mutex;
  };
}// hdcs

#endif
