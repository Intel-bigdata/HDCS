
INCS=-I../rdma_common/rdma_messenger/include -lboost_system -lpthread -fPIC -lrdmacm -libverbs
SRC=../rdma_common/rdma_messenger/src/core/

all: Acceptor.o Connector.o RDMAConnection.o RDMAStack.o
	#g++ -std=c++11 -pthread -I./inlcude -L./usr/lib64  main_client.cpp  -lboost_system -lboost_serialization -lboost_thread -o test_client $(INCS) $(SRC)Acceptor.o $(SRC)RDMAConnection.o $(SRC)RDMAConnection.o
	g++ -std=c++14 -g main_client.cpp -o test_client $(SRC)Acceptor.o $(SRC)RDMAConnection.o $(SRC)Connector.o $(SRC)RDMAStack.o $(INCS)
	g++ -std=c++14 -g  main_server.cpp -o test_server $(SRC)Acceptor.o $(SRC)RDMAConnection.o $(SRC)Connector.o $(SRC)RDMAStack.o $(INCS)
	#g++ -std=c++1y -pthread -I./inlcude -L./usr/lib64  main_thread_group.cpp  -lboost_system -lboost_serialization -lboost_thread -o threadpool $(INCS)

Acceptor.o:
	g++ -std=c++14 -g -c $(SRC)Acceptor.cc -o $(SRC)Acceptor.o $(INCS)
Connector.o:
	g++ -std=c++14 -g -c $(SRC)Connector.cc -o $(SRC)Connector.o $(INCS)
RDMAStack.o:
	g++ -std=c++14 -g -c $(SRC)RDMAStack.cc -o $(SRC)RDMAStack.o $(INCS)
RDMAConnection.o:
	g++ -std=c++14 -g -c $(SRC)RDMAConnection.cc -o $(SRC)RDMAConnection.o $(INCS)


clean:
	rm -f $(SRC)*.o 
	rm -f test_*
