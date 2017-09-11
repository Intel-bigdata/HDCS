#include "rbc/CacheService.h"
#include "rbc/common/Config.h"

namespace rbc {

CacheService::CacheService(const char* rbd_name, bool process_mode, bool if_master){
    cct = new Context(rbd_name, process_mode, if_master);
    int tp_size = stoi(cct->config->configValues["cacheservice_threads_num"]);
    threadpool = new ThreadPool(tp_size);
    process_tp = new ThreadPool(1);
    process_tp->schedule(boost::bind(&CacheService::_process, this));
    agentservice = new AgentService( cct );
    log_print("load meta\n");

    cct->cachemap_lock.lock();
    cct->metastore->get_all( cct->cache_map, cct->lru_dirty, cct->lru_clean, cct->mempool );
    cct->cachemap_lock.unlock();

    log_print("CacheService constructed\n");
}

CacheService::~CacheService(){
    cct->go = false;
    cct->request_queue.wake_all();
    delete threadpool;
    delete process_tp;
    delete agentservice;
    delete cct;
}

int CacheService::queue_io( Request* req ){
    cct->request_queue.enqueue((void*)req);
    return 0;
}

int CacheService::do_flush_op(Op* op) {
    //TODO:
    agentservice->flush_all();
    return 0;
}

int CacheService::do_discard_op(Op* op) {
    //TODO:
    agentservice->do_evict(op->cache_entry);
    return 0;
}

void CacheService::_process(){
    while(cct->go){
        dequeue_op();
    };
}

void CacheService::dequeue_op() {
    Request *req = (Request*)cct->request_queue.dequeue();
    if( NULL == req ) {
        return;
    }
    //std::cerr << "dequeue_op offset: " << req->msg->header.offset << std::endl;
    std::list<Op*> op_list = map_request_by_entry( req );
    req->uncomplete_count = op_list.size();
    if (1 != op_list.size()) {
        log_print("[WARN]CacheService:: op_list size %d != 1, req = %p\n", op_list.size(), req);
    }
    for( std::list<Op*>::iterator it = op_list.begin(); it != op_list.end(); ++it ){
        threadpool->schedule(boost::bind(&CacheService::do_op, this, *it));
    }
}

void CacheService::do_op(Op *op){
    if (!op->op_lock.try_lock()) {
        log_print("[WARN]CacheService::data races on op %p\n", op);
        return;
    }
    ReadLock r_lock(cct->cachemap_access);

    if (MSG_FLUSH == op->req->msg->header.type) {
            do_flush_op(op);
            return;
    }

    std::string oid_string = get_index(op->image_name, op->offset);

    cct->cachemap_lock.lock();
    CacheEntry* cache_entry = (CacheEntry*)cct->cache_map->find_key( oid_string );
    if( cache_entry == NULL ){
        cache_entry = new CacheEntry( cct->mempool );
        cct->cache_map->insert( oid_string, (char*)cache_entry );
    }
    cct->cachemap_lock.unlock();

    if( cache_entry->is_null() ) {
        cache_entry->init( cct->object_size, oid_string, op->image_name, op->offset );
    }

    op->cache_entry = cache_entry;
    switch( op->req->msg->header.type ){
        case MSG_READ:
            do_read_op(op);
            break;
        case MSG_WRITE:
            do_write_op(op);
            break;
        case MSG_DISCARD:
            do_discard_op(op);
            break;
        default:
            break;
    }
}

std::string CacheService::get_index( std::string location_id, uint64_t offset ){
    std::ostringstream ss;
    uint64_t start_off = offset/cct->object_size;
    ss << location_id << "-" << start_off;
    return ss.str();
}

std::list<Op*> CacheService::map_request_by_entry( Request* req ){
    std::list<Op*> op_list;
    uint64_t cur_offset = req->msg->header.offset;
    uint64_t cur_offset_by_req = 0;
    uint64_t remain_length = req->msg->header.content_length;
    uint64_t max_length = cct->object_size;
    uint64_t op_len = 0;

    if ( 0 == remain_length && MSG_FLUSH == req->msg->header.type ) {
        //shortcut for flush request
        op_list.push_back(nullptr);
        return op_list;
    }

    while( remain_length > 0 ){
        if(cur_offset % cct->object_size > 0)
            max_length = cct->object_size - cur_offset % cct->object_size;
        else
            max_length = cct->object_size;

        if( remain_length < max_length )
            op_len = remain_length;
        else
            op_len = max_length;
        std::string image_name(req->msg->get_location_id());
        Op *new_op;
        new_op = new Op( cct, image_name, cur_offset, &req->msg->content[cur_offset_by_req], op_len, cct->mempool, req);
        op_list.push_back(new_op);
        remain_length -= op_len;
        cur_offset += op_len;
        cur_offset_by_req += op_len;
    }

    return op_list;
}

int CacheService::do_read_op(Op *op){
    std::string oid_string = get_index( op->image_name, op->offset );
    const char* oid = oid_string.c_str();
    ssize_t ret = 0;

    if( cache_lookup(op, oid) ){
        ret = read_hit( op );
    }else{
        ret = read_miss( op );
    }
    return ret;
}

int CacheService::do_write_op( Op *op ){

    ssize_t ret = 0;

    //if (std::difftime(op->cache_entry->get_ts(), op->ts)) {
    if (op->ts < op->cache_entry->get_ts()) {
        /*log_print("CacheService::[WARN] unorderd write sequence, ");
        std::cerr << "entry->ts: " << op->cache_entry->get_ts() << "op->ts: " << op->ts << std::endl;*/
        op->commit(ret);
        delete op;
        return ret;
    }
    //for replica write
    if(cct->if_master){
        int replica_num = std::stoi(cct->config->configValues["replication_num"]);
        for( int i=0; i<replica_num; i++){
            Op* replica_op = new Op(cct, op->image_name, op->offset,
                            op->data, op->length, cct->mempool, op->req );
            C_AioReplicationCompletion *onfinish = new C_AioReplicationCompletion(replica_op);
            replica_op->replica_aio_write((AioCompletetion*)onfinish, i);

        }
        /*
        Op* replica_op = new Op(cct, op->image_name, op->offset,
                            op->data, op->length, cct->mempool, op->req );
        C_AioReplicationCompletion *onfinish = new C_AioReplicationCompletion(replica_op);
        replica_op->replica_aio_write((AioCompletion*)onfinish,0);
        // 3 replication
        Op* replica_op_1 = new Op(cct, op->image_name, op->offset,
                            op->data, op->length, cct->mempool, op->req );
        C_AioReplicationCompletion *onfinish_1 = new C_AioReplicationCompletion(replica_op_1); 
        replica_op_1->replica_aio_write((AioCompletion*)onfinish_1,1);
        */
    }

    std::string oid_string = get_index( op->image_name, op->offset );
    const char* oid = oid_string.c_str();

    if( cache_lookup(op, oid) ){
        ret = write_hit( op );
    }else{
        ret = write_miss( op );
    }

    return ret;
}

bool CacheService::cache_lookup( Op* op, const char* oid){
    ReadLock cache_entry_r_lock(op->cache_entry->rwlock);
    if( !op->cache_entry->data_map_lookup(op->offset%cct->object_size, op->length) ){
        return false;
    }
    return true;
}

/*
int CacheService::update_write_meta(Op* op) {

        int ret = 0;
        WriteLock cache_entry_w_lock(op->cache_entry->rwlock);
        if (op->cache_entry->is_null() || op->ts < op->cache_entry->get_ts()) {
            //entry has been reset by later evict op
            //or entry has been updated by later write op
            return ret;
        }
        op->cache_entry->data_map_update( op->offset % cct->object_size, op->length );

        cct->lru_dirty->touch_key( (char*)op->cache_entry );
        cct->lru_clean->remove((char*)op->cache_entry);
        op->cache_entry->set_cache_dirty();

        ret = op->metastore_update();
        assert(ret == 0);

        return ret;

}
*/

int CacheService::write_hit( Op *op ){
    ssize_t ret = op->datastore_write();
    if( ret == 0 ){

        WriteLock cache_entry_w_lock(op->cache_entry->rwlock);
        op->cache_entry->data_map_update( op->offset % cct->object_size, op->length );

        cct->lru_dirty->touch_key( (char*)op->cache_entry );
        cct->lru_clean->remove((char*)op->cache_entry);
        op->cache_entry->set_cache_dirty();

        ret = op->metastore_update();
        assert(ret == 0);

    }
    op->commit(ret);
    delete op;
    return ret;
}

ssize_t CacheService::read_hit( Op *op ){
    ssize_t ret = op->datastore_read();
    if( ret >= 0 ){

        ReadLock cache_entry_r_lock(op->cache_entry->rwlock);
        if(op->cache_entry->get_if_dirty())
            cct->lru_dirty->touch_key( (char*)op->cache_entry );
        else
            cct->lru_clean->touch_key( (char*)op->cache_entry );
    }
    op->commit(ret);
    delete op;
    return ret;
}

int CacheService::do_promote(Op *orig_op) {
    char* promote_data = (char*)cct->mempool->malloc( sizeof(char) * cct->object_size );
    Op* promote_op = new Op(cct, orig_op->image_name, orig_op->cache_entry->get_offset(),
                            promote_data, cct->object_size, cct->mempool, orig_op->req );
    ssize_t ret = promote_op->backstore_read();
    if( ret < 0 ){
        cct->mempool->free( (void*)promote_data, sizeof(char) * cct->object_size );
        return ret;
    }
    assert( ret == promote_op->length );
    memcpy( &promote_op->data[orig_op->offset % cct->object_size], orig_op->data, orig_op->length );

    orig_op->data = promote_op->data;
    orig_op->offset = promote_op->offset;
    orig_op->length = promote_op->length;
    cct->mempool->free( (void*)promote_data, sizeof(char) * cct->object_size );
    return ret;
}

int CacheService::write_miss( Op *op ){
    ssize_t ret;
    if( op->length < cct->object_size ){
        ret = do_promote(op);
        if (ret < 0) {
            goto finish;
        }
    }
    ret = op->datastore_write();
    if( ret == 0 ){
        WriteLock cache_entry_w_lock(op->cache_entry->rwlock);
        op->cache_entry->data_map_update( op->offset % cct->object_size, op->length );

        cct->lru_dirty->touch_key( (char*)op->cache_entry );
        cct->lru_clean->remove((char*)op->cache_entry);
        op->cache_entry->set_cache_dirty();

        ret = op->metastore_update();
        assert(ret == 0);

    }
finish:
    op->commit(ret);
    delete op;
    return ret;
}

ssize_t CacheService::read_miss( Op* op ){
    ssize_t data_size = do_promote(op);
    int ret = 0;
    if (data_size < 0) {
        goto finish;
    }
    ret = op->datastore_write();
    if( ret == 0 ){

        ReadLock cache_entry_r_lock(op->cache_entry->rwlock);
        op->cache_entry->data_map_update( op->offset % cct->object_size, op->length );
        // must be clean here?
        if(op->cache_entry->get_if_dirty())
            cct->lru_dirty->touch_key( (char*)op->cache_entry );
        else
            cct->lru_clean->touch_key( (char*)op->cache_entry );

        ret = op->metastore_update();
        assert(ret == 0);
    }

finish:
    op->commit(data_size);
    delete op;
    return ret;
}
}
