bin=HttpServer
cc=g++ -std=c++11
LDFLAGS=-lpthread

$(bin):HttpServer.cc
	$(cc) -o $@ $^ $(LDFLAGS) #-D_DEBUG_

.PHONY:clean
clean:
	rm -f $(bin)
