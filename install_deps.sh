#!/bin/sh

DEPS_DIR=`pwd`/deps
DEPS_SRC_DIR=$DEPS_DIR/src
DEPS_UT_DIR=`pwd`/unittest
DEPS_UT_SRC_DIR=$DEPS_UT_DIR/include
DEPS_UT_LIB_DIR=$DEPS_UT_DIR/lib

# Create deps folder
mkdir -p $DEPS_SRC_DIR
mkdir -p $DEPS_UT_SRC_DIR
mkdir -p $DEPS_UT_LIB_DIR

# CPP_Redis
cd $DEPS_SRC_DIR
## Fetch the cppredis sources
git clone https://github.com/intel-bigdata/hlkvds.git
cd hlkvds; git checkout origin/master; make && make install


# CPP_Redis
cd $DEPS_SRC_DIR
## Fetch the cppredis sources
git clone https://github.com/zhouyuan/cpp_redis.git
cd cpp_redis; git checkout origin/build_shared_lib; cmake .; make && make install

# Rocksdb
cd $DEPS_SRC_DIR
## Fetch the rocksdb sources
git clone https://github.com/facebook/rocksdb.git
cd rocksdb; git checkout v4.8; make shared_lib && make install-shared

. /etc/os-release

if [ "$ID" = "ubuntu" ] || [ "$ID" = "debian" ]; then
    apt-get install -y libaio-dev libboost-all-dev
elif [ "$ID" = "centos" ] || [ "$ID" = "fedora" ]; then
    yum install -y libaio-devel boost-devel
fi

# Googletest
cd $DEPS_SRC_DIR
## Fetch the googletest sources
git clone https://github.com/google/googletest.git
cd googletest
g++ -isystem googletest/include/ -I googletest/ -isystem googlemock/include/ -I googlemock/ -pthread -c googletest/src/gtest-all.cc
g++ -isystem googletest/include/ -I googletest/ -isystem googlemock/include/ -I googlemock/ -pthread -c googlemock/src/gmock-all.cc
ar -rv libgmock.a gtest-all.o gmock-all.o

cp googletest/include/gtest $DEPS_UT_SRC_DIR -r
cp googlemock/include/gmock $DEPS_UT_SRC_DIR -r
cp libgmock.a $DEPS_UT_LIB_DIR -r
