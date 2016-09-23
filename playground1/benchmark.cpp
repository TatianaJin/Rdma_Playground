#include "rdma_resource.hpp"
#include "zmq.hpp"

#include <iostream>
#include <chrono>

using namespace std::chrono;

RdmaResourcePair rdma_setup(std::string remote_name, std::string role, char* mem, size_t size) {
    int port = 12332;
    RdmaResourcePair rdma(mem, size);
    rdma.exchange_info(remote_name, port);
    std::cout << "[Testing] RDMA Connection setup" << std::endl;
    return rdma;
}

void rdma_communicate(RdmaResourcePair& rdma, std::string remote_name, std::string role, char* mem, size_t size) {
    if (role == "reader")
        rdma.post_receive(rdma.get_buf(), 1);
    rdma.barrier(remote_name);
    if (role == "writer") {
        // char* msg = rdma.get_buf();
        // strcpy(msg, "hello world");
        // the last parameter is the virtual address of remote machine
        auto t1 = steady_clock::now();
        rdma.RdmaWriteWithImmediate(rdma.get_buf(), size, 0); 
        auto t2 = steady_clock::now();
        auto dura = duration_cast<duration<double>>(t2-t1);
        std::cout << "[Testing] Write time: " << dura.count() << " seconds." << std::endl;
    }
    else if (role == "reader") {
        rdma.poll_completion();
        char* res;
        rdma.rdma_read(&res);
        std::cout << "[Testing] Message received: " << strlen(res) << std::endl;
    }
    else {
        assert(false);
    }
}

void test_rdma(std::string remote_name, std::string role, char* mem, size_t size, size_t times) {
    // setup rdma connection
    auto rdma = rdma_setup(remote_name, role, mem, size);

    // communicate
    for (int i = 0; i < times; ++ i) {
        auto t1 = steady_clock::now();
        rdma_communicate(rdma, remote_name, role, mem, size);
        auto t2 = steady_clock::now();
        auto dura = duration_cast<duration<double>>(t2-t1);
        std::cout << "[Testing] Round time: " << dura.count() << " seconds." << std::endl;
    }

}

zmq::socket_t zmq_setup(zmq::context_t& ctx, std::string remote_name, std::string role, char* mem, size_t size) {
    int port = 12332;
    if (role == "reader") {
        zmq::socket_t pull(ctx, ZMQ_PULL);
        pull.bind("tcp://*:"+std::to_string(port));
        return pull;
    } else if (role == "writer") {
        zmq::socket_t push(ctx, ZMQ_PUSH);
        push.connect("tcp://"+remote_name+":"+std::to_string(port));
        return push;
    }
}

void zmq_communicate(zmq::socket_t& zmq_socket, std::string remote_name, std::string role, char* mem, size_t size) {
    if (role == "reader") {
        zmq::message_t msg;
        zmq_socket.recv(&msg);
        std::cout << "[Testing] message size: " << msg.size() << std::endl;
    } else if (role == "writer") {
        zmq::message_t msg(mem, size);
        zmq_socket.send(msg);
    }
}

void test_zmq(std::string remote_name, std::string role, char* mem, size_t size, size_t times) {
    // setup zmq connection
    zmq::context_t ctx;
    std::cout << "[Testing] zmq tring to setup done" << std::endl;
    auto zmq_socket = zmq_setup(ctx, remote_name, role, mem, size);
    std::cout << "[Testing] zmq setup done" << std::endl;

    // communiate
    for (int i = 0; i < times; ++ i) {
        auto t1 = steady_clock::now();
        zmq_communicate(zmq_socket, remote_name, role, mem, size);
        auto t2 = steady_clock::now();
        auto dura = duration_cast<duration<double>>(t2-t1);
        std::cout << "[Testing] Round time: " << dura.count() << " seconds." << std::endl;
    }
}

int main(int argc, char** argv) {

    assert(argc == 3);
    std::string remote_name = argv[1];
    std::string role = argv[2];

    // Generate a very large string
    constexpr size_t megabyte = 1048576; 
    constexpr size_t gigabyte = 1073741824; 
    constexpr size_t size = gigabyte;
    std::cout << "[Testing] Testing size : " << size << std::endl;

    char* mem = (char*)malloc(size);
    memset(mem, -1, size);
    std::cout << "[Testing] string generated" << std::endl;

    std::cout << "[Testing] rdma" << std::endl;
    auto t1 = steady_clock::now();
    test_rdma(remote_name, role, mem, size, 3);
    auto t2 = steady_clock::now();
    auto dura = duration_cast<duration<double>>(t2-t1);
    std::cout << "[Testing] Time elapsed: " << dura.count() << " seconds." << std::endl;

    std::cout << "[Testing] zmq" << std::endl;
    test_zmq(remote_name, role, mem, size, 3);
}
