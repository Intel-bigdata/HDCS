// Copyright [2017] <Intel>
#include "core/BlockOp.h"
#include "core/BlockRequest.h"
#include "core/policy/CachePolicy.h"
#include "common/LRU_Linklist.h"
#include "common/AioCompletionImp.h"
#include <stdlib.h>

namespace hdcs {

namespace core {

CachePolicy::CachePolicy(uint64_t total_size, uint64_t cache_size, uint32_t block_size,
            Block** block_map, store::DataStore *data_store, store::DataStore *back_store,
            float cache_ratio_health,
            WorkQueue<void*> *request_queue,
            uint64_t timeout_nanoseconds,
            CACHE_MODE_TYPE cache_mode,
            int process_threads_num) :
            total_size(total_size), cache_size(cache_size),
            block_size(block_size), data_store(data_store),
            back_store(back_store), go(true),
            cache_ratio_health(cache_ratio_health),
            request_queue(request_queue),
            timeout_nanoseconds(timeout_nanoseconds),
            cache_mode(cache_mode),
            process_threads_num(process_threads_num) {
  blocks_count = total_size / block_size + 1; 
  cache_blocks_count = cache_size / block_size + 1; 

  process_blocks_count = 0;
  flush_all_blocks_count = 0;

  for (int32_t i = cache_blocks_count - 1; i >= 0; i--) {
    entries.push_back(Entry(i));
  }
  EntryToBlock_t *entry_to_block_map = new EntryToBlock_t[cache_blocks_count]();
  EntryToBlock_t *tmp_entry_to_block;
  Block* block_ptr;
  //load meta
  uint32_t status;
  uint32_t entry_id;
  for (int64_t block_id = 0; block_id < blocks_count; block_id++) {
    status = data_store->get_block_meta(block_id);
    if(status >> 30 != 0) {
      entry_id = status & 0X3FFFFFFF;
      tmp_entry_to_block = &(entry_to_block_map[entry_id]);
      tmp_entry_to_block->block = block_map[block_id];
      tmp_entry_to_block->status = status & 0XC0000000;
      tmp_entry_to_block->valid = true;
    }
  }
  entry_id = 0;
  for (auto &entry : entries) {
    tmp_entry_to_block = &(entry_to_block_map[entry_id++]);
    if (tmp_entry_to_block->valid) {
      entry.status = tmp_entry_to_block->status;
      tmp_entry_to_block->block->entry = &entry;
      if (entry.status == PROMOTED_UNFLUSHED)
        dirty_lru.touch_key(tmp_entry_to_block->block);
      else if (entry.status == FLUSHED)
        clean_lru.touch_key(tmp_entry_to_block->block);
    } else {
      free_lru.touch_key(&entry);
    }
  }
  delete entry_to_block_map;
  process_thread = new std::thread(std::bind(&CachePolicy::process, this));
}

CachePolicy::~CachePolicy() {
  go = false;
  process_thread->join();
  delete data_store;
  delete back_store;
  delete process_thread;
}

BlockOp* CachePolicy::map(BlockRequest &&block_request, BlockOp** block_op_end) {
  std::lock_guard<std::mutex> lock(entry_map_lock);
  Entry *entry;
  Block* block = block_request.block;
  assert (block != nullptr);
  char* block_buffer = nullptr;
  Entry* cache_entry = nullptr;
  
  BlockOp* block_op = new BlockRequestComplete(block, std::move(block_request), nullptr);
  BlockRequest* block_request_ptr = block_op->block_request;
  *block_op_end = block_op;

  uint64_t block_id = block->block_id;
  uint64_t block_size = block->block_size;
  AioCompletion *entry_timeout_comp_ptr = nullptr;
  if (timeout_nanoseconds != 0) {
    entry_timeout_comp_ptr = new AioCompletionImp([&, block_id, block_size](ssize_t r){
      //check current inflight flush, so we won't schedule too many
      std::unique_lock<std::mutex> dirty_flush_unique_lock(dirty_flush_cond_lock);
      dirty_flush_cond.wait(dirty_flush_unique_lock, [&]{
          return process_blocks_count < (process_threads_num / 2);
      });

      //if current inflight flush is lower than throttle, create new flush request
      //create a completion lambda to dec when flush done.
      AioCompletion *comp = new AioCompletionImp([&, block_id](ssize_t r){
        if (--process_blocks_count < (process_threads_num / 2)) {
          dirty_flush_cond.notify_all();
        }
      });
      char* data;
      posix_memalign((void**)&data, 4096, sizeof(char)*block_size);
      comp->set_reserved_ptr((void*)data);
      Request* req = new Request(IO_TYPE_FLUSH, data, (block_id * block_size), block_size, comp);
      request_queue->enqueue((void*)req);
      process_blocks_count++;
    });
  }

  switch (block_request.req->io_type) {
    case IO_TYPE_READ:
    // read
      if (block->entry != nullptr){
        // in cache
        cache_entry = block->entry;
        block_op = new UpdateLRU(&clean_lru, &dirty_lru, &free_lru,
                                 &(cache_entry->status), timeout_nanoseconds,
                                 block->entry,
                                 &(cache_entry->timeout_comp), &timer,
                                 entry_timeout_comp_ptr,
                                 block, block_request_ptr, block_op);
        if (block_request.size < block->block_size) {
          // partial read
          posix_memalign((void**)&block_buffer, 4096, block->block_size);
          block_op = new DemoteBlockBuffer(block_buffer, block, block_request_ptr, block_op);
          block_op = new ReadFromBuffer(block_buffer, block, block_request_ptr, block_op);
          block_op = new ReadBlockFromCache(cache_entry->entry_id, block_buffer, data_store,
                                            block, block_request_ptr, block_op); 
        } else {
          // full block read
          block_op = new ReadBlockFromCache(cache_entry->entry_id, block_request_ptr->data_ptr,
                                            data_store, block, block_request_ptr, block_op); 
        }
      } else {
        // not in cache
        cache_entry = (Entry*)free_lru.get_head();
        if (cache_entry != nullptr) {
          // Found a free cache entry
          free_lru.remove((void*)cache_entry);
          block->entry = cache_entry;
          block_op = new UpdateLRU(&clean_lru, &dirty_lru, &free_lru,
                                 &(cache_entry->status), timeout_nanoseconds,
                                 block->entry,
                                 &(cache_entry->timeout_comp), &timer,
                                 entry_timeout_comp_ptr,
                                 block, block_request_ptr, block_op);
          if (block_request_ptr->size < block->block_size) {
            // partial read
            posix_memalign((void**)&block_buffer, 4096, block->block_size);
            block_op = new UpdateCacheMeta(&(cache_entry->status), FLUSHED,
                                         cache_entry->entry_id,
                                         data_store, block, block_request_ptr, block_op);
            block_op = new DemoteBlockBuffer(block_buffer, block, block_request_ptr, block_op);
            block_op = new ReadFromBuffer(block_buffer, block, block_request_ptr, block_op);
            block_op = new WriteBlockToCache(cache_entry->entry_id, block_buffer, data_store,
                                           block, block_request_ptr, block_op); 
            block_op = new PromoteBlockFromBackend(block_buffer, back_store,
                                                 block, block_request_ptr, block_op); 
          } else {
            // full block read
            block_op = new UpdateCacheMeta(&(cache_entry->status), FLUSHED, 
                                       cache_entry->entry_id,
                                       data_store, block, block_request_ptr, block_op);
            block_op = new WriteBlockToCache(cache_entry->entry_id, block_request_ptr->data_ptr, data_store,
                                           block, block_request_ptr, block_op); 
            block_op = new PromoteBlockFromBackend(block_request_ptr->data_ptr, back_store,
                                                 block, block_request_ptr, block_op); 
          }
        } else {
          // no free cache entry
          block_op = new PromoteFromBackend(block_request_ptr->data_ptr, back_store,
                                            block, block_request_ptr, block_op); 
        }
      }
      break;
    case IO_TYPE_WRITE:
    // write
      if (cache_mode == CACHE_MODE_READ_ONLY) {
        block_op = new WriteToBackend(block_request_ptr->data_ptr, back_store,
                                      block, block_request_ptr, block_op); 
      } else {
        if (block->entry != nullptr) {
          // in cache
          cache_entry = block->entry;
          block_op = new UpdateLRU(&clean_lru, &dirty_lru, &free_lru,
                                   &(cache_entry->status), timeout_nanoseconds,
                                   block->entry,
                                   &(cache_entry->timeout_comp), &timer,
                                   entry_timeout_comp_ptr,
                                   block, block_request_ptr, block_op);
          if (timeout_nanoseconds != 0) {
            block_op = new UpdateCacheMeta(&(cache_entry->status), PROMOTED_UNFLUSHED,
                                      cache_entry->entry_id,
                                      data_store, block, block_request_ptr, block_op);
          } else {
            block_op = new UpdateCacheMeta(&(cache_entry->status), FLUSHED,
                                      cache_entry->entry_id,
                                      data_store, block, block_request_ptr, block_op);
            block_op = new WriteToBackend(block_request_ptr->data_ptr, back_store,
                                          block, block_request_ptr, block_op); 
          }
          if (block_request_ptr->size < block->block_size) {
            // partial write
            posix_memalign((void**)&block_buffer, 4096, block->block_size);
            block_op = new DemoteBlockBuffer(block_buffer, block, block_request_ptr, block_op);
            block_op = new WriteBlockToCache(cache_entry->entry_id, block_buffer, data_store,
                                             block, block_request_ptr, block_op); 
            block_op = new WriteToBuffer(block_buffer, block, block_request_ptr, block_op);
            block_op = new ReadBlockFromCache(cache_entry->entry_id, block_request_ptr->data_ptr,
                                              data_store, block, block_request_ptr, block_op); 
          } else {
            // full block write
            block_op = new WriteBlockToCache(cache_entry->entry_id, block_request_ptr->data_ptr, data_store,
                                             block, block_request_ptr, block_op);
          }
        } else {
          // not in cache
          cache_entry = (Entry*)free_lru.get_head();
          if (cache_entry != nullptr) {
            // Found a free cache entry
            free_lru.remove((void*)cache_entry);
            block->entry = cache_entry;
            block_op = new UpdateLRU(&clean_lru, &dirty_lru, &free_lru,
                                   &(cache_entry->status), timeout_nanoseconds,
                                   block->entry,
                                   &(cache_entry->timeout_comp), &timer,
                                   entry_timeout_comp_ptr,
                                   block, block_request_ptr, block_op);
            if (timeout_nanoseconds != 0) {
              block_op = new UpdateCacheMeta(&(cache_entry->status), PROMOTED_UNFLUSHED,
                                     cache_entry->entry_id,
                                     data_store, block, block_request_ptr, block_op);
            } else {
              block_op = new UpdateCacheMeta(&(cache_entry->status), FLUSHED,
                                     cache_entry->entry_id,
                                     data_store, block, block_request_ptr, block_op);
              block_op = new WriteToBackend(block_request_ptr->data_ptr, back_store,
                                          block, block_request_ptr, block_op); 
            }
            if (block_request_ptr->size < block->block_size) {
              // partial write
              posix_memalign((void**)&block_buffer, 4096, block->block_size);
              block_op = new DemoteBlockBuffer(block_buffer, block, block_request_ptr, block_op);
              block_op = new WriteBlockToCache(cache_entry->entry_id, block_buffer, data_store,
                                               block, block_request_ptr, block_op); 
              block_op = new WriteToBuffer(block_buffer, block, block_request_ptr, block_op);
              block_op = new PromoteFromBackend(block_buffer, back_store,
                                                block, block_request_ptr, block_op); 
            } else {
              // full block write
              block_op = new WriteBlockToCache(cache_entry->entry_id, block_request_ptr->data_ptr, data_store,
                                               block, block_request_ptr, block_op);
            }
          } else {
            // run out cache
            block_op = new WriteToBackend(block_request_ptr->data_ptr, back_store,
                                          block, block_request_ptr, block_op); 
          }
        } // end of not in cache
      }// end of cache_write_back
      break;
    case IO_TYPE_DEMOTE_CACHE:
      if (block->entry != nullptr){
        // in cache
        // Lock this block until discard done
        block->in_discard_process = true;
        block_op = new ReleaseDiscardBlock(block, block_request_ptr, block_op);
        block_op = new UpdateLRU(&clean_lru, &dirty_lru, &free_lru,
                                 nullptr, timeout_nanoseconds,
                                 block->entry,
                                 &(block->entry->timeout_comp), &timer,
                                 entry_timeout_comp_ptr,
                                 block, block_request_ptr, block_op);
        block_op = new UpdateCacheMeta(nullptr, UNPROMOTED, 0, data_store,
                                 block, block_request_ptr, block_op);
        block_op = new ResetCacheEntry(&(block->entry->status), &(block->entry->timeout_comp), block, block_request_ptr, block_op);
        block_op = new DemoteBlockToCache(block->entry->entry_id, data_store,
                                          block, block_request_ptr, block_op);
        block_op = new FlushBlockToBackend(block->entry->entry_id, &(block->entry->status),
                                           block_request_ptr->data_ptr,
                                           back_store, data_store,
                                           block, block_request_ptr, block_op); 
      }
      break;
    case IO_TYPE_DISCARD:
      // Lock this block until discard done
      block->in_discard_process = true;
      block_op = new ReleaseDiscardBlock(block, block_request_ptr, block_op);
      if (block->entry != nullptr){
        // in cache
        block_op = new UpdateLRU(&clean_lru, &dirty_lru, &free_lru,
                                 nullptr, timeout_nanoseconds,
                                 block->entry,
                                 &(block->entry->timeout_comp), &timer,
                                 entry_timeout_comp_ptr,
                                 block, block_request_ptr, block_op);
        block_op = new UpdateCacheMeta(nullptr, UNPROMOTED, 0, data_store,
                                 block, block_request_ptr, block_op);
        block_op = new ResetCacheEntry(&(block->entry->status), &(block->entry->timeout_comp), block, block_request_ptr, block_op);
        block_op = new DemoteBlockToCache(block->entry->entry_id, data_store,
                                          block, block_request_ptr, block_op);
      }
      // not in cache
      block_op = new DiscardBlockToBackend(back_store, block, block_request_ptr, block_op);
      break;
    case IO_TYPE_FLUSH:
      if (block->entry != nullptr){
        // in cache
        block_op = new UpdateLRU(&clean_lru, &dirty_lru, &free_lru,
                                 &(block->entry->status), timeout_nanoseconds,
                                 block->entry,
                                 &(block->entry->timeout_comp), &timer,
                                 entry_timeout_comp_ptr,
                                 block, block_request_ptr, block_op);
        block_op = new UpdateCacheMeta(&(block->entry->status), FLUSHED,
                                 block->entry->entry_id,
                                 data_store, block, block_request_ptr, block_op);
        block_op = new FlushBlockToBackend(block->entry->entry_id,
                                 &(block->entry->status),
                                 block_request_ptr->data_ptr,
                                 back_store, data_store,
                                 block, block_request_ptr, block_op); 
      }
      break;
    default:
      block_op = new DoNothing(block, block_request_ptr, block_op); 
  }// switch io_type
  return block_op;
}// map

void CachePolicy::process() {
  std::unique_lock<std::mutex> unique_lock(process_thread_cond_lock);
  while(go) {
    uint64_t dirty_block_count = dirty_lru.get_length();
    uint64_t clean_block_count = clean_lru.get_length();
    uint64_t total_cached_block = dirty_block_count + clean_block_count;
    Block** block_list;

    // prepare need to be flushed and evict blocks
    int64_t need_to_evict_count = total_cached_block - cache_ratio_health * cache_blocks_count;
    if (need_to_evict_count < 0) {
      continue;
    }
    uint64_t need_to_flush_count = 0;
    if (need_to_evict_count > clean_block_count) {
      need_to_flush_count = need_to_evict_count - clean_block_count;
    } else {
      need_to_flush_count = 0;
    }
    if (need_to_evict_count == 0) {
      sleep(1);
      continue;
    }
    block_list = (Block**)malloc(sizeof(Block*)*need_to_evict_count+1);
    memset(block_list, 0, need_to_evict_count+1);
    if (need_to_flush_count) {
      dirty_lru.get_keys((void**)block_list, need_to_flush_count, false);
    }
    clean_lru.get_keys((void**)(block_list+need_to_flush_count), (need_to_evict_count - need_to_flush_count), false);

    //queue req to request_queue
    Request* req;
    char* data;
    AioCompletion* comp;
    Block* block;
    for (uint64_t i = 0; i < need_to_evict_count; i++) {
      process_thread_cond.wait(unique_lock, [&]{
        return process_blocks_count < (process_threads_num / 2);
      });
      // do demote cache one by one
      block = block_list[i];
      log_print("cache_policy demote block_id: %lu", block->block_id);
      comp = new AioCompletionImp([this](ssize_t r){
        if (--process_blocks_count < (process_threads_num / 2)) {
          process_thread_cond.notify_all();
          log_print("Signal cache_policy process to continue");
        }
      });
      posix_memalign((void**)&data, 4096, sizeof(char)*block->block_size);
      comp->set_reserved_ptr((void*)data);
      req = new Request(IO_TYPE_DEMOTE_CACHE, data, (block->block_id * block->block_size), block->block_size, comp);
      request_queue->enqueue((void*)req);
      process_blocks_count++;
    }
    free(block_list);
  }
}

void CachePolicy::flush_all() {
  std::unique_lock<std::mutex> unique_lock(flush_all_cond_lock);
  uint64_t need_to_flush_count = dirty_lru.get_length();
  Block** block_list;

  // prepare need to be flushed and evict blocks
  block_list = (Block**)malloc(sizeof(Block*)*need_to_flush_count+1);
  memset(block_list, 0, need_to_flush_count+1);
  if (need_to_flush_count) {
    dirty_lru.get_keys((void**)block_list, need_to_flush_count, false);
  }

  //queue req to request_queue
  Request* req;
  char* data;
  AioCompletion* comp;
  Block* block;
  for (uint64_t i = 0; i < need_to_flush_count; i++) {
    // do demote cache one by one
    flush_all_cond.wait(unique_lock, [&]{
      return flush_all_blocks_count < process_threads_num;    
    });
    block = block_list[i];
    if (block == 0) continue;
    comp = new AioCompletionImp([this](ssize_t r){
      flush_all_blocks_count--;
      if (!last_batch && flush_all_blocks_count < process_threads_num) {
        flush_all_cond.notify_all();
      }
      if (last_batch && flush_all_blocks_count == 0) {
        log_err("flush_all completed");
        flush_all_cond.notify_all();
      }
    });
    posix_memalign((void**)&data, 4096, sizeof(char)*block->block_size);
    comp->set_reserved_ptr((void*)data);
    req = new Request(IO_TYPE_FLUSH, data, (block->block_id * block->block_size), block->block_size, comp);
    request_queue->enqueue((void*)req);
    flush_all_blocks_count++;
  }
  uint64_t tmp = flush_all_blocks_count;
  log_err("Wait %llu blocks to be flushed", tmp);
  last_batch = true;
  flush_all_cond.wait(unique_lock, [&]{
    return flush_all_blocks_count == 0;
  });
  free(block_list);
}
 
}// core
}// hdcs

