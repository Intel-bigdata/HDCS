#ifndef HDCS_CORE_STAT_CONTROLLER_H
#define HDCS_CORE_STAT_CONTROLLER_H

#include "Network/hdcs_networking.h"
#include "common/Timer.h"
#include "ha/HDCSCoreStat.h"

namespace hdcs {

namespace ha {
 
typedef std::map<void*, std::shared_ptr<HDCSCoreStat>> hdcs_core_stat_map_t;

typedef uint8_t HDCS_HA_MSG_TYPE;
#define HDCS_MSG_CORE_STAT  0X32
#define HDCS_HA_CORE_STAT_UPDATE  0XD2

struct HDCS_CORE_STAT_MSG_T {
  HDCS_HA_MSG_TYPE reserved_flag;
  HDCS_CORE_STAT_T* data;
};

class HDCS_CORE_STAT_MSG {
public:
  HDCS_CORE_STAT_MSG (uint8_t cores_num)
    : cores_num(cores_num),
      data_size(sizeof(HDCS_CORE_STAT_T) * cores_num) {
    data_ = (char*)malloc (sizeof(HDCS_HA_MSG_TYPE) + data_size);
    ((HDCS_CORE_STAT_MSG_T*)data_)->reserved_flag = HDCS_MSG_CORE_STAT; 
  }

  HDCS_CORE_STAT_MSG (std::string msg) {
    data_size = msg.length() - sizeof(HDCS_HA_MSG_TYPE);
    data_ = (char*)malloc (sizeof(HDCS_HA_MSG_TYPE) + data_size);
    memcpy(data_, msg.c_str(), sizeof(HDCS_HA_MSG_TYPE) + data_size);
    cores_num = data_size / sizeof(HDCS_CORE_STAT_T);
    printf("received cores_num: %u\n", cores_num);
  }

  ~HDCS_CORE_STAT_MSG() {
    free(data_);
  }

  uint8_t get_cores_num() {
    return cores_num;
  }

  char* data() {
    return data_;
  }

  uint64_t size() {
    return sizeof(HDCS_HA_MSG_TYPE) + data_size;
  }

  void loadline(uint8_t index, HDCS_CORE_STAT_T *stat) {
    char* stat_data = data_ + sizeof(HDCS_HA_MSG_TYPE);
    memcpy(stat_data + index, stat, sizeof(HDCS_CORE_STAT_T));
  } 

  void readline(uint8_t index, HDCS_CORE_STAT_T *stat) {
    char* stat_data = data_ + sizeof(HDCS_HA_MSG_TYPE);
    memcpy(stat, stat_data + index, sizeof(HDCS_CORE_STAT_T));
  } 

private:
  char* data_;
  uint8_t cores_num;
  uint64_t data_size;
};

class HDCSCoreStatController {
public:
  HDCSCoreStatController():conn(nullptr) {
  }

  HDCSCoreStatController(std::string addr, std::string port) {
    std::cout << "addr:" << addr << " port:" << port << std::endl;
    conn = new networking::Connection([&](void* p, std::string s){receiver_handler(p, s);}, 1, 1);
    conn->connect(addr, port);
  }

  ~HDCSCoreStatController() {
    if (conn)
      conn->close();
  }

  void set_conn (networking::Connection* new_conn) {
    conn = new_conn;
  }

  int add_listener (std::string port) {
    core_stat_listener = new networking::server("0.0.0.0", port, 1, 1);
    core_stat_listener->start([&](void* p, std::string s){request_handler(p, s);});
    core_stat_listener->sync_run();
    return 0;
  }

  std::shared_ptr<HDCSCoreStat> register_core (void* hdcs_core_id) {
    std::shared_ptr<AioCompletion> error_handler = std::make_shared<AioCompletionImp>([&](ssize_t r){
      send_stat_map();
      printf("one core stat is updated, send out new stat map\n");
    }, -1);
    auto it = hdcs_core_stat_map.insert(
      std::pair<void*, std::shared_ptr<HDCSCoreStat>>(
        hdcs_core_id,
        std::make_shared<HDCSCoreStat>(hdcs_core_id, error_handler)
      )
    );
    return it.first->second;
  }

  int unregister_core (void* hdcs_core_id) {
    hdcs_core_stat_map.erase(hdcs_core_id);
    return 0;
  }

  hdcs_core_stat_map_t* get_stat_map() {
    return &hdcs_core_stat_map;
  }

  void send_stat_map() {
    HDCS_CORE_STAT_MSG msg_content(hdcs_core_stat_map.size());
    uint8_t i = 0;
    for (auto &item : hdcs_core_stat_map) {
      msg_content.loadline(i++, item.second->get_stat());
    }
    printf ("hdcs_core_stat_controller conn: %p\n", conn);
    conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  }

  void request_handler(void* session_id, std::string msg_content) {
    printf("received new hdcs core stat update\n");
    HDCS_CORE_STAT_MSG core_stat_msg(msg_content);
    HDCS_CORE_STAT_T tmp_stat;
    for (uint8_t i = 0; i < core_stat_msg.get_cores_num(); i++) {
      core_stat_msg.readline(i, &tmp_stat);
      ha_hdcs_core_stat[tmp_stat.hdcs_core_id] = tmp_stat.stat;
    }

    for (auto &print_stat : ha_hdcs_core_stat) {
      printf("new update    ");
      printf("%x", print_stat.first);
      /*uint64_t tmp = (uint64_t)(print_stat.first);
      for (int i = 0; i < sizeof(print_stat.first); i++){
        printf("%x", ((char*)(&tmp))[i]);
      }*/
      printf(": %x\n", print_stat.second);
    }
  }

  void receiver_handler(void* session_id, std::string msg_content) {

  }

private:
  hdcs_core_stat_map_t hdcs_core_stat_map;
  networking::Connection* conn;
  networking::server* core_stat_listener;
  std::map<void*, HDCS_CORE_STAT_TYPE> ha_hdcs_core_stat; 

};
}// ha
}// hdcs
#endif
