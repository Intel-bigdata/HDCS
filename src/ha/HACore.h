#ifndef HDCS_HA_CORE_H
#define HDCS_HA_CORE_H

#include "common/AioCompletionImp.h"
#include "Network/hdcs_networking.h"
#include "common/HeartBeat/HeartBeatService.h"

#include "ha/HDCSCoreStatController.h"
#include "ha/HAConfig.h"
#include "ha/HACmdHandler.h"

#include <iostream>

namespace hdcs {
typedef std::map<std::string, networking::Connection*> name_to_conn_map_t;
typedef uint8_t HDCS_HA_MSG_TYPE;
#define HDCS_MSG_HEARTBEAT  0X31
#define HDCS_MSG_CORE_STAT  0X32
#define HDCS_MSG_DOMAIN_MAP 0X33
#define HDCS_MSG_CMD        0X34
#define HDCS_MSG_HA_CONN    0x35

struct HDCS_MSG_HA_CONN_TYPE {
  HDCS_HA_MSG_TYPE reserved_flag;
  char node_name[32];
};

class HDCS_HA_CONN_MSG {
public:
  HDCS_HA_CONN_MSG (std::string name) {
    memset(&data_, 0, sizeof(HDCS_MSG_HA_CONN_TYPE));
    data_.reserved_flag = HDCS_MSG_HA_CONN;
    memcpy(data_.node_name, name.c_str(), name.size());
  }

  ~HDCS_HA_CONN_MSG () {}

  char* data () {
    return (char*)(&data_);
  }

  uint64_t size () {
    return sizeof(HDCS_MSG_HA_CONN_TYPE);
  }

  std::string get_name () {
    return std::string(data_.node_name);
  }
private:
  HDCS_MSG_HA_CONN_TYPE data_;
};

namespace ha {

class HACore {
public:
  HACore (std::string name, HAConfig ha_config) :
    name(name),
    ha_config(ha_config),
    listener([&](void* p, std::string s){request_handler(p, s);},
             "0.0.0.0", ha_config.get_host_port(name), 1, 1) {
    listener.start();
    listener.async_run();
  }

  ~HACore () {
    for (auto &it : conn_map) {
      it.second->close();
    }
    listener.stop();
  }

  //virtual void apply_domain_map_item (HDCS_DOMAIN_MAP_ITEM domain_map_item) = 0;
  virtual void handle_heartbeat_request (void* session_arg, std::string msg_content) = 0;
  virtual void handle_core_stat_request (void* session_arg, std::string msg_content) = 0;
  virtual void handle_mgr_request (void* session_arg, std::string msg_content) = 0;
  virtual void handle_domain_map_request (void* session_arg, std::string msg_content) = 0;
  virtual void handle_ha_conn_request (void* session_arg, std::string msg_content) = 0;

  void request_handler (void* session_arg, std::string msg_content) {
    // use different msg handler to handle
    HDCS_HA_MSG_TYPE type = *((HDCS_HA_MSG_TYPE*)msg_content.c_str());
    switch (type) {
      // 1. HeartBeat
      case HDCS_MSG_HEARTBEAT:
        handle_heartbeat_request(session_arg, msg_content);
        break;    
      // 2. domain map
      case HDCS_MSG_DOMAIN_MAP:
        handle_domain_map_request(session_arg, msg_content);
        break;    
      // 3. cmdline
      case HDCS_MSG_CMD:
        handle_mgr_request(session_arg, msg_content);
        break;    
      // 4. core_stat
      case HDCS_MSG_CORE_STAT:
        handle_core_stat_request(session_arg, msg_content);
        break;    
      case HDCS_MSG_HA_CONN:
        handle_ha_conn_request(session_arg, msg_content);
        break;    
      default:
        break;
    }
  }

  std::vector<std::string> get_host_list () {
    return ha_config.get_host_list();
  }

  int get_replication_count () {
    return 1 + stoi(ha_config.get("replica_size"));
  }

  networking::server listener;
  name_to_conn_map_t conn_map;
  HAConfig ha_config;
  HACmdHandler cmd_handler;
  std::string name;
};
}// ha
}// hdcs
#endif
