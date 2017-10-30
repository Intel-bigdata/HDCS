// Copyright [2017] <Intel>
#ifndef HDCS_BLOCK_GUARD_H
#define HDCS_BLOCK_GUARD_H

#include "BlockRequest.h"
#include "BlockMap.h"

#include "common/Log.h"
namespace hdcs {

namespace core {

class BlockGuard {
public:
  BlockGuard(uint64_t total_size, uint32_t block_size) : 
         total_size(total_size), block_size(block_size) {
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

  void create_block_requests(Request *req, BlockRequestList *block_request_list) {
    uint64_t offset = req->offset;
    uint64_t length = req->length;
    uint64_t left = length;
    uint64_t offset_by_block = 0;
    uint64_t length_by_block = 0;
    char* data_ptr = req->data;
    Block* block;
    uint64_t block_id;

    std::lock_guard<std::mutex> lock(block_map_lock);
    while(left) {
      block_id = offset / block_size;
      block = block_map[block_id];

      offset_by_block = offset % block_size;
      length_by_block = (block_size - offset_by_block) < left ?
                          (block_size - offset_by_block):left;
      block_request_list->emplace_back(std::move(BlockRequest(
                                       data_ptr, offset_by_block,
                                       length_by_block, req, block)));
      data_ptr = req->data + length_by_block;
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

};

} //namespace core
} //namespace core
#endif
