// Copyright [2017] <Intel>
#ifndef HDCS_BLOCK_STORE_H
#define HDCS_BLOCK_STORE_H

#include "store/DataStore.h"
#include <string>

namespace hdcs {
namespace store {

  class SimpleBlockStore : public DataStore {
  public:
    SimpleBlockStore(std::string store_path, uint64_t store_size, uint64_t block_size);
    ~SimpleBlockStore();

    int write(char* data, uint64_t offset, uint64_t size){}
    int read(char* data, uint64_t offset, uint64_t size){}
    int aio_write(char* data, uint64_t offset, uint64_t size, AioCompletion* on_finish){}
    int aio_read(char* data, uint64_t offset, uint64_t size, AioCompletion* on_finish){}
    int block_write(uint64_t block_id, char* data);
    int block_read(uint64_t block_id, char* data);
    int block_aio_write(uint64_t block_id, char* data,
                        AioCompletion* on_finish){}
    int block_aio_read(uint64_t block_id, char* data,
                       AioCompletion* on_finish){}
    int block_discard(uint64_t block_id);
    int block_meta_update(uint64_t block_id, BLOCK_STATUS_TYPE status);

  private:
    int open_and_init(const char* path, uint64_t size);
    int load_mmap(void* mmappedData, uint64_t size, int fd);
    int close_fd(int fd);

    int data_store_fd;
    int meta_store_fd;
    char* meta_store_mmap;
    uint64_t block_size;
    uint64_t store_size;
  };
}// store
}// hdcs

#endif
