#ifndef LIST_H
#define LIST_H

#include <mutex>
#include <vector>

#include "Log.h"
#include "Mempool.h"

namespace rbc{
class List{
public:
    List( Mempool *mempool = NULL ):mempool(mempool){
        hold_items_count = 0;
        list_size = list.size();
    }
    ~List(){
        mempool->rm( "List", list_size );
    }
    void reset(){
        list.clear();

        mempool->update( "List", list_size, list.size() );
        list_size = list.size();
    }
    std::vector<bool>::iterator insert(){
        int64_t i = 0;
        list_lock.lock();
        std::vector<bool>::iterator it;
        it = list.insert( list.end(), true );
        mempool->update( "List", list_size, list.size() );
        list_size = list.size();
        hold_items_count++;
        list_lock.unlock();
        return it;
    }
    int16_t remove( std::vector<bool>::iterator it ){
        list_lock.lock();
        list.erase( it );
        mempool->update( "List", list_size, list.size() );
        list_size = list.size();
        int16_t ret = --hold_items_count;
        list_lock.unlock();
        return ret;
    }

    int16_t get_size(){
        int16_t ret = hold_items_count;
        return ret;
    }
private:
    std::vector<bool> list;
    std::mutex list_lock;
    int16_t hold_items_count;
    ssize_t list_size;
    Mempool *mempool;

};

#endif
}
