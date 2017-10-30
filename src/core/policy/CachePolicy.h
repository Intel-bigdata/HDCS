// Copyright [2017] <Intel>
#ifndef HDCS_CACHE_POLICY_H
#define HDCS_CACHE_POLICY_H
#include "core/BlockOp.h"
#include "core/BlockRequest.h"
#include "core/policy/Policy.h"
#include "common/LRU_Linklist.h"
#include "store/DataStore.h"
#include "common/WorkQueue.h"
#include "common/AioCompletion.h"
#include "common/Timer.h"
#include <mutex>
#include <condition_variable>

namespace hdcs {

namespace core {

typedef uint8_t CACHE_MODE_TYPE;
#define CACHE_MODE_WRITE_BACK 0XF0
#define CACHE_MODE_READ_ONLY 0XF1

typedef std::vector<Entry> Entries;
class CachePolicy : public Policy {
public:
  CachePolicy(uint64_t total_size, uint64_t cache_size, uint32_t block_size,
              Block** block_map,
              store::DataStore *data_store,
              store::DataStore *back_store,
              float cache_ratio_health,
              WorkQueue<void*> *request_queue,
              uint64_t timeout_nanoseconds,
              CACHE_MODE_TYPE cache_mode,
              int process_threads_num);
  ~CachePolicy();
  BlockOp* map(BlockRequest &&block_request, BlockOp** block_op_end);
  void flush_all();

private:
  void process();
  bool go;
  uint64_t blocks_count;
  uint64_t cache_blocks_count;
  Entries entries;
  std::mutex entry_map_lock;
  LRU_TYPE free_lru;
  LRU_TYPE dirty_lru;
  LRU_TYPE clean_lru;
  uint64_t total_size;
  uint64_t cache_size;
  uint32_t block_size;
  uint64_t timeout_nanoseconds;
  int process_threads_num;
  CACHE_MODE_TYPE cache_mode;
  store::DataStore *data_store;
  store::DataStore *back_store;
  float cache_ratio_health;
  WorkQueue<void*> *request_queue;
  SafeTimer timer;

  std::thread *process_thread;
  std::condition_variable process_thread_cond;
  std::mutex process_thread_cond_lock;
  std::atomic<uint64_t> process_blocks_count;
  std::atomic<uint64_t> flush_all_blocks_count;
  
  std::condition_variable dirty_flush_cond;
  std::mutex dirty_flush_cond_lock;

  std::condition_variable flush_all_cond;
  std::mutex flush_all_cond_lock;
  bool last_batch = false;

  struct EntryToBlock_t {
    bool valid;
    Block* block;
    store::BLOCK_STATUS_TYPE status;
    EntryToBlock_t(): valid(false){}
  };
};

}//core

}//hdcs

#endif
