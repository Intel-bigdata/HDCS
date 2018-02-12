// Copyright [2017] <Intel>
#ifndef HDCS_CONTROLLER_H
#define HDCS_CONTROLLER_H

//#include "common/Config.h"
#include "core/HDCSCore.h"
#include "Network/hdcs_networking.h"
#include "common/HDCS_REQUEST_CTX.h"
#include "ha/HAClient.h"

namespace hdcs {

  class HDCSController {
  public:
    HDCSController(std::string name, std::string config_file_path, std::string role = "hdcs_master");
    ~HDCSController();
    void handle_request(void* session_id, std::string msg_content);
  private:
    Config config;
    //std::map<std::string, std::string> conf_of_HDCSController;
    ConfigInfo conf_of_HDCSController;
    std::string name;
    std::string config_file_path;
    networking::server *network_service;
    std::map<std::string, core::HDCSCore*> hdcs_core_map;
    std::mutex hdcs_core_map_mutex;
    ha::HAClient *ha_client;
    std::string role;
  };
}// hdcs

#endif
