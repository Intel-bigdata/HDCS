#ifndef HDCS_HA_CLIENT_H
#define HDCS_HA_CLIENT_H

#include "ha/HACore.h"
#include "common/HeartBeat/HeartBeatService.h"
#include "ha/HDCSCoreStatController.h"
#include "ha/HDCSDomainMapRequestHandler.h"
#include "common/AioCompletionImp.h"
#include <iostream>

namespace hdcs {
namespace ha {

class HAClient : public HACore {
public:
  HAClient (std::string name, HAConfig&& ha_config):
    HACore (name, ha_config),
    core_stat_controller(name),
    hb_service(std::move(HeartBeatOpts(1000000000, 1000000000))) {
  }

  void add_ha_server (std::string node_name) {
    std::string node = ha_config.get_host_addr(node_name);
    //create connection to ha_server
    int colon_pos;
    colon_pos = node.find(':');
    std::string addr = node.substr(0, colon_pos);
    std::string port = node.substr(colon_pos + 1, node.length() - colon_pos);
    //not same as current messenger, dehao will fix network later.
    auto it = conn_map.insert(std::pair<std::string, networking::Connection*>(node_name,
      new networking::Connection([&](void* p, std::string s){request_handler(p, s);}, 1, 1)));
    it.first->second->connect(addr, port);
    
    //send connect msg
    HDCS_HA_CONN_MSG msg_content(name); 
    it.first->second->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));

    // provide this connection to hdcs_core_stat_controller
    core_stat_controller.set_conn(it.first->second);
    core_stat_controller.send_stat_map();

    // clean old conn
    for (name_to_conn_map_t::iterator it = conn_map.begin();
      it != conn_map.end();) {
      name_to_conn_map_t::iterator tmp = it++;
      if (tmp->first.compare(node_name) != 0) {
        rm_ha_server(tmp);
      }
    }
  }

  void rm_ha_server (std::string node) {
    // disconnect connection with ha_server
    conn_map.erase(node);  
  }

  void rm_ha_server (name_to_conn_map_t::iterator it) {
    // disconnect connection with ha_server
    conn_map.erase(it);
  }

  std::shared_ptr<HDCSCoreStat> register_core (void* hdcs_core_id) {
    return core_stat_controller.register_core(hdcs_core_id);
  }

  void unregister_core (void* hdcs_core_id) {
    core_stat_controller.unregister_core(hdcs_core_id);
  }

  void handle_heartbeat_request (void* session_arg, std::string msg_content) {
    hb_service.request_handler(session_arg, msg_content, &listener);
  }

  void handle_core_stat_request (void* session_arg, std::string msg_content) {
    core_stat_controller.request_handler(session_arg, msg_content); 
  }

  void handle_mgr_request (void* session_arg, std::string msg_content) {
    cmd_handler.request_handler(session_arg, msg_content, &listener);
  }

  void handle_domain_map_request (void* session_arg, std::string msg_content) {
    HDCS_DOMAIN_ITEM_MSG domain_item_msg(msg_content);
    domain_item = domain_item_msg.get_domain_item();
  }

  void handle_ha_conn_request (void* session_arg, std::string msg_content) {
  }

  std::string printToString_domain_item() {
    bool first = true;
    std::stringstream ss;
    for (auto &it : domain_item) {
      if (first) {
        first = false;
      } else {
        ss << ",";
      }
      ss << it;
    }
    return ss.str();
  }

  HDCS_DOMAIN_ITEM_TYPE get_domain_item() {
    return domain_item;
  }

private:
  HeartBeatService hb_service;
  name_to_conn_map_t conn_map;
  HDCSCoreStatController core_stat_controller;
  HDCS_DOMAIN_ITEM_TYPE domain_item;
};

}// ha
}// hdcs

#endif
