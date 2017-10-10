#include "rbc/KVBlockCacher.h"


namespace rbc {

KVBlockCacher::KVBlockCacher(std::string device_name, std::string metastore_dir, uint64_t cache_total_size,
                             uint64_t p_object_size, Mempool *mempool ): mempool(mempool), device_name(device_name), object_size(p_object_size), _total_size(cache_total_size){

    device_fd = _open(device_name);
    opts.segment_size = 2<<15;
    opts.expired_time = 500;

    if (device_fd < 0) {
        assert(0);
    }
}

KVBlockCacher::~KVBlockCacher(){
    if(testkvdb) {
        std::cout << "close db" << std::endl;
        delete testkvdb;

    }
}

int KVBlockCacher::_open( std::string device_name ){

    if (kvdb::DB::CreateDB(device_name, opts) < 0) {
        assert(0);
        return -1;
    }


    if (kvdb::DB::OpenDB(device_name, &testkvdb, opts) < 0) {
        std::cout << "open error" << std::endl;
        return -1;
    }
    std::cout << "open ok" << std::endl;
    if(!testkvdb) {
        assert(0);
    }
    return 0;
}

int KVBlockCacher::_close( int block_fd ){
    return 0;

}

int KVBlockCacher::_remove( uint64_t cache_id ){
    int ret = 0;
    return ret;
}

int KVBlockCacher::_write( uint64_t cache_id, const char *buf, uint64_t offset, uint64_t length, std::time_t ts ){
    std::string cache_key = std::to_string(cache_id);
    uint32_t key_len = cache_key.length();
    testkvdb->Insert(cache_key.c_str(), key_len, buf, length);
    return 0;

}

ssize_t KVBlockCacher::_read( uint64_t cache_id, char *buf, uint64_t offset, uint64_t length ){
    std::string cache_key = std::to_string(cache_id);
    uint32_t key_len = cache_key.length();
    std::string cache_value(buf);
    testkvdb->Get(cache_key.c_str(), key_len, cache_value);
    return length;
}

}
