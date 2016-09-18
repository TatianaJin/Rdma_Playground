# include "rdma_resource.hpp"
#include <iostream>

int main(int argc, char** argv) {

    int port = 12332;

    assert(argc == 3);
    std::string remote_name = argv[1];
    std::string role = argv[2];

    int size = 1024;
    void* mem = malloc(size);

    // create an RdmaResourcePair
    RdmaResourcePair rdma((char*)mem, size);
    // exchange the info    
    rdma.exchange_info(remote_name, port);

    // test rdma write
    // writer writes data to reader
    {
        if (role == "writer") {
            char* msg = rdma.get_buf();
            strcpy(msg, "hello world");
            // the last parameter is the virtual address of remote machine
            rdma.RdmaWrite(msg, strlen(msg), 0); 
        }
        else if (role == "reader") {
            char* res;
            rdma.busy_read(&res);
            printf("Message received: %s\n" , res);
        }
        else {
            assert(false);
        }
    }
    // test rdma read
    // reader reads data from writer
    {
        const char* data = "Come and read my data!";
        if (role == "writer") {
            char* msg = rdma.get_buf();
            strcpy(msg, data);
        }
        // set a barrier
        rdma.barrier(remote_name);
        if (role == "reader") {
            char* msg = rdma.get_buf();
            rdma.RdmaRead(msg, strlen(data), 0);
            printf("Message received: %s\n" , msg);
        }
    }
    {
        if (role == "reader")
            rdma.post_receive(rdma.get_buf(), 1);
        rdma.barrier(remote_name);
        if (role == "writer") {
            char* msg = rdma.get_buf();
            strcpy(msg, "hello world");
            // the last parameter is the virtual address of remote machine
            rdma.RdmaWriteWithImmediate(msg, strlen(msg), 0); 
        }
        else if (role == "reader") {
            rdma.poll_completion();
            char* res;
            rdma.rdma_read(&res);
            printf("Message received: %s\n" , res);
        }
        else {
            assert(false);
        }
    }
}
