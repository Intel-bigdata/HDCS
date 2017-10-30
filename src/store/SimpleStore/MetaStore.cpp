#include "rbc/MetaStore.h"

using namespace rocksdb;

namespace rbc {
MetaStore::MetaStore( std::string kDBPath ):kDBPath(kDBPath){
  // Optimize RocksDB. This is the easiest way to get RocksDB to perform well
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();
  // create the DB if it's not already present
  options.create_if_missing = true;
  Status s = DB::Open(options, kDBPath, &db);
  if (!s.ok())
  {
    log_print("MetaStore::MetaStore open %s failed\n", kDBPath.c_str());
    assert(0);
  }
}

MetaStore::~MetaStore(){
  if ( db ){
    log_print("MetaStore::MetaStore close %s\n", kDBPath.c_str());
    delete db;
  }
}

int MetaStore::get(const char* oid, char* data){
    return 0;
    std::string value;
    Status s = db->Get(ReadOptions(), oid, &value);
    if (!s.ok()){
        return -1;
    }
    data = const_cast<char*>(value.c_str());
    return 0;

}

int MetaStore::put(std::string key, std::string val){
    return 0;
    Status s = db->Put(WriteOptions(), key, val);
    if (!s.ok()){
        return -1;
    }
    return 0;
}

int MetaStore::remove(std::string key){
    Status s = db->Delete(WriteOptions(), key);
    if (!s.ok()){
        return -1;
    }
    return 0;
}

int MetaStore::get_all( CacheMap *cache_map, LRU_LIST<char*> *lru_dirty, LRU_LIST<char*> *lru_clean, Mempool *mempool ){
    rocksdb::Iterator* it = db->NewIterator(rocksdb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        CacheEntry* cache_entry;
        cache_entry = new CacheEntry( mempool );
        cache_entry->load_from_string( it->value().ToString() );
        cache_map->insert( it->key().ToString(), (char*)cache_entry );
        if( cache_entry->get_if_dirty() )
            lru_dirty->touch_key( (char*)cache_entry );
        else
            lru_clean->touch_key( (char*)cache_entry );

        assert(it->status().ok()); // Check for any errors found during the scan
    }
    delete it;
    return 0;
}

int MetaStore::get_all( BLOCK_INDEX *block_map, bool* cached_array, uint64_t object_size ){
    uint64_t value;
    std::time_t ts = 0; //TODO: fix timestamp for block map
    rocksdb::Iterator* it = db->NewIterator(rocksdb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        value = stoull(it->value().ToString());
        block_map->insert( std::make_pair(stoull(it->key().ToString()), std::make_pair(value/object_size, ts)) );
        cached_array[value/object_size] = true;

        assert(it->status().ok()); // Check for any errors found during the scan
    }
    delete it;
    return 0;
}
}
