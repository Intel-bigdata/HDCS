// Copyright [2017] <Intel>
#ifndef HDCS_DATA_STORE_H
#define HDCS_DATA_STORE_H
#include <stdint.h>
#include "common/AioCompletion.h"

namespace hdcs {
namespace store {

typedef uint32_t BLOCK_STATUS_TYPE; 
#define META_BLOCK_SIZE sizeof(BLOCK_STATUS_TYPE)

  class DataStore {
  public:
    virtual int write(char* data, uint64_t offset, uint64_t size) = 0;
    virtual int read(char* data, uint64_t offset, uint64_t size) = 0;
    virtual int aio_write(char* data, uint64_t offset, uint64_t size, AioCompletion* on_finish) = 0;
    virtual int aio_read(char* data, uint64_t offset, uint64_t size, AioCompletion* on_finish) = 0;
    virtual int block_write(uint64_t block_id, char* data) = 0;
    virtual int block_read(uint64_t block_id, char* data) = 0;
    virtual int block_aio_write(uint64_t block_id, char* data,
                                AioCompletion* on_finish) = 0;
    virtual int block_aio_read(uint64_t block_id, char* data,
                                AioCompletion* on_finish) = 0;
    virtual int block_discard(uint64_t block_id) = 0;
    virtual int block_meta_update(uint64_t block_id, BLOCK_STATUS_TYPE status) = 0;
  };
}// store
}// hdcs

#endif
