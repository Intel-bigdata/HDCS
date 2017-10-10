#include "rbc/AgentService.h"

namespace rbc {
AgentService::AgentService( Context* cct ): cct(cct){
    int tp_size = stoi(cct->config->configValues["agent_threads_num"]);
    threadpool = new ThreadPool(tp_size);
    process_tp = new ThreadPool(2);
    cache_total_size = cct->cache_total_size;
    cache_dirty_ratio_min = stof(cct->config->configValues["cache_dirty_ratio_min"]);
    object_size = stoull(cct->config->configValues["object_size"]);
    cache_flush_interval = stoi(cct->config->configValues["cache_flush_interval"]);
    cache_evict_interval = stoi(cct->config->configValues["cache_evict_interval"]);
    cache_ratio_max = stof(cct->config->configValues["cache_ratio_max"]);
    cache_ratio_health = stof(cct->config->configValues["cache_ratio_health"]);
    process_tp->schedule(boost::bind(&AgentService::_process_flush, this));
    process_tp->schedule(boost::bind(&AgentService::_process_evict, this));
    log_print("AgentService constructed\n");
}

AgentService::~AgentService(){
    log_print("delete agentservice started\n");
    cct->go = false;
    log_print("AgentService::~AgentService start to delete threadpool\n");
    threadpool->wait();
    delete threadpool;
    log_print("AgentService::~AgentService start to delete process_tp\n");
    process_tp->wait();
    delete process_tp;
    log_print("AgentService::~AgentService flush_all\n");
    flush_all();
    log_print("after last time flush_all, still need to wait %" PRId32 "op to finish\n", uint64_t(cct->flush_op_count));
    while( cct->flush_op_count > 0 ) {
        std::unique_lock<std::mutex> flush_stop_scoped_lock(cct->flush_stop_mutex);
        cct->flush_stop_cond.wait_for(flush_stop_scoped_lock, std::chrono::milliseconds(1000));
    }
    //TODO: what if some flush fails, retry here?
    log_print("after last time flush_all, lru_dirty length is %lu\n", cct->lru_dirty->get_length());
    log_print("delete agentservice complete\n");
}

void AgentService::do_flush( CacheEntry* c_entry ){

    ReadLock cache_entry_r_lock(c_entry->rwlock);
    if( c_entry == NULL || c_entry->is_null() ){
        return;
    }

    if(!c_entry->get_if_dirty()){
        return;
    }

    if(c_entry->inflight_flush){
        //the c_entry is still pending for flush completion.
        return;
    }

    /*
    size_t this_id = std::hash<std::thread::id>()(std::this_thread::get_id());
    if (!c_entry->set_flush_owner(this_id)) {
        return;
    }
    */

    char* data_from_cache = (char*)cct->mempool->malloc( sizeof(char) * cct->object_size );
    char* data;
    uint64_t offset_by_cache_entry = 0;
    uint64_t cache_id = c_entry->get_offset()/cct->object_size;
    ssize_t ret = cct->cacher->_read( cache_id, data_from_cache, offset_by_cache_entry, cct->object_size );
    if(ret < 0){
        log_err( "AgentService::flush read_from_cache failed.Details: %s\n", (c_entry->get_name()).c_str() );
        cct->mempool->free( (void*)data_from_cache, sizeof(char) * cct->object_size);
        return;
    }
    //create a char* list by cachemap
    DATAMAP_T data_map = c_entry->get_data_map();
    for(DATAMAP_T::iterator it = data_map.begin(); it != data_map.end(); it++){
        data =&data_from_cache[it->first];
        Op *new_op = new Op( cct, c_entry->get_location_id(), (c_entry->get_offset() + it->first), data, it->second, cct->mempool);
        new_op->cache_entry = c_entry;
        C_AioBackendCompletion *onfinish = new C_AioBackendWrite( new_op );
        ret = new_op->backstore_aio_write( onfinish );
        if(ret < 0){
            onfinish->finish( ret );
            log_print("AgentService::flush write_to_backend failed. Details: %s:%lu:%lu\n", (c_entry->get_location_id()).c_str(), (c_entry->get_offset() + it->first), it->second);
            delete onfinish;
        }
    }
}

void AgentService::flush( CacheEntry** c_entry_list ){
    //sort c_entry by offset
    std::map<uint64_t, CacheEntry*> tmp_sort;
    for(uint64_t i = 0; c_entry_list[i]; i++){
        if ( !c_entry_list[i]->is_null() ) {
            tmp_sort.insert( std::make_pair( c_entry_list[i]->get_offset(), c_entry_list[i] ) );
        }
    }

    ssize_t tmp_sort_size = tmp_sort.size();
    CacheEntry** c_entry_list_sort = (CacheEntry**)cct->mempool->malloc( sizeof(CacheEntry*) * (tmp_sort_size+1) );
    uint64_t j = 0;
    uint64_t completed_flush = 0;
    uint8_t completed_flush_ratio = 0;
    for( std::map<uint64_t, CacheEntry*>::iterator it = tmp_sort.begin(); it!=tmp_sort.end(); it++ ){
        c_entry_list_sort[j++] = it->second;
    }

    log_print("AgentService::flush start\n");
    log_print("AgentService::flush waiting %lu c_entry finish doing flush\n", j);

    for(uint64_t i = 0; c_entry_list_sort[i]!=0; i++){
        if( completed_flush * 100 / j > completed_flush_ratio ) {
            completed_flush_ratio++;
            log_print("AgentService::flush completed ratio=%d %\n", completed_flush*100/j);
        }

        while(cct->flush_op_count >= cct->cache_flush_queue_depth){
            std::unique_lock<std::mutex> flush_qd_scoped_lock(cct->flush_qd_mutex);
            cct->flush_qd_cond.wait_for(flush_qd_scoped_lock, std::chrono::milliseconds(1000));
        }

        CacheEntry* c_entry = c_entry_list_sort[i];

        do_flush( c_entry );

        completed_flush++;
    }
    tmp_sort.clear();
    cct->mempool->free( (void*)c_entry_list_sort, sizeof(CacheEntry*) * (tmp_sort_size+1) );
    log_print("AgentService::flush complete\n");
    return;
}

void AgentService::flush_all(){
    //should scan on all caches
    return flush_by_ratio(0);
}

void AgentService::do_evict( CacheEntry* c_entry ){
    WriteLock cache_entry_w_lock(c_entry->rwlock);
    if( c_entry == NULL || c_entry->is_null() ){
        return;
    }
    if( c_entry->get_if_dirty() ){
        return;
    }
    uint64_t cache_id = c_entry->get_offset() / cct->object_size;
    cct->metastore->remove( c_entry->get_name() );
    //TODO: fix when racing with write/read
    c_entry->reset();
    cct->cacher->_remove( cache_id );
    cct->lru_clean->remove( (char*)c_entry );
}

void AgentService::evict( CacheEntry** c_entry_list ){
    log_print("AgentService::evict start\n");
    uint64_t i = 0;
    for(;c_entry_list[i]!=0; i++){
        CacheEntry* c_entry = c_entry_list[i];
        threadpool->schedule(boost::bind(&AgentService::do_evict, this, c_entry));
    }
    log_print("AgentService::evict waiting for %lu c_entry finish doing evict\n", i);
    threadpool->wait();
    log_print("AgentService::evict complete\n");
    return;
}

void AgentService::flush_by_ratio( float target_ratio = 1.0 ){
    if(target_ratio == 1.0) {
        target_ratio = cache_dirty_ratio_min;
    }

    uint64_t dirty_block_count = cct->lru_dirty->get_length();
    uint64_t total_block_count = cache_total_size / object_size;

    log_print( "AgentService::flush_by_ratio: dirty_ratio:%2.4f \n", ( 1.0*dirty_block_count/total_block_count ) );

    if( ( 1.0 * dirty_block_count/total_block_count ) < target_ratio ){
        return;
    } else {
        uint64_t need_to_flush_count = dirty_block_count - total_block_count * target_ratio;
        char** c_entry_list = (char**)cct->mempool->malloc( sizeof(char*) * (need_to_flush_count+1) );
        cct->lru_dirty->get_keys( &c_entry_list[0], need_to_flush_count, false );
        flush( (CacheEntry**)c_entry_list );

        cct->mempool->free( (void*)c_entry_list, sizeof(char*) * (need_to_flush_count+1) );
    }
    return;
}

void AgentService::evict_by_ratio(){
    uint64_t dirty_block_count = cct->lru_dirty->get_length();
    uint64_t clean_block_count = cct->lru_clean->get_length();
    uint64_t total_cached_block = dirty_block_count + clean_block_count;
    uint64_t total_block_count = cache_total_size / object_size;

    log_print( "AgentService::evict_by_ratio:  current cache ratio:%2.4f \n", ( 1.0*total_cached_block/total_block_count ) );
    if( (1.0 * total_cached_block/total_block_count) < cache_ratio_max ){
        log_print("AgentService::evict_by_ratio: cache ratio is less than cache_ratio_max, no need do evict this time.\n");
        return;
    }

    WriteLock w_lock(cct->cachemap_access);
    // check if we have enough clean cached blocks
    uint64_t need_to_evict_count = total_cached_block - cache_ratio_health * total_block_count;
    if (need_to_evict_count > clean_block_count) {

        flush_by_ratio(cache_ratio_health);

    }

    log_print("AgentService::evict_by_ratio: need to evict %ld entry\n", need_to_evict_count);
    char** c_entry_list = (char**)cct->mempool->malloc( sizeof(char*) * (need_to_evict_count+1) );
    log_print("lru_clean length %ld\n", cct->lru_clean->get_length());
    cct->lru_clean->get_keys( &c_entry_list[0], need_to_evict_count, false );
    evict( (CacheEntry**)c_entry_list );
    cct->mempool->free( (void*)c_entry_list, sizeof(char*) * (need_to_evict_count+1) );

}

void AgentService::_process_flush(){
    while(cct->go && cct->working_mode == "cache"){
        sleep( cache_flush_interval );
        if(cct->go){
            flush_by_ratio();
            cct->mempool->print_total();
            std::this_thread::yield();
        }
    }
    log_print("AgentService::_process_flush complete\n");
}

void AgentService::_process_evict(){
    while(cct->go && cct->working_mode == "cache"){
        sleep( cache_evict_interval );
            if(cct->go){
            evict_by_ratio();
            std::this_thread::yield();
        }
    }
    log_print("AgentService::_process_evict complete\n");
}
}
