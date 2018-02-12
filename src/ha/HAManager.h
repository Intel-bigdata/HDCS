#ifndef HDCS_HA_MANAGER_H
#define HDCS_HA_MANAGER_H

#include "ha/HACore.h"
#include "common/HeartBeat/HeartBeatService.h"
#include "ha/HDCSCoreStatController.h"
#include "ha/HDCSDomainMapRequestHandler.h"
#include "common/Timer.h"
#include "common/AioCompletionImp.h"
#include <iostream>
#include "HDCSDomainMap.h"

namespace hdcs {
namespace ha {

class HAManager : public HACore{
public:
  HAManager (std::string name, HAConfig&& ha_config) :
    HACore(name, ha_config),
    core_stat_controller(name, &listener, &global_domain_map),
    global_domain_map(get_host_list(), get_replication_count()),
    hb_worker(std::move(HeartBeatOpts(1000000000, 1000000000))){
      core_stat_controller.set_host_list(get_host_list());
      global_domain_map.generate_domain_map();
      //add a timer check
      uint64_t status_check_timeout = stoull(ha_config.get("status_check_timeout"));
      timeout_event = new AioCompletionImp([&](ssize_t r){
        printf("Initiation status check.\n");
        for (auto &host : ha_config.get_host_list()) {
          if (host == name)
            continue;
          if (conn_map.find(host) == conn_map.end()) {
            core_stat_controller.set_stat_map(host, HDCS_HA_NODE_STAT_DOWN);
          }
        } 
        distribute_domain_map();
        timeout_event = nullptr;
      });
      event_timer.add_event_after(status_check_timeout, timeout_event);
      // set core_stat to Cmd Handler
      cmd_handler.set_core_stat(&core_stat_controller);
  }

  void register_hdcs_node (std::string node_name) {
    auto search_it = conn_map.find(node_name);
    if (search_it != conn_map.end()) {
      return;
    }
    std::string node = ha_config.get_host_addr(node_name);
    //create conn
    int colon_pos;
    colon_pos = node.find(':');
    std::string addr = node.substr(0, colon_pos);
    std::string port = node.substr(colon_pos + 1, node.length() - colon_pos);
    auto it = conn_map.insert(std::pair<std::string, networking::Connection*>(node_name,
      new networking::Connection([&](void* p, std::string s){request_handler(p, s);}, 1, 1)));
    it.first->second->connect(addr, port);

    // register to Heartbeat
    std::shared_ptr<hdcs::AioCompletion> err_handler = std::make_shared<hdcs::AioCompletionImp>([&, node_name](ssize_t r){
      printf("bad news, heartbeat to %s failed.\n", node_name.c_str());    
      // disconnect to failed node
      auto rm_it = conn_map.find(node_name);
      if (rm_it != conn_map.end()) {
        rm_it->second->close();
      }
      conn_map.erase(node_name);
      // update to ha_stat
      core_stat_controller.set_stat_map(node_name, HDCS_HA_NODE_STAT_DOWN);
      //distribute domain_map
      if (!timeout_event) {
        uint64_t status_check_timeout = stoull(ha_config.get("layback_domain_distribute_timeout"));
        timeout_event = new AioCompletionImp([&](ssize_t r){
          printf("Hit timer event, send out domain_map.\n");
          distribute_domain_map();
          timeout_event = nullptr;
        });
        event_timer.add_event_after(status_check_timeout, timeout_event);
      }
    }, -1, false);
    hb_worker.register_node(it.first->second, node, err_handler);

    // register to CMDHandler
    cmd_handler.register_node(node, it.first->second);
  }

  void unregister_hdcs_node (std::string node) {
    //unregister from HeartBeat
    hb_worker.unregister_node(node);

    //unregister from CMDHandler
    cmd_handler.unregister_node(node);
  }

  void distribute_domain_map () {
    for (auto &it : conn_map) {
      // reply with domain map
      HDCS_DOMAIN_ITEM_TYPE domain_item = core_stat_controller.get_host_domain_item(it.first);
      HDCS_DOMAIN_ITEM_MSG domain_item_msg(domain_item);
      it.second->aio_communicate(std::move(std::string(domain_item_msg.data(), domain_item_msg.size())));
    }
  }

  void handle_heartbeat_request (void* session_arg, std::string msg_content) {
    hb_worker.request_handler(session_arg, msg_content);
  }

  void handle_core_stat_request (void* session_arg, std::string msg_content) {
    core_stat_controller.request_handler(session_arg, msg_content); 
  }

  void handle_mgr_request (void* session_arg, std::string msg_content) {
    cmd_handler.request_handler(session_arg, msg_content, &listener);
  }

  void handle_domain_map_request (void* session_arg, std::string msg_content) {
  }

  void handle_ha_conn_request (void* session_arg, std::string msg_content) {
    std::string node_name((msg_content.c_str()) + sizeof(HDCS_HA_MSG_TYPE));
    //set node stat
    core_stat_controller.set_stat_map(node_name, HDCS_HA_NODE_STAT_UP);
    register_hdcs_node(node_name);
    distribute_domain_map();
  }

  void process_cmd (std::string cmd) {
    HDCS_CMD_MSG cmd_msg(HDCS_CMD_MSG_CONSOLE, cmd);
    handle_mgr_request (nullptr, std::move(std::string(cmd_msg.data(), cmd_msg.size())));
  }
private:
  HeartBeatService hb_worker;
  HDCSCoreStatController core_stat_controller;
  HDCSDomainMap global_domain_map;
  SafeTimer event_timer;
  AioCompletion* timeout_event;
};

}// ha
}// hdcs

#endif
