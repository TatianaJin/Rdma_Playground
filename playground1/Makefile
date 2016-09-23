

CXX=g++
CPPFLAGS=-g -std=c++11
LDFLAGS=-libverbs -lzmq 

all: rdma_test benchmark

rdma_resouce.o: rdma_resource.hpp rdma_resource.cpp
	$(CXX) $(CPPFLAGS) $(LDFLAGS) rdma_resource.cpp -c

rdma_test: rdma_test.cpp rdma_resource.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) rdma_test.cpp -o rdma_test rdma_resource.o

benchmark: benchmark.cpp rdma_resource.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) benchmark.cpp -o benchmark rdma_resource.o
