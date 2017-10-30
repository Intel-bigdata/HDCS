// Copyright [2017] <Intel>
#ifndef HDCS_RBD_STORE_H
#define HDCS_RBD_STORE_H

#include "store/DataStore.h"
#include "common/AioCompletion.h"
#include "common/Log.h"
#include <rbd/librbd.h>

namespace hdcs {

namespace store {

  class RBDImageStore : public DataStore{
  public:
    RBDImageStore(std::string pool_name,
                  std::string volume_name,
                  uint64_t block_size);
    ~RBDImageStore();
    int write(char* data, uint64_t offset, uint64_t size);
    int read(char* data, uint64_t offset, uint64_t size);
    int aio_write(char* data, uint64_t offset, uint64_t size, AioCompletion* on_finish);
    int aio_read(char* data, uint64_t offset, uint64_t size, AioCompletion* on_finish);
    int block_write(uint64_t block_id, char* data);
    int block_read(uint64_t block_id, char* data);
    int block_aio_write(uint64_t block_id, char* data, AioCompletion* on_finish);
    int block_aio_read(uint64_t block_id, char* data, AioCompletion* on_finish);
    int block_discard(uint64_t block_id){}
    int block_meta_update(uint64_t block_id, BLOCK_STATUS_TYPE status){}
    BLOCK_STATUS_TYPE get_block_meta(uint64_t block_id){}

  private:
    std::string pool_name;
    std::string volume_name;
    uint64_t block_size;

    rados_t cluster;
    rados_ioctx_t io_ctx;
    rbd_image_t image_ctx;
  };

}// store

}// hdcs

#endif
