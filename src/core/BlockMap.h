// Copyright [2017] <Intel>
#ifndef HDCS_BLOCK_MAP_H
#define HDCS_BLOCK_MAP_H

#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <list>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace hdcs {

namespace core {

typedef std::atomic<uint8_t> BlockStatus;
struct Entry;
class BlockOp;

struct Block {
  Block(uint64_t block_id, uint32_t block_size):
    block_id(block_id), block_size(block_size),
    entry(nullptr),
    in_process(false), block_ops_end(nullptr),
    in_discard_process(false) {}
  uint64_t block_id;
  uint32_t block_size;
  Entry* entry;
  std::mutex block_mutex;
  bool in_process;
  bool in_discard_process;
  BlockOp* block_ops_end;
};

//typedef std::unordered_map<uint64_t, Block*> BlockMap;
typedef Block* BlockMap;
}// core

}// hdcs
#endif
