LIBRBC_VERSION = 1.5.0

CC=cc
CXX=g++
PLATFORM=OS_LINUX
OPTIMIZATION?=-O2 -Wno-unused-result
OPT=$(OPTIMIZATION)
CXXFLAGS=-std=c++14 ${OPT} -fPIC -g -D__STDC_FORMAT_MACROS -DLIBRBC_VERSION='"${LIBRBC_VERSION}"'
CCFLAGS=${OPT} -fPIC -g -D__STDC_FORMAT_MACROS -DLIBRBC_VERSION='"${LIBRBC_VERSION}"'
PLATFORM_LDFLAGS=-lboost_thread -lboost_system -lpthread -lrbd -lrados
INCS=-I./
CONTROLLER = HDCS
DIST_LIB = lib/libhdcs.so
TEST_TOOL = hdcs_test_tool
#POLICY = CACHE
#POLICY = TIER
POLICY = ALL
.PHONY : all
all: $(DIST_LIB) $(CONTROLLER) $(TEST_TOOL)

%.o : %.cpp
	${CXX} -c ${CXXFLAGS} ${INCS} $< -o $@

%.o : %.c
	${CC} -c ${CCFLAGS} ${INCS} $< -o $@

COMMON_OBJECTS:= store/SimpleStore/SimpleBlockStore.o 

RBD_OBJECTS:= store/RBD/RBDImageStore.o 

#NETWORK_OBJECTS:= Network/io_pool.o

HDCSCore_cache.oo : core/HDCSCore.cpp
	${CXX} -c ${CXXFLAGS} -DCACHE_POLICY ${INCS} $< -o $@

HDCSCore_tier.oo : core/HDCSCore.cpp
	${CXX} -c ${CXXFLAGS} -DTIER_POLICY ${INCS} $< -o $@

HDCSCore.oo : core/HDCSCore.cpp
	${CXX} -c ${CXXFLAGS} -DCACHE_POLICY -DTIER_POLICY ${INCS} $< -o $@

HDCSController.oo :
	${CXX} ${CXXFLAGS} -DCACHE_POLICY -DTIER_POLICY ${INCS} core/HDCSCore.cpp HDCSController.cpp -o $@ $^ ${PLATFORM_LDFLAGS}

CACHE_CORE_OBJECTS := HDCSCore_cache.oo
TIER_CORE_OBJECTS := HDCSCore_tier.oo
HDCS_CORE_OBJECTS := HDCSCore.oo
HDCS_CONTROLLER_OBJECTS := HDCSController.oo

ifeq ($(POLICY),CACHE)
$(DIST_LIB): core/policy/CachePolicy.o $(COMMON_OBJECTS) $(RBD_OBJECTS) $(CACHE_CORE_OBJECTS)
	mkdir -p lib
	${CXX} ${CXXFLAGS} ${INCS} libhdcs.c -shared -o $@ $^ ${PLATFORM_LDFLAGS}
endif
ifeq ($(POLICY), TIER)
$(DIST_LIB): core/policy/TierPolicy.o $(COMMON_OBJECTS) $(RBD_OBJECTS) $(TIER_CORE_OBJECTS)
	mkdir -p lib
	${CXX} ${CXXFLAGS} ${INCS} libhdcs.c -shared -o $@ $^ ${PLATFORM_LDFLAGS}
endif

ifeq ($(POLICY), ALL)
#$(CONTROLLER): core/policy/CachePolicy.o core/policy/TierPolicy.o $(COMMON_OBJECTS) $(RBD_OBJECTS) $(NETWORK_OBJECTS)
$(CONTROLLER): core/policy/CachePolicy.o core/policy/TierPolicy.o $(COMMON_OBJECTS) $(RBD_OBJECTS) 
	${CXX} ${CXXFLAGS} ${INCS} -DCACHE_POLICY -DTIER_POLICY  core/HDCSCore.cpp HDCSController.cpp HDCS.cpp -o $@ $^ ${PLATFORM_LDFLAGS}
endif

$(DIST_LIB):
	mkdir -p lib
	${CXX} ${CXXFLAGS} ${INCS} libhdcs.c -shared -o $@ $^ ${PLATFORM_LDFLAGS}

$(TEST_TOOL):
	${CXX} ${CXXFLAGS} ${INCS} test.c -o $@ $^ -Llib -lhdcs

clean:
	rm -rf *.oo common/*.o core/*.o core/*.oo store/SimpleStore/*.o store/RBD/*.o core/policy/*.o Network/*.o lib/libhdcs.so hdcs_test_tool HDCS
	rm -rf /usr/local/include/hdcs
	rm -rf /usr/local/lib/libhdcs.so

clean_test:
	rm hdcs_test_tool

install:
	cp -r include/ /usr/local/include/hdcs
	cp lib/libhdcs.so /usr/local/lib/
	cp HDCS /usr/local/bin/
