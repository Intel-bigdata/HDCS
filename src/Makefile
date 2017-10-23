LIBRBC_VERSION = 0.1.0

CC=cc
CXX=g++
PLATFORM=OS_LINUX
OPTIMIZATION?=-O2
OPT=$(OPTIMIZATION)
CXXFLAGS=-std=c++11 ${OPT} -fPIC -g -D__STDC_FORMAT_MACROS -DLIBRBC_VERSION='"${LIBRBC_VERSION}"'
CCFLAGS=${OPT} -fPIC -g -D__STDC_FORMAT_MACROS -DLIBRBC_VERSION='"${LIBRBC_VERSION}"'
PLATFORM_LDFLAGS=-lrocksdb -lpthread -lboost_thread -lboost_system -lboost_serialization -lrados -lrbd -lrt
INCS=-I./include  -I../unittest/include/

DIST_LIB = librbc.so
DIST_PROCESS = librbc_process.so
MAIN = hyperstash

.PHONY : all
all: $(DIST_LIB)

%.o : %.cpp
	${CXX} -c ${CXXFLAGS} ${INCS} $< -o $@

%.o : %.c
	${CC} -c ${CCFLAGS} ${INCS} $< -o $@

    
COMMON_OBJECTS:= CacheService.o CacheEntry.o MetaStore.o AgentService.o BackendStore.o SimpleBlockCacher.o

TEST_OBJECTS:= ../unittest/test_common/test_CacheMap.o ../unittest/test_common/test_AllocateMap.o ../unittest/test_common/test_WorkQueue.o ../unittest/test_common/test_MemoryUsageTracker.o

$(MAIN): $(COMMON_OBJECTS)
	cp librbc_process.so librbc.so
	${CXX} ${CXXFLAGS} ${INCS} main.cpp -o $@ $^ ${PLATFORM_LDFLAGS} librbc.so

$(DIST_PROCESS):
	${CXX} ${CXXFLAGS} ${INCS} librbc_client.cpp -shared -o $@ $^ ${PLATFORM_LDFLAGS}

$(DIST_LIB): $(COMMON_OBJECTS)
	${CXX} ${CXXFLAGS} ${INCS} librbc.cpp -shared -o $@ $^ ${PLATFORM_LDFLAGS}

process: $(DIST_PROCESS) $(MAIN)

test: $(TEST_OBJECTS)
	${CXX} ${CXXFLAGS} ${INCS} ../unittest/test_common/testrun.cpp -o $@ $^ ../unittest/lib/libgmock.a -pthread
	echo "./test" | bash


install: $(DIST_LIB)
	cp librbc.so /usr/lib/librbc.so
	cp librbc.so /usr/local/lib/librbc.so
	cp -r include/rbc /usr/include/
	cp -r include/rbc /usr/local/include/

install-process: process $(DIST_PROCESS)
	cp librbc_process.so /usr/lib/librbc.so
	cp librbc_process.so /usr/local/lib/librbc.so
	cp hyperstash /usr/local/bin
	cp -r include/rbc /usr/include/
	cp -r include/rbc /usr/local/include/

uninstall:
	rm -rf /usr/local/bin/hyperstash
	rm -rf /usr/lib/librbc.so
	rm -rf /usr/local/lib/librbc.so
	rm -rf /usr/local/include/rbc
	rm -rf /usr/include/rbc

clean:
	rm -rf *.o *.so test bench ../unittest/test_common/*.o include/rbc/HttpHandler/*.o hyperstash include/rbc/mqueueMessenger/*.o include/rbc/common/*.o
