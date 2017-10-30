#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdio.h>
#include "common/lib/boost/threadpool/threadpool.hpp"

namespace hdcs {
  typedef boost::threadpool::pool ThreadPool;
}

#endif
