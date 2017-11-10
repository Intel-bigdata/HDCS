#include "store/SimpleStore/SimpleBlockStore.h"
#include "common/Log.h"
#include <string>
#include <cstring>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

namespace hdcs {
namespace store {

SimpleBlockStore::SimpleBlockStore(std::string store_path,
                                   uint64_t total_size,
                                   uint64_t store_size,
                                   uint64_t block_size) :
                                   block_size(block_size),
                                   store_size(store_size){
  std::string data_store_path = store_path + ".data"; 
  std::string meta_store_path = store_path + ".meta"; 

  uint64_t meta_block_count = total_size%block_size ?
                              total_size/block_size+1 :
                              total_size/block_size;
  uint64_t data_block_count = store_size%block_size ?
                              store_size/block_size+1 :
                              store_size/block_size;
  int ret = open_and_init(data_store_path.c_str(), store_size);
  if (ret < 0) {
    log_print("Datastore init failed with err: %s", std::strerror(ret));
    assert(0);
  }
  data_store_fd = ret;
  ret = open_and_init(meta_store_path.c_str(), meta_block_count*META_BLOCK_SIZE);
  if (ret < 0) {
    log_print("Metastore init failed with err: %s", std::strerror(ret));
    assert(0);
  }
  meta_store_fd = ret;
  ret = load_mmap((void**)&meta_store_mmap, meta_block_count*META_BLOCK_SIZE, meta_store_fd);
  if (ret < 0) {
    log_print("Metastore mmap failed with err: %s", std::strerror(ret));
    assert(0);
  }
}

SimpleBlockStore::~SimpleBlockStore(){
  close_fd(data_store_fd);
  close_fd(meta_store_fd);
}

int SimpleBlockStore::open_and_init(const char* path, uint64_t size){
    int mode = O_CREAT | O_DIRECT | O_NOATIME | O_RDWR | O_SYNC, permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    //int mode = O_CREAT | O_NOATIME | O_RDWR | O_SYNC, permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    int fd = ::open( path, mode, permission );
    if (fd < 0) {
        log_err( "[ERROR] SimpleBlockStore::open_and_init, unable to open %s, error: %s ", path, std::strerror(fd) );
        ::close(fd);
        return fd;
    }

    struct stat file_st;
    memset(&file_st, 0, sizeof(file_st));
    ::fstat(fd, &file_st);
    if (file_st.st_size < size) {
      if (-1 == ftruncate(fd, size)) {
        ::close(fd);
        return fd;
      }
    }
    return fd;
}

int SimpleBlockStore::close_fd(int fd) {
  return ::close(fd);
}

int SimpleBlockStore::load_mmap(void** mmappedData, uint64_t size, int fd) {
  /* skip disk access
  *mmappedData = malloc(size);
  return 0;*/
  *mmappedData = ::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
  if (*mmappedData != MAP_FAILED) {
    return 0;
  } else {
    return -1;
  }
}

int SimpleBlockStore::block_write(uint64_t block_id, char* data) {
  /* skip disk access
  return 0;*/
  int ret = ::pwrite(data_store_fd, data, block_size, block_id * block_size); 
  if (ret < 0) {
    log_err("[ERROR] SimpleBlockStore::block_write, unable to write block %lu, error: %s ", block_id, std::strerror(ret));
    return ret;
  }
  return ret;
}

int SimpleBlockStore::block_read(uint64_t block_id, char* data) {
  /* skip disk access
  return 0;*/
  int ret = ::pread(data_store_fd, data, block_size, block_id * block_size); 
  if (ret < 0) {
    log_err("[ERROR] SimpleBlockStore::block_read, unable to read block %lu, error: %s ", block_id, std::strerror(ret));
    return ret;
  }
}

int SimpleBlockStore::block_discard(uint64_t block_id) {
  int ret = ::fallocate(data_store_fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                        block_id * block_size, block_size);
  if (ret < 0) {
    log_err("[ERROR] SimpleBlockStore::block_discard, unable to discard block %lu, error: %s ", block_id, std::strerror(ret));
    return ret;
  }
}

int SimpleBlockStore::block_meta_update(uint64_t block_id, BLOCK_STATUS_TYPE status) {
  meta_store_mmap[block_id] = status;
  return 0;
}

BLOCK_STATUS_TYPE SimpleBlockStore::get_block_meta(uint64_t block_id) {
  return meta_store_mmap[block_id];
}
}// store
}// hdcs
