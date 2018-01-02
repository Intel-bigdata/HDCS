#ifndef HDCS_COMMON_HEARTBEAT_SERVICE_H
#define HDCS_COMMON_HEARTBEAT_SERVICE_H

#include "Network/hdcs_networking.h"
#include "common/Timer.h"
#include "common/HeartBeat/HeartBeat.h"

namespace hdcs {
 
typedef std::map<std::string, std::shared_ptr<HeartBeat>> hdcs_heartbeat_map_t;

class HeartBeatService {
public:
  HeartBeatService (HeartBeatOpts &&hb_opts) :
    hb_opts(hb_opts),
    event_timer(std::make_shared<SafeTimer>()) {
  }

  ~HeartBeatService () {
  }

  int add_listener (std::string port) {
    hb_server = std::make_shared<networking::server>("0.0.0.0", port, 1, 1);
    hb_server->start([&](void* p, std::string s){request_handler(p, s);});
    hb_server->run();
    return 0;
  }

  int register_node (std::string node, std::shared_ptr<AioCompletion> error_handler) {
    int colon_pos;
    colon_pos = node.find(':');
    std::string addr = node.substr(0, colon_pos);
    std::string port = node.substr(colon_pos + 1, node.length() - colon_pos);
  
    hb_map[node] = std::make_shared<HeartBeat>(addr, port, error_handler, &hb_opts, event_timer);
    return 0;
  }

  int unregister_node (std::string node) {
    //remove events in timer?
    //remove node in map
    hb_map.erase(node);
    return 0;
  }

  void request_handler(void* session_id, std::string s) {
    HDCS_HEARTBEAT_MSG msg_content(HDCS_HEARTBEAT_REPLY);
    hb_server->send(session_id, std::move(std::string(msg_content.data(), msg_content.size())));
  }

private:
  hdcs_heartbeat_map_t hb_map;
  std::shared_ptr<SafeTimer> event_timer;
  HeartBeatOpts hb_opts;
  std::shared_ptr<networking::server> hb_server;
};
}

#endif
