// Copyright [2017] <Intel>
#ifndef HDCS_BLOCK_GUARD_H
#define HDCS_BLOCK_GUARD_H

#include "BlockRequest.h"
#include "BlockMap.h"
#include "common/AioCompletionImp.h"
#include "common/HDCS_REQUEST_CTX.h"
#include "Network/client.h"
#include "common/Log.h"
#include "common/Timer.h"
#include <mutex>
namespace hdcs {

namespace core {
typedef std::map<std::string, void*> hdcs_replica_nodes_t;

class BlockGuard {
public:
  BlockGuard(uint64_t total_size, uint32_t block_size,
             int replica_size, uint64_t request_timeout,
             hdcs_replica_nodes_t&& connection_v) :
         total_size(total_size),
         block_size(block_size),
         replica_size(replica_size),
         request_timeout(request_timeout),
         connection_v(connection_v),
         tid(1) {
    block_count = total_size / block_size;
    log_err("Total blocks: %lu", block_count);
    block_map = new Block*[block_count]();
    for (uint64_t i = 0; i < block_count; i++) {
      Block* block = new Block(i, block_size);
      block_map[i] = block;
    }
  }

  ~BlockGuard() {
    for (uint64_t i = 0; i < block_count; i++) {
      delete block_map[i];
    }
  }

  void create_block_requests(std::shared_ptr<Request> req, BlockRequestList *block_request_list) {
    uint64_t offset = req->offset;
    uint64_t length = req->length;
    uint64_t left = length;
    uint64_t offset_by_block = 0;
    uint64_t length_by_block = 0;
    char* data_ptr = req->data;
    Block* block;
    uint64_t block_id;

    uint64_t tmp_length = (length + (offset % block_size));
    int shared_count = tmp_length % block_size == 0 ? 0 : 1;
    shared_count += tmp_length / block_size;

    //create replication complete if needed.
    AioCompletion* replica_write_comp = nullptr;
    AioCompletion* original_req_comp = req->comp;
    replica_write_comp = new AioCompletionImp([&, original_req_comp](ssize_t r, void* data){
      // make this as a timeout event
      // 1. if called by timer, return -2
      // 2. if called by reply handler, return as input and
      // remove itself from timer.
      if ( r != -2 ) {
        timer.cancel_event((AioCompletion*)data, false);
      }
      original_req_comp->complete(r);
    }, (replica_size + 1), false);
    replica_write_comp->set_reserved_ptr((void*)replica_write_comp);
    req->comp = replica_write_comp;

    std::shared_ptr<store::DataStoreRequest> data_store_req = std::make_shared<store::DataStoreRequest>(
        shared_count, block_size, replica_write_comp, &connection_v, &timer, request_timeout);

    std::lock_guard<std::mutex> lock(block_map_lock);
    while(left) {
      block_id = offset / block_size;
      block = block_map[block_id];

      offset_by_block = offset % block_size;
      length_by_block = (block_size - offset_by_block) < left ?
                          (block_size - offset_by_block):left;
      block_request_list->emplace_back(std::move(BlockRequest(
                                       data_ptr,
                                       offset_by_block,
                                       length_by_block,
                                       req,
                                       block,
                                       data_store_req,
                                       tid++)));
      if (tid == 0) tid++;
      data_ptr += length_by_block;
      left -= length_by_block;
      offset += length_by_block;
    }
  }

  uint64_t get_block_count() {
    return block_count;
  }

  BlockMap* get_block_map() {
    return block_map;
  }

private:
  std::mutex block_map_lock;
  BlockMap *block_map;
  uint64_t total_size;
  uint32_t block_size;
  uint64_t block_count;
  int replica_size;
  hdcs_replica_nodes_t connection_v;
  SafeTimer timer;
  uint64_t request_timeout;
  uint32_t tid; //Use tid to record for all requests.
};

} //namespace core
} //namespace core
#endif
