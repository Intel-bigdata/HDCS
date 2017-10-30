// Copyright [2017] <Intel>
#ifndef HDCS_TIER_POLICY_H
#define HDCS_TIER_POLICY_H
#include "core/BlockOp.h"
#include "core/BlockRequest.h"
#include "core/policy/Policy.h"
#include "store/DataStore.h"
#include "common/WorkQueue.h"
#include "common/AioCompletion.h"
#include <vector>
#include <mutex>
#include <condition_variable>

namespace hdcs {

namespace core {

typedef std::vector<Entry> Entries;
class TierPolicy : public Policy {
public:
  TierPolicy(uint64_t total_size, uint32_t block_size,
              Block** block_map,
              store::DataStore *data_store,
              store::DataStore *back_store,
              WorkQueue<void*> *request_queue,
              int process_threads_num);
  ~TierPolicy();
  BlockOp* map(BlockRequest &&block_request, BlockOp** block_op_end);
  void flush_all();
  void promote_all();

private:
  Entries entries;
  uint64_t blocks_count;
  uint64_t total_size;
  uint32_t block_size;
  int process_threads_num;

  store::DataStore *data_store;
  store::DataStore *back_store;
  WorkQueue<void*> *request_queue;

  std::condition_variable flush_all_cond;
  std::mutex flush_all_cond_lock;
  std::atomic<uint64_t> flush_all_blocks_count;
  bool last_batch = false;
};
}// core

}// hdcs

#endif
