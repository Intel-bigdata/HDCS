// Copyright [2017] <Intel>
#ifndef HDCS_BLOCK_OP_H
#define HDCS_BLOCK_OP_H

#include "common/AioCompletion.h"
#include "common/AioCompletionImp.h"
#include "common/Log.h"
#include "common/Timer.h"
#include "common/LRU_Linklist.h"
#include "core/BlockRequest.h"
#include "core/BlockMap.h"
#include "store/DataStore.h"

namespace hdcs {

namespace core {

#define MAX_BLOCK_ID 1073741824 
#define UNPROMOTED 0x00
#define PROMOTED_UNFLUSHED 0x40000000
#define FLUSHED 0xC0000000

typedef LRU_LIST<void*> LRU_TYPE;
class Entry;
class BlockOp {
public:
  BlockOp(Block* block, BlockRequest* block_request, BlockOp* block_op) :
          block(block),
          block_request(block_request),
          block_op_next(block_op) {
  }
  void complete(int r) {
    if (r < 0) {
      log_print("Block complete before expected, block: %lu", block->block_id);
      block_request->complete(r);
    } else {
      if (block_op_next != nullptr) {
        block_op_next->send();
      }
    }
    delete this;
  }
  virtual void send() = 0;
  Block* block;
  BlockRequest* block_request;
  BlockOp* block_op_next;
};

class BlockRequestComplete : public BlockOp{
public:
  BlockRequestComplete(Block* block, BlockRequest &&block_request,
                       BlockOp* block_op) :
                       block_request_inst(block_request),
                       BlockOp(block, &block_request_inst, block_op) {
  }
  void send() {
    complete(0);
  }
  void complete(int r) {
    bool call_next = false;
    block->block_mutex.lock();
    if (block_op_next != nullptr) {
      call_next = true;
    } else {
      block->in_process = false;
      block->block_ops_end = nullptr;
    }
    block->block_mutex.unlock();
    
    if(call_next) {
      log_print("Continue to do next request block: %lu", block->block_id);
      block_request->complete(r);
      block_op_next->send();
    } else {
      log_print("Complete block: %lu", block->block_id);
      block_request->complete(r);
    }
    delete this;
  }
  BlockRequest block_request_inst;
};

class DemoteBlockBuffer : public BlockOp{
public:
  DemoteBlockBuffer(char* block_buffer, Block* block,
                   BlockRequest* block_request, BlockOp* block_op) :
                   BlockOp(block, block_request, block_op),
                   block_buffer(block_buffer) {
  }
  void send() {
    free(block_buffer);
    complete(0);
  }
  char* block_buffer;
};

class ReadFromBuffer : public BlockOp{
public:
  ReadFromBuffer(char* block_buffer, Block* block,
                 BlockRequest* block_request, BlockOp* block_op) :
                 BlockOp(block, block_request, block_op),
                 block_buffer(block_buffer) {
  }
  void send() {
    log_print("ReadFromBuffer block: %lu", block->block_id);
    memcpy(block_request->data_ptr, block_buffer+block_request->offset, block_request->size);
    complete(0);
  }
  char* block_buffer;
};

class WriteToBuffer : public BlockOp{
public:
  WriteToBuffer(char* block_buffer, Block* block,
                 BlockRequest* block_request, BlockOp* block_op) :
                 BlockOp(block, block_request, block_op),
                 block_buffer(block_buffer) {
  }
  void send() {
    log_print("WriteToBuffer block: %lu", block->block_id);
    memcpy(block_buffer+block_request->offset, block_request->data_ptr, block_request->size);
    complete(0);
  }
  char* block_buffer;
};

class PromoteBlockFromBackend : public BlockOp{
public:
  PromoteBlockFromBackend(char* data, store::DataStore* back_store,
                          Block* block, BlockRequest* block_request,
                          BlockOp* block_op) :
                          BlockOp(block, block_request, block_op),
                          data(data),
                          back_store(back_store) {
  }
  void send() {
    log_print("PromoteBlockFromBackend block: %lu", block->block_id);
    uint64_t block_id = block->block_id;
    /*AioCompletion* on_finish = new AioCompletion(
      [this](ssize_t r){
        complete(r);
    });*/ 
    int ret = back_store->block_read(block_id, data);
    complete(ret);
  }
  store::DataStore *back_store;
  char* data;
};

class WriteBlockToBackend : public BlockOp{
public:
  WriteBlockToBackend(char* data, store::DataStore* back_store,
                      Block* block, BlockRequest* block_request,
                      BlockOp* block_op) :
                      BlockOp(block, block_request, block_op),
                      data(data),
                      back_store(back_store) {
  } 
  void send() {
    log_print("WriteBlockToBackend block: %lu", block->block_id);
    //back_store->block_aio_wrire(block->block_id, block_request->data_ptr, this);
    int ret = back_store->block_write(block->block_id, data);
    complete(ret);
  }
  char* data;
  store::DataStore *back_store;
};

class PromoteFromBackend : public BlockOp{
public:
  PromoteFromBackend(char* data, store::DataStore* back_store,
                      Block* block, BlockRequest* block_request,
                      BlockOp* block_op) :
                      BlockOp(block, block_request, block_op),
                      data(data),
                      back_store(back_store) {
  } 
  void send() {
    log_print("PromoteFromBackend block: %lu", block->block_id);
    //back_store->block_aio_wrire(block->block_id, block_request->data_ptr, this);
    uint64_t offset = block->block_id * block->block_size + block_request->offset;
    int ret = back_store->read(data, offset, block_request->size);
    complete(ret);
  }
  char* data;
  store::DataStore *back_store;
};

class WriteToBackend : public BlockOp{
public:
  WriteToBackend(char* data, store::DataStore* back_store,
                      Block* block, BlockRequest* block_request,
                      BlockOp* block_op) :
                      BlockOp(block, block_request, block_op),
                      data(data),
                      back_store(back_store) {
  } 
  void send() {
    log_print("WriteToBackend block: %lu", block->block_id);
    uint64_t offset = block->block_id * block->block_size + block_request->offset;
    int ret = back_store->write(data, offset, block_request->size);
    complete(ret);
  }
  char* data;
  store::DataStore *back_store;
};

class ReadBlockFromCache : public BlockOp{
public:
  ReadBlockFromCache(uint64_t entry_id, char* data,
                     store::DataStore* data_store,
                     Block* block,
                     BlockRequest* block_request,
                     BlockOp* block_op) :
                     BlockOp(block, block_request, block_op),
                     entry_id(entry_id),
                     data(data),
                     data_store(data_store) {
  }
  void send() {
    log_print("ReadBlockFromCache block: %lu", block->block_id);
    int ret = data_store->block_read(entry_id, data);
    complete(ret);
  }
private:
  uint64_t entry_id;
  char* data;
  store::DataStore *data_store;
};

class WriteBlockToCache : public BlockOp{
public:
  WriteBlockToCache(uint64_t entry_id, char* data,
                    store::DataStore* data_store,
                    Block* block,
                    BlockRequest* block_request,
                    BlockOp* block_op) :
                    BlockOp(block, block_request, block_op),
                    entry_id(entry_id),
                    data(data),
                    data_store(data_store) {
  }
  void send() {
    log_print("WriteBlockToCache block: %lu", block->block_id);
    int ret = data_store->block_write(entry_id, data);
    complete(ret);
  }
  uint64_t entry_id;
  store::DataStore *data_store;
  char* data;
};

class DemoteBlockToCache : public BlockOp{
public:
  DemoteBlockToCache(uint64_t entry_id, store::DataStore* data_store,
                    Block* block, BlockRequest* block_request, BlockOp* block_op) :
                    BlockOp(block, block_request, block_op),
                    entry_id(entry_id), data_store(data_store) {
  }
  void send() {
    log_print("DemoteBlockToCache block: %lu", block->block_id);
    int ret = data_store->block_discard(entry_id);
    complete(ret);
  }
  uint64_t entry_id;
  store::DataStore *data_store;
};

class DiscardBlockToBackend : public BlockOp{
public:
  DiscardBlockToBackend(store::DataStore* back_store,
                    Block* block, BlockRequest* block_request, BlockOp* block_op) :
                    BlockOp(block, block_request, block_op),
                    back_store(back_store) {
  }
  void send() {
    log_print("DemoteBlockToCache block: %lu", block->block_id);
    int ret = back_store->block_discard(block->block_id);
    complete(ret);
  }
  store::DataStore *back_store;
};

class UpdateTierMeta : public BlockOp{
public:
  UpdateTierMeta(store::BLOCK_STATUS_TYPE status_data, store::BLOCK_STATUS_TYPE* status,
                 store::DataStore* data_store,
                 Block* block, BlockRequest* block_request, BlockOp* block_op) :
                 status_data(status_data), status(status),
                 data_store(data_store), 
                 BlockOp(block, block_request, block_op) {}
  void send() {
    log_print("UpdateTierMeta block: %lu", block->block_id);
    *status = status_data;
    data_store->block_meta_update(block->block_id, *status);  
    complete(0);
  }
  store::BLOCK_STATUS_TYPE status_data;
  store::BLOCK_STATUS_TYPE *status;
  store::DataStore* data_store;
};

class UpdateCacheMeta : public BlockOp{
public:
  UpdateCacheMeta(store::BLOCK_STATUS_TYPE* status, store::BLOCK_STATUS_TYPE status_data,
               uint32_t entry_id,
               store::DataStore* data_store,
               Block* block, BlockRequest* block_request, BlockOp* block_op) :
               entry_id(entry_id), status(status), status_data(status_data),
               BlockOp(block, block_request, block_op),
               data_store(data_store) {
  }
  void send() {
    log_print("UpdateToMeta block: %lu", block->block_id);
    assert(entry_id <= MAX_BLOCK_ID);
    if (status != nullptr) {
      *status = status_data;
      status_data = status_data | entry_id;
    }
    data_store->block_meta_update(block->block_id, status_data);  
    complete(0);
  }
  store::DataStore *data_store;
  store::BLOCK_STATUS_TYPE* status;
  store::BLOCK_STATUS_TYPE status_data;
  uint32_t entry_id;
};

class DoNothing : public BlockOp{
public:
  DoNothing(Block* block, BlockRequest* block_request,
            BlockOp* block_op) :
            BlockOp(block, block_request, block_op){
  }
  void send() {
    log_print("DoNothing block: %lu", block->block_id);
    complete(0);
  }
};

class UpdateLRU : public BlockOp{
public:
  UpdateLRU(LRU_TYPE* clean_lru, LRU_TYPE* dirty_lru, LRU_TYPE* free_lru,
            store::BLOCK_STATUS_TYPE* status, uint64_t timeout_nanosecond, Entry* entry,
            AioCompletion** entry_timeout_comp_ptr, SafeTimer* timer,
            AioCompletion* timeout_comp_def,
            Block* block, BlockRequest* block_request, BlockOp* block_op) :
            clean_lru(clean_lru), dirty_lru(dirty_lru), free_lru(free_lru),
            status(status), timeout_nanosecond(timeout_nanosecond),
            entry_timeout_comp_ptr(entry_timeout_comp_ptr), timer(timer),
            timeout_comp_def(timeout_comp_def), entry(entry),
            BlockOp(block, block_request, block_op) {
  }
  void send() {
    log_print("UpdateLRU block: %lu", block->block_id);
    if (status == nullptr) {
      clean_lru->remove(block);
      dirty_lru->remove(block);
      free_lru->touch_key(entry);
    } else { 
      if (*status == PROMOTED_UNFLUSHED) {
        clean_lru->remove(block);
        dirty_lru->touch_key(block);
        //add this block to timer here
        if (entry_timeout_comp_ptr != nullptr) {
          if (*entry_timeout_comp_ptr != nullptr) {
            timer->cancel_event(*entry_timeout_comp_ptr);
          }
          *entry_timeout_comp_ptr = timeout_comp_def;
          if (timer != nullptr && timeout_nanosecond != 0) {
            timer->add_event_after(timeout_nanosecond, timeout_comp_def);
          }
        }
      } else {
        if (entry_timeout_comp_ptr != nullptr) {
          delete timeout_comp_def;
          if (*entry_timeout_comp_ptr != nullptr) {
            timer->cancel_event(*entry_timeout_comp_ptr);
            *entry_timeout_comp_ptr = nullptr;
          }
        }
        dirty_lru->remove(block);
        clean_lru->touch_key(block);
      }
    }
    complete(0);
  }
  LRU_TYPE* clean_lru;
  LRU_TYPE* dirty_lru;
  LRU_TYPE* free_lru;
  store::BLOCK_STATUS_TYPE* status;
  Entry* entry;
  uint64_t timeout_nanosecond;
  AioCompletion** entry_timeout_comp_ptr;
  AioCompletion* timeout_comp_def;
  SafeTimer* timer;
};

class ResetCacheEntry : public BlockOp{
public:
  ResetCacheEntry(store::BLOCK_STATUS_TYPE* status,
                  AioCompletion** entry_timeout_comp_ptr,
                  Block* block,
                  BlockRequest* block_request, BlockOp* block_op) :
                  status(status),
                  entry_timeout_comp_ptr(entry_timeout_comp_ptr),
                  BlockOp(block, block_request, block_op){
  }
  void send() {
    log_print("ResetCacheEntry block: %lu", block->block_id);
    *status = UNPROMOTED;
    *entry_timeout_comp_ptr = nullptr;

    block->block_mutex.lock();
    block->entry = nullptr;
    block->block_mutex.unlock();
    complete(0);
  }
  store::BLOCK_STATUS_TYPE* status;
  AioCompletion** entry_timeout_comp_ptr;
};

class ReleaseDiscardBlock : public BlockOp{
public:
  ReleaseDiscardBlock(Block* block, BlockRequest* block_request, BlockOp* block_op) :
                      BlockOp(block, block_request, block_op){
  }
  void send() {
    log_print("ReleaseDiscardBlock block: %lu", block->block_id);
    block->block_mutex.lock();
    block->in_discard_process = false;
    block->block_mutex.unlock();
    complete(0);
  }
};

class FlushBlockToBackend : public BlockOp{
public:
  FlushBlockToBackend(uint64_t entry_id, store::BLOCK_STATUS_TYPE* status,
                     char* data,
                     store::DataStore* back_store,
                     store::DataStore* data_store,
                     Block* block,
                     BlockRequest* block_request,
                     BlockOp* block_op) :
                     BlockOp(block, block_request, block_op),
                     entry_id(entry_id),
                     status(status),
                     data(data),
                     data_store(data_store),
                     back_store(back_store) {
  }
  void send() {
    log_print("FlushBlockToBackend block: %lu", block->block_id);
    ssize_t ret = 0;
    if (status != nullptr & *status == PROMOTED_UNFLUSHED) {
      uint64_t offset = block->block_id * block->block_size;
      ret = data_store->block_read(entry_id, data);
      ret = back_store->write(data, offset, block->block_size);
    }
    complete(ret);
  }
private:
  uint64_t entry_id;
  char* data;
  store::BLOCK_STATUS_TYPE* status;
  store::DataStore *data_store;
  store::DataStore *back_store;
};

} //core

} //hdcs
#endif
