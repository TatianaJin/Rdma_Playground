#include "rdma_helper.hpp"

#include <iostream>
#include <unistd.h>

#include <thread>
#include <chrono>

#define RDMA_WRID 3


int main(int argc, char** argv) {
    pid_t pid;
    RDMA::app_context* ctx = nullptr;

    RDMA::app_data data;
    data.port = 18515;  // only for tcp
    data.ib_port = 1;  // physical port number
    data.size = 65536;
    data.tx_depth = 100;
    data.servername = nullptr;
    data.remote_connection = static_cast<RDMA::ib_connection*>(malloc(sizeof(RDMA::ib_connection)));
    memset(data.remote_connection, 0, sizeof(RDMA::ib_connection));
    data.ib_dev = nullptr;

    ASSERT_MSG(argc == 3, "Two arguments should be provided.");
    data.servername = argv[1];  // remote host name
    std::string role = argv[2];  // writer/reader

    // Set pid
    pid = getpid();

    // Print app parameters.
    printf("PID=%d | port=%d | ib_port=%d | size=%d | tx_depth=%d | sl=%d |\n",
        pid, data.port, data.ib_port, data.size, data.tx_depth, RDMA::sl);

	// srand48(pid * time(NULL));

    // 1. init context
    ctx = RDMA::Util::init_ctx(&data);
    ASSERT_MSG(ctx != nullptr, "init_ctx error");

    // 2. set local ib connection
    RDMA::Util::set_local_ib_connection(ctx, &data);

    // 3. exchange ib connection info
    bool use_zmq = true;
    // uses_zmq or tcp?
    if (use_zmq) {
        std::cout << "Waiting for exchanging ib info" << std::endl;
        RDMA::Util::exchange_ib_info(data.local_connection, data.remote_connection, data.servername);
        std::cout << "ib info exchanged" << std::endl;
    }
    else {
        std::cout << "Waiting for exchanging ib info" << std::endl;
        if (role == "writer") {
            data.sockfd = RDMA::Util::tcp_server_listen(&data);
        }
        else if (role == "reader") {
            data.sockfd = RDMA::Util::tcp_client_connect(&data);
        }
        else {
            ASSERT_MSG(false, "exchange");
        }
        int rt = RDMA::Util::tcp_exch_ib_connection_info(&data);
        ASSERT_MSG(rt == 0, "Could not exchange connection, tcp_exch_ib_connection");
        std::cout << "ib info exchanged" << std::endl;
    }

	// Print IB-connection details
    RDMA::Util::print_ib_connection("Local  Connection", &data.local_connection);
    RDMA::Util::print_ib_connection("Remote Connection", data.remote_connection);	


    // 4. change state
	if (role == "writer") {
        RDMA::Util::qp_change_state_rts(ctx->qp, &data);
	} 
    else if (role == "reader") {
        RDMA::Util::qp_change_state_rtr(ctx->qp, &data);
	}
    else {
        ASSERT_MSG(false, "unable to change state");
    }

    // Should have a barrier!
    RDMA::Util::ready_to_write(data.servername);

    // 5. RDMA write
    if (role == "writer") {
        std::cout << "Writting"  << std::endl;
        char* msg = (char*)ctx->buf;
        strcpy(msg, "hello world");

        RDMA::Util::rdma_write(ctx, &data);
        std::cout << "Writting done"  << std::endl;
    }
    else if (role == "reader") {
        std::cout << "reading" << std::endl;
        RDMA::Util::rdma_read(&data);
        std::cout << "Reading done"  << std::endl;
    }

    RDMA::Util::destroy_ctx(ctx);
}
