

CXX=g++
CPPFLAGS=-g -std=c++11
LDFLAGS=-libverbs -lzmq 

all: rdma

rdma_resouce.o: rdma_resource.hpp rdma_resource.cpp
	$(CXX) $(CPPFLAGS) $(LDFLAGS) rdma_resource.cpp -c

rdma: rdma_test.cpp rdma_resource.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) rdma_test.cpp -o rdma_test rdma_resource.o

