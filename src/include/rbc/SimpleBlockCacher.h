#ifndef SIMPLEBLOCKCACHER_H
#define SIMPLEBLOCKCACHER_H

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <mutex>
#include <ctime>
#include <boost/unordered_map.hpp>
#include <utility>

#include "rbc/BlockCacher.h"
#include "rbc/MetaStore.h"
#include "rbc/common/Log.h"
#include "rbc/common/Mempool.h"


namespace rbc {

class SimpleBlockCacher: public BlockCacher {
public:
    typedef boost::unordered::unordered_map<uint64_t, std::pair<uint64_t, std::time_t>> BLOCK_INDEX;
    //typedef boost::unordered::unordered_map<uint64_t, uint64_t> BLOCK_INDEX;
    struct free_node{
        uint64_t index;
        free_node* next;
        free_node(){
            next = NULL;
        }
    };
    std::string device_name;

    SimpleBlockCacher( std::string device_name, std::string metastore_dir, uint64_t cache_total_size, uint64_t object_size, Mempool* mempool );
    ~SimpleBlockCacher();
    int _remove( uint64_t cache_id );
    int _write( uint64_t cache_id, const char *buf, uint64_t offset, uint64_t length, std::time_t ts );
    ssize_t _read(  uint64_t cache_id, char *buf, uint64_t offset, uint64_t length );
private:
    Mempool *mempool;
    BLOCK_INDEX cache_index;
    ssize_t cache_index_size;
    std::mutex cache_index_lock;

    // create two type of data to index free cache item
    // put evict node to free_node_head
    free_node* free_node_head;
    free_node* free_node_tail;

    uint64_t cache_block_size;
    uint64_t object_size;
    uint64_t _total_size;

    int device_fd;
    MetaStore* metastore;

    int _open(const char* Device_Name);
    int _open(std::string Device_Name);
    int _close(int block_fd);
    int64_t write_index_lookup( uint64_t cache_id, std::time_t ts, bool no_update);
    int64_t read_index_lookup( uint64_t cache_id );
    int64_t index_insert( uint64_t cache_id, uint64_t free_index, std::time_t ts);
    int64_t free_lookup();
    int update_cache_index( const typename BLOCK_INDEX::iterator it, uint64_t, std::time_t ts);
    int update_index(uint64_t cache_id, uint64_t block_id, std::time_t ts);
    uint64_t get_block_index( uint64_t* index, uint64_t offset );
};
}

#endif
