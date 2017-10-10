#include "store/KVStore/KVStore.h"
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

KVStore::KVStore(std::string store_path,
                                   uint64_t total_size,
                                   uint64_t store_size,
                                   uint64_t block_size) :
                                   block_size(block_size),
                                   store_size(store_size){
  std::string data_store_path = store_path + ".data"; 
  std::string meta_store_path = store_path + ".meta"; 

  opts.segment_size = 2<<15;
  opts.expired_time = 500; 

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

KVStore::~KVStore(){
  close_fd(data_store_fd);
  close_fd(meta_store_fd);
}

int KVStore::open_and_init(const char* path, uint64_t size){
    std::string device_name(path);
    if (hlkvds::DB::CreateDB(device_name, opts) < 0) {
        assert(0);
        return -1;
    }


    if (hlkvds::DB::OpenDB(device_name, &testhlkvds, opts) < 0) {
        std::cout << "open error" << std::endl;
        return -1;
    }
    std::cout << "open ok" << std::endl;
    if(!testhlkvds) {
        assert(0);
    }
    return 0;
}

int KVStore::close_fd(int fd) {
  return ::close(fd);
}

int KVStore::load_mmap(void** mmappedData, uint64_t size, int fd) {
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

int KVStore::block_write(uint64_t block_id, char* data) {
  /* skip disk access
  return 0;*/
  std::string cache_key = std::to_string(block_id);
  uint32_t key_len = cache_key.length();
  testhlkvds->Insert(cache_key.c_str(), key_len, data, block_size);
  return 0;

}

int KVStore::block_read(uint64_t block_id, char* data) {
  /* skip disk access
  return 0;*/
  std::string cache_key = std::to_string(block_id);
  uint32_t key_len = cache_key.length();
  std::string cache_value(data);
  testhlkvds->Get(cache_key.c_str(), key_len, cache_value);
  return cache_value.length();
}

int KVStore::block_discard(uint64_t block_id) {
  int ret = 0;
  return ret;
}

int KVStore::block_meta_update(uint64_t block_id, BLOCK_STATUS_TYPE status) {
  meta_store_mmap[block_id] = status;
  return 0;
}

BLOCK_STATUS_TYPE KVStore::get_block_meta(uint64_t block_id) {
  return meta_store_mmap[block_id];
}
}// store
}// hdcs
