#ifndef LRU_LINKLIST_H
#define LRU_LINKLIST_H

#include <mutex>
#include <boost/unordered_map.hpp>
#include <stdint.h>
#include <stdlib.h>

namespace hdcs{

template< typename key_type > class LRU_LIST{
private:
    struct LRU_LIST_NODE{
        typename boost::unordered::unordered_map<key_type, LRU_LIST_NODE*>::const_iterator _it;
        LRU_LIST_NODE* prev;
        LRU_LIST_NODE* next;
        LRU_LIST_NODE(){
            prev = NULL;
            next = NULL;
        }
        void init(typename boost::unordered::unordered_map<key_type, LRU_LIST_NODE*>::const_iterator it){
            _it = it;
        }
    };
    typedef boost::unordered::unordered_map<key_type, LRU_LIST_NODE*> EntryMap;
    EntryMap _key_to_point_map;

    LRU_LIST_NODE *head;
    LRU_LIST_NODE *tail;
    uint64_t length;
    ssize_t _key_to_point_map_size;
    std::mutex _lock;
public:
    LRU_LIST(){
        length = 0;
        head = NULL;
        tail = NULL;
    }
    ~LRU_LIST(){
        LRU_LIST_NODE *tmp = head;
        while( tmp!=NULL ){
            LRU_LIST_NODE *tmp_next = tmp->next;
            delete tmp;
            tmp = tmp->next;
        }
    }
    
    key_type get_head() {
      key_type ret;
      _lock.lock();
      ret = head->_it->first;
      _lock.unlock();
      return ret;
    }

    void get_keys( key_type* dst, uint64_t required_len = 0, bool most_recent_top = true ){
        if (0 == required_len) {
            // need to get keys
            return;
        }
        uint64_t count = 0;
        LRU_LIST_NODE* src;
        _lock.lock();
        if(most_recent_top){
            src = head;
            while(count < length){
                *dst++ = src->_it->first;
                src = src->next;
                count++;
                if(count==required_len)
                    break;
            }
        }else{
            src = tail;
            while(count < length){
                *dst++ = src->_it->first;
                src = src->prev;
                count++;
                if(count == required_len)
                    break;
            }
        }
        _lock.unlock();
        return;
    }

    void touch_key( const key_type& k ){
        LRU_LIST_NODE* node;
        _lock.lock();
        const typename EntryMap::iterator it = _key_to_point_map.find(k);
        if( it == _key_to_point_map.end() ){
            node = insert(k);
        }else{
            node = it->second;
            touch_key( node );
        }
        _lock.unlock();
    }

    void touch_key( LRU_LIST_NODE* key ){
        //fprintf(stderr, "touch_key %p\n", key);

        if( head == key ){
            return;
        }
        LRU_LIST_NODE* head_next = head;
        LRU_LIST_NODE* orig_prev = key->prev;
        LRU_LIST_NODE* orig_next = key->next;

        orig_prev->next = orig_next;
        if(orig_next)
            orig_next->prev = orig_prev;
        else
            tail = orig_prev;

        head = key;
        head->next = head_next;
        // reset head->prev to NULL
        head->prev = NULL;
        head_next->prev = head;
    }

    void remove( const key_type& k ){
        LRU_LIST_NODE* node;
        _lock.lock();
        const typename EntryMap::iterator it = _key_to_point_map.find(k);
        if( it != _key_to_point_map.end() ){
            remove( it->second );
        }
        _lock.unlock();
    }

    void remove( LRU_LIST_NODE* key ){

        LRU_LIST_NODE* orig_prev = key->prev;
        LRU_LIST_NODE* orig_next = key->next;

        if(head == key)
            head = orig_next;
        else
            orig_prev->next = orig_next;

        if(orig_next)
            orig_next->prev = orig_prev;
        else
            tail = orig_prev;

        length--;
        _key_to_point_map.erase( key->_it );
        delete key;
    }

    LRU_LIST_NODE* insert( const key_type& k ) {
        LRU_LIST_NODE* new_node = new LRU_LIST_NODE();
        typename EntryMap::iterator it = _key_to_point_map.insert( std::make_pair(k, new_node) ).first;
        _key_to_point_map_size = _key_to_point_map.size();
        new_node->init( it );

        LRU_LIST_NODE* head_next = head;
        head = new_node;
        head->next = head_next;

        if(head_next)
            head_next->prev = head;
        else
            tail = head;
        length++;
        //fprintf(stderr, "insert %p\n", new_node);
        return new_node;
    }

    uint64_t get_length(){
        return length;
    }

};


}

#endif
