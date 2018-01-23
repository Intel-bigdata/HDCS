LIBRBC_VERSION = 1.3.0

CC=cc
CXX=g++
PLATFORM=OS_LINUX
OPTIMIZATION?=-O2 -flto
OPT=$(OPTIMIZATION)
CXXFLAGS=-std=c++11 ${OPT} -fPIC -g -D__STDC_FORMAT_MACROS -DLIBRBC_VERSION='"${LIBRBC_VERSION}"'
CCFLAGS=${OPT} -fPIC -g -D__STDC_FORMAT_MACROS -DLIBRBC_VERSION='"${LIBRBC_VERSION}"'
#PLATFORM_LDFLAGS=-lboost_thread -lboost_system -lpthread -lrbd -lrados
PLATFORM_LDFLAGS=-lboost_thread -lboost_system -lboost_program_options -lpthread -lrdmacm -libverbs
INCS=-I./ -I../unittest/include -I./Network/rdma_common/rdma_messenger/include
SRC = ./Network/rdma_common/rdma_messenger/src/core/
CONTROLLER = HDCS
DIST_LIB = lib/libhdcs.so
TEST_TOOL = hdcs_test_tool
POLICY = ALL
.PHONY : all
all: RDMA $(DIST_LIB) $(CONTROLLER) $(TEST_TOOL)

%.o : %.cpp
	${CXX} -c ${CXXFLAGS} ${INCS} $< -o $@

%.o : %.c
	${CC} -c ${CCFLAGS} ${INCS} $< -o $@

COMMON_OBJECTS:= store/SimpleStore/SimpleBlockStore.o core/policy/CachePolicy.o core/policy/TierPolicy.o core/HDCSCore.o HDCSController.o
LIB_OBJECTS:= store/SimpleStore/SimpleBlockStore.o core/policy/CachePolicy.o core/policy/TierPolicy.o core/HDCSCore.o
RDMA_OBJECT= Network/rdma_common/rdma_messenger/src/core/RDMAStack.o Network/rdma_common/rdma_messenger/src/core/RDMAConnection.o Network/rdma_common/rdma_messenger/src/core/Acceptor.o Network/rdma_common/rdma_messenger/src/core/Connector.o 
RBD_OBJECTS:= store/RBD/RBDImageStore.o 

RDMA:
	$(CXX) $(CXXFLAGS) $(INCS) $(SRC)Acceptor.cc -c -o $(SRC)Acceptor.o $(PLATFORM_LDFAGS) 
	$(CXX) $(CXXFLAGS) $(INCS) $(SRC)Connector.cc -c -o $(SRC)Connector.o $(PLATFORM_LDFAGS)
	$(CXX) $(CXXFLAGS) $(INCS) $(SRC)RDMAStack.cc -c -o $(SRC)RDMAStack.o $(PLATFORM_LDFAGS)
	$(CXX) $(CXXFLAGS) $(INCS) $(SRC)RDMAConnection.cc -c -o $(SRC)RDMAConnection.o $(PLATFORM_LDFAGS)

$(CONTROLLER): $(COMMON_OBJECTS) $(RBD_OBJECTS)
	${CXX} ${CXXFLAGS} ${INCS} HDCS.cpp -o $@ $^ $(RDMA_OBJECT) ${PLATFORM_LDFLAGS}

$(DIST_LIB): $(LIB_OBJECTS) $(RBD_OBJECTS)
	mkdir -p lib
	${CXX} ${CXXFLAGS} ${INCS} libhdcs.cpp -shared -o $@ $^ $(RDMA_OBJECT) ${PLATFORM_LDFLAGS} -fPIC

$(TEST_TOOL): $(DIST_LIB)
	${CXX} ${CXXFLAGS} ${INCS} test.c -o $@ $^ -Llib -lhdcs $(RDMA_OBJECT)

UNIT_TESTS:= ../unittest/test_common/test_WorkQueue.o \
	     ../unittest/test_common/test_Config.o

test: $(UNIT_TESTS)
	${CXX} ${CXXFLAGS} ${INCS} ../unittest/testrun.cpp -o $@ $^ ../unittest/lib/libgmock.a -pthread
	echo "./test --gtest_output='xml:testresult.xml'" | bash

clean:
	rm -f test testresult.xml $(CONTROLLER) $(DIST_LIB) $(TEST_TOOL) $(UNIT_TESTS) && find . -name "*.o" | xargs rm -f
	rm -f $(SRC)*.o

uninstall:
	rm -rf /usr/local/include/hdcs
	rm -rf /usr/local/lib/libhdcs.so

clean_test:
	rm hdcs_test_tool

install:
	cp -r include/ /usr/local/include/hdcs
	cp lib/libhdcs.so /usr/local/lib/
	cp HDCS /usr/local/bin/
