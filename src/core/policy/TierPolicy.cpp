// Copyright [2017] <Intel>
#include "core/BlockOp.h"
#include "core/BlockRequest.h"
#include "core/policy/TierPolicy.h"
#include "common/AioCompletionImp.h"
#include <stdlib.h>

namespace hdcs {

namespace core {

TierPolicy::TierPolicy(uint64_t total_size, uint32_t block_size,
                       Block** block_map,
                       store::DataStore *data_store,
                       store::DataStore *back_store,
                       WorkQueue<std::shared_ptr<Request>> *request_queue,
                       int process_threads_num,
                       HDCSCoreStatGuard* core_stat): 
                      total_size(total_size), block_size(block_size),
                      data_store(data_store), back_store(back_store),
                      request_queue(request_queue), core_stat_ptr(core_stat),
                      process_threads_num(process_threads_num) {
  //promote all blocks when doing initialization
  blocks_count = total_size / block_size; 
  flush_all_blocks_count = 0;

  entries.resize(blocks_count);
  uint64_t block_id = 0;
  store::BLOCK_STATUS_TYPE status;
  for (auto &entry : entries) {
    status = data_store->get_block_meta(block_id);
    entry.status = status;
    (block_map[block_id])->entry = &entry;
    block_id++;
  }

}

TierPolicy::~TierPolicy() {
  delete data_store;
  delete back_store;
}

BlockOp* TierPolicy::map(BlockRequest &&block_request, BlockOp** block_op_end) {
  Block* block = block_request.block;
  assert (block != nullptr);

  BlockOp* block_op = new BlockRequestComplete(block, std::move(block_request), nullptr);
  BlockRequest* block_request_ptr = block_op->block_request;
  *block_op_end = block_op;

  uint64_t block_id = block->block_id;
  uint64_t block_size = block->block_size;
  char* block_buffer = nullptr;
  HDCS_CORE_STAT_TYPE core_stat;

  switch (block_request.req->io_type) {
    case IO_TYPE_READ:
      //read
      core_stat = core_stat_ptr->get_hdcs_core_state();
      if (core_stat == HDCS_CORE_STAT_ERROR ||
          core_stat == HDCS_CORE_STAT_INCONSISTENT) {
        block_op = new CompleteWithError(block, block_request_ptr, block_op);
      } else {
        if (block->entry == UNPROMOTED) {
          int ret = posix_memalign((void**)&block_buffer, 4096, block->block_size);
          if (ret < 0) {
            return nullptr;
          }
  
          block_op = new UpdateTierMeta(FLUSHED, &(block->entry->status), data_store,
                                        block, block_request_ptr, block_op);
          block_op = new DemoteBlockBuffer(block_buffer, block, block_request_ptr, block_op);
          block_op = new ReadFromBuffer(block_buffer, block, block_request_ptr, block_op);
          block_op = new ExecuteDataStoreRequest(block_id, block_buffer, data_store,
                                                 block, block_request_ptr, block_op);
          block_op = new PromoteBlockFromBackend(block_buffer, back_store,
                                                 block, block_request_ptr, block_op); 
        } else {
          if (block_request.size < block->block_size) {
            // partial read
            int ret = posix_memalign((void**)&block_buffer, 4096, block->block_size);
            if (ret < 0) {
              return nullptr;
            }
  
            block_op = new DemoteBlockBuffer(block_buffer, block, block_request_ptr, block_op);
            block_op = new ReadFromBuffer(block_buffer, block, block_request_ptr, block_op);
            block_op = new ReadBlockFromCache(block_id, block_buffer, data_store,
                                              block, block_request_ptr, block_op); 
          } else {
            // full block read
            block_op = new ReadBlockFromCache(block_id, block_request_ptr->data_ptr,
                                              data_store, block, block_request_ptr, block_op); 
          }
        }
      }
      break;
    case IO_TYPE_WRITE:
      //write
      core_stat = core_stat_ptr->get_hdcs_core_state();
      if (core_stat == HDCS_CORE_STAT_ERROR ||
          core_stat == HDCS_CORE_STAT_INCONSISTENT ||
          core_stat == HDCS_CORE_STAT_READONLY) {
        block_op = new CompleteWithError(block, block_request_ptr, block_op);
      } else {
        block_op = new UpdateTierMeta(PROMOTED_UNFLUSHED, &(block->entry->status),
                                     data_store, block, block_request_ptr, block_op);
        if (block->entry == UNPROMOTED) {
          if (block_request_ptr->size < block->block_size) {
            // partial write
            int ret = posix_memalign((void**)&block_buffer, 4096, block->block_size);
            if (ret < 0) {
              return nullptr;
            }
  
            block_op = new DemoteBlockBuffer(block_buffer, block, block_request_ptr, block_op);
    
            // todo
            block_op = new ExecuteDataStoreRequest(block_id, block_buffer, data_store, 
                                                   block, block_request_ptr, block_op);
    
            block_op = new WriteToBuffer(block_buffer, block, block_request_ptr, block_op);
            block_op = new PromoteBlockFromBackend(block_buffer, back_store,
                                                   block, block_request_ptr, block_op); 
          } else {
            // full block write
            // todo
            block_op = new ExecuteDataStoreRequest(block_id, block_request_ptr->data_ptr, data_store,
                                                   block, block_request_ptr, block_op);
          }
        } else {
          if (block_request_ptr->size < block->block_size) {
            // partial write
            int ret = posix_memalign((void**)&block_buffer, 4096, block->block_size);
            if (ret < 0) {
              return nullptr;
            }
  
            block_op = new DemoteBlockBuffer(block_buffer, block, block_request_ptr, block_op);
            // todo
            block_op = new ExecuteDataStoreRequest(block_id, block_buffer, data_store,
                                                   block, block_request_ptr, block_op);
            block_op = new WriteToBuffer(block_buffer, block, block_request_ptr, block_op);
            block_op = new ReadBlockFromCache(block_id, block_buffer,
                                              data_store, block, block_request_ptr, block_op); 
          } else {
            // full block write
            // todo
            block_op = new ExecuteDataStoreRequest(block_id, block_request_ptr->data_ptr, data_store, 
                                                   block, block_request_ptr, block_op);
          }
        }
      }
      break;
    case IO_TYPE_DISCARD:
      //discard
      core_stat = core_stat_ptr->get_hdcs_core_state();
      if (core_stat == HDCS_CORE_STAT_ERROR ||
          core_stat == HDCS_CORE_STAT_INCONSISTENT ||
          core_stat == HDCS_CORE_STAT_READONLY) {
        block_op = new CompleteWithError(block, block_request_ptr, block_op);
      } else {
        block->in_discard_process = true;
        block_op = new ReleaseDiscardBlock(block, block_request_ptr, block_op);
        block_op = new UpdateTierMeta(UNPROMOTED, &(block->entry->status),
                                     data_store, block, block_request_ptr, block_op);
        block_op = new DemoteBlockToCache(block_id, data_store,
                                          block, block_request_ptr, block_op);
        block_op = new FlushBlockToBackend(block_id, &(block->entry->status),
                                           block_request_ptr->data_ptr,
                                           back_store, data_store,
                                           block, block_request_ptr, block_op); 
      }
      break;
    case IO_TYPE_FLUSH:
      core_stat = core_stat_ptr->get_hdcs_core_state();
      if (core_stat == HDCS_CORE_STAT_ERROR ||
          core_stat == HDCS_CORE_STAT_INCONSISTENT) {
        block_op = new CompleteWithError(block, block_request_ptr, block_op);
      } else {
        //flush
        block_op = new UpdateTierMeta(FLUSHED, &(block->entry->status),
                                     data_store, block, block_request_ptr, block_op);
        block_op = new FlushBlockToBackend(block_id, &(block->entry->status),
                                           block_request_ptr->data_ptr,
                                           back_store, data_store,
                                           block, block_request_ptr, block_op); 
      }
      break;
    case IO_TYPE_PROMOTE:
      core_stat = core_stat_ptr->get_hdcs_core_state();
      if (core_stat == HDCS_CORE_STAT_ERROR ||
          core_stat == HDCS_CORE_STAT_INCONSISTENT) {
        block_op = new CompleteWithError(block, block_request_ptr, block_op);
      } else {
        //promote
        block_op = new UpdateTierMeta(FLUSHED, &(block->entry->status), data_store,
                                      block, block_request_ptr, block_op);
        block_op = new WriteBlockToCache(block_id, block_request_ptr->data_ptr, data_store,
                                         block, block_request_ptr, block_op); 
        block_op = new PromoteBlockFromBackend(block_request_ptr->data_ptr, back_store,
                                               block, block_request_ptr, block_op); 
      }
      break;
    default:
      block_op = new DoNothing(block, block_request_ptr, block_op); 
      break;
  }// end of switch io_type
  return block_op;
}

void TierPolicy::flush_all() {
  std::unique_lock<std::mutex> unique_lock(flush_all_cond_lock);
  //queue req to request_queue
  char* data;
  AioCompletion* comp;
  //Block* block;
  for (uint64_t block_id = 0; block_id < blocks_count; block_id++) {
    //block = block_map[block_id];
    flush_all_cond.wait(unique_lock, [&]{
      return flush_all_blocks_count < process_threads_num;    
    });
    comp = new AioCompletionImp([this](ssize_t r){
      flush_all_blocks_count--;
      if ( !last_batch && flush_all_blocks_count < process_threads_num) {
        flush_all_cond.notify_all();
      }
      if ( last_batch && flush_all_blocks_count == 0) {
        flush_all_cond.notify_all();
      }
    });
    int ret = posix_memalign((void**)&data, 4096, sizeof(char)*block_size);
    if (ret < 0) {
      return;
    }

    comp->set_reserved_ptr((void*)data);
    std::shared_ptr<Request> req = std::make_shared<Request>(IO_TYPE_FLUSH, data, (block_id * block_size), block_size, comp);
    request_queue->enqueue(req);
    flush_all_blocks_count ++;
  }
  uint64_t tmp = flush_all_blocks_count;
  log_err("Wait %llu blocks to be flushed", tmp);
  last_batch = true;
  flush_all_cond.wait(unique_lock, [&]{
    return flush_all_blocks_count == 0;
  });
}

void TierPolicy::promote_all() {
  std::unique_lock<std::mutex> unique_lock(flush_all_cond_lock);
  //queue req to request_queue
  char* data;
  AioCompletion* comp;
  //Block* block;
  for (uint64_t block_id = 0; block_id < blocks_count; block_id++) {
    //block = block_map[block_id];
    flush_all_cond.wait(unique_lock, [&]{
      return flush_all_blocks_count < process_threads_num;    
    });
    comp = new AioCompletionImp([this](ssize_t r){
      flush_all_blocks_count--;
      if ( !last_batch && flush_all_blocks_count < process_threads_num) {
        flush_all_cond.notify_all();
      }
      if ( last_batch && flush_all_blocks_count == 0) {
        flush_all_cond.notify_all();
      }
    });
    int ret = posix_memalign((void**)&data, 4096, sizeof(char)*block_size);
    if (ret < 0) {
      return;
    }
    comp->set_reserved_ptr((void*)data);
    std::shared_ptr<Request> req = std::make_shared<Request>(IO_TYPE_PROMOTE, data, (block_id * block_size), block_size, comp);
    request_queue->enqueue(req);
    flush_all_blocks_count ++;
  }
  uint64_t tmp = flush_all_blocks_count;
  log_err("Wait %llu blocks to be promoted", tmp);
  last_batch = true;
  flush_all_cond.wait(unique_lock, [&]{
    return flush_all_blocks_count == 0;
  });
}

}// core

}// hdcs
