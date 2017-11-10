test:
	#g++ test.cpp -std=c++1y -lboost_thread -lboost_system -pthread 
	g++ -std=c++11 -pthread -I./inlcude -L./usr/lib64  test.cpp  -lboost_system -lboost_filesystem -lboost_serialization -lboost_thread -o test
#	g++ client1.cpp  -std=c++11 -lboost_thread -lboost_system -pthread -o client
client:
	g++ -std=c++11 -pthread -I./inlcude -L./usr/lib64  main_client.cpp  -lboost_system -lboost_filesystem -lboost_serialization -lboost_thread -o client
server:
	g++ -std=c++1y -pthread -I./inlcude -L./usr/lib64  main_server.cpp  -lboost_system -lboost_filesystem -lboost_serialization -lboost_thread -o server

clean:
	rm client server test -f
kill:
	ps -axu | grep server
