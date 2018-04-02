#ifndef HDCS_CORE_STAT_H
#define HDCS_CORE_STAT_H
#include <stdexcept>
#include <chrono>             // std::chrono::seconds
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable, std::cv_status
#include "common/AioCompletionImp.h"

namespace hdcs {

namespace ha {
typedef uint8_t HDCS_CORE_STAT_TYPE;
#define HDCS_CORE_STAT_OK            0XD0
#define HDCS_CORE_STAT_DEGRADED      0XD1
#define HDCS_CORE_STAT_READONLY      0XD2
#define HDCS_CORE_STAT_INCONSISTENT  0XD3
#define HDCS_CORE_STAT_ERROR         0XD4

struct HDCS_CORE_STAT_T {
  void* hdcs_core_id;
  HDCS_CORE_STAT_TYPE stat;
  HDCS_CORE_STAT_T () {}
  HDCS_CORE_STAT_T (void* hdcs_core_id, HDCS_CORE_STAT_TYPE stat) :
    hdcs_core_id(hdcs_core_id), stat(stat) {
  }
};

class HDCSCoreStat {
public:
  HDCSCoreStat(void* hdcs_core_id, std::shared_ptr<AioCompletion> update_handler)
    : update_handler(update_handler), hdcs_core_stat(hdcs_core_id, HDCS_CORE_STAT_OK) {
  }

  ~HDCSCoreStat() {
  }

  void update_stat(HDCS_CORE_STAT_TYPE stat) {
    stat_mutex.lock();
    hdcs_core_stat.stat = stat;
    stat_mutex.unlock();
    update_handler->complete(0);
  }

  HDCS_CORE_STAT_T* get_stat() {
    return &hdcs_core_stat;
  }
private:
  HDCS_CORE_STAT_T hdcs_core_stat;
  std::shared_ptr<AioCompletion> update_handler;
  std::mutex stat_mutex;

};
}// end ha
}// end hdcs
#endif
