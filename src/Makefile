LIBRBC_VERSION = 1.5.0

CC=cc
CXX=g++
PLATFORM=OS_LINUX
OPTIMIZATION?=-O2 -Wno-unused-result
OPT=$(OPTIMIZATION)
CXXFLAGS=-std=c++11 ${OPT} -fPIC -g -D__STDC_FORMAT_MACROS -DLIBRBC_VERSION='"${LIBRBC_VERSION}"'
CCFLAGS=${OPT} -fPIC -g -D__STDC_FORMAT_MACROS -DLIBRBC_VERSION='"${LIBRBC_VERSION}"'
PLATFORM_LDFLAGS=-lboost_thread -lboost_system -lpthread -lrbd -lrados -lhlkvds
INCS=-I./

DIST_LIB = libhdcs.so
#POLICY = CACHE
POLICY = TIER
.PHONY : all
all: $(DIST_LIB)

%.o : %.cpp
	${CXX} -c ${CXXFLAGS} ${INCS} $< -o $@

%.o : %.c
	${CC} -c ${CCFLAGS} ${INCS} $< -o $@

COMMON_OBJECTS:= store/SimpleStore/SimpleBlockStore.o  store/KVStore/KVStore.o

RBD_OBJECTS:= store/RBD/RBDImageStore.o 

HDCSCore_cache.oo : core/HDCSCore.cpp
	${CXX} -c ${CXXFLAGS} -DCACHE_POLICY ${INCS} $< -o $@

HDCSCore_tier.oo : core/HDCSCore.cpp
	${CXX} -c ${CXXFLAGS} -DTIER_POLICY ${INCS} $< -o $@

CACHE_CORE_OBJECTS := HDCSCore_cache.oo
TIER_CORE_OBJECTS := HDCSCore_tier.oo

ifeq ($(POLICY),CACHE)
$(DIST_LIB): core/policy/CachePolicy.o $(COMMON_OBJECTS) $(RBD_OBJECTS) $(CACHE_CORE_OBJECTS)
	${CXX} ${CXXFLAGS} ${INCS} libhdcs.c -shared -o $@ $^ ${PLATFORM_LDFLAGS}
else
$(DIST_LIB): core/policy/TierPolicy.o $(COMMON_OBJECTS) $(RBD_OBJECTS) $(TIER_CORE_OBJECTS)
	${CXX} ${CXXFLAGS} ${INCS} libhdcs.c -shared -o $@ $^ ${PLATFORM_LDFLAGS}
endif
clean:
	rm -rf common/*.o core/*.o store/SimpleStore/*.o store/KVStore/*.o store/RBD/*.o core/policy/*.o *.oo libhdcs.so

install:
	cp -r include/ /usr/local/include/hdcs
	cp libhdcs.so /usr/local/lib/

uninstall:
	rm -rf /usr/local/include/hdcs
	rm -rf /usr/local/lib/libhdcs.so
