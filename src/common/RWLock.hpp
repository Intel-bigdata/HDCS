#ifndef RW_LOCK_HPP
#define RW_LOCK_HPP

#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace rbc {

typedef boost::shared_mutex smutex;
typedef boost::unique_lock< smutex > WriteLock;
typedef boost::shared_lock< smutex > ReadLock;

/*
smutex myLock;


void ReadFunction()
{
    ReadLock r_lock(myLock);
    //Do reader stuff
}

void WriteFunction()
{
     WriteLock w_lock(myLock);
     //Do writer stuff
}

*/

}
#endif //LOCK_GUARD_HPP
