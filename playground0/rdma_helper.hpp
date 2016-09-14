#include <iostream>
#include <unistd.h>
#include <cstring>

#include <netdb.h>

#include "zmq.hpp"

#include "core/utils.hpp"
#include "infiniband/verbs.h"

#define RDMA_WRID 3

namespace RDMA {

struct app_context {
    ibv_context* context; // ibv context 
    ibv_pd* pd; // Protection Domain
    ibv_mr* mr; // Memroy Region
    ibv_cq* rcq; // Recv Completion Queue
    ibv_cq* scq; // Send Completion Queue
    ibv_qp* qp; // Queue Pair
    ibv_comp_channel* ch; // Completion Channel
    void* buf = nullptr; // Buffer
    unsigned size; // Size
    int tx_depth;
    ibv_sge sge_list; // Scatter/Gatter Element List
    ibv_send_wr wr; // Work Request
};

struct ib_connection {
    int lid; // Local Identifier
    int qpn; // Queue Pair Number
    int psn; // Packet Sequence Number
    unsigned rkey; // Remote key
    unsigned long long vaddr; // Virtual address
};

struct app_data {
    int port;  // only used for tcp
    int ib_port;  // physical port number
    unsigned size;
    int tx_depth;  // minimun number of cqe in CQ, used in ibv_create_cq
    int sockfd;  // only used for tcp
    char* servername;
    ib_connection local_connection;
    ib_connection* remote_connection;
    ibv_device* ib_dev;
};

static int sl = 1; // What's this?

namespace Util {

/// Change Queue Pair status to INIT
static int qp_change_state_init(ibv_qp* qp, app_data* data) {
    ibv_qp_attr* attr;
    attr = static_cast<ibv_qp_attr*>(malloc(sizeof(ibv_qp_attr)));
    memset(attr, 0, sizeof(ibv_qp_attr));

    attr->qp_state = IBV_QPS_INIT;
    attr->pkey_index = 0;
    attr->port_num = data->ib_port;
    attr->qp_access_flags = IBV_ACCESS_REMOTE_WRITE;
    bool rt = ibv_modify_qp(qp, attr, IBV_QP_STATE|IBV_QP_PKEY_INDEX|IBV_QP_PORT|IBV_QP_ACCESS_FLAGS);
    ASSERT_MSG(rt == 0, "Could not modify QP to INIT");
    free(attr); // TODO need this, right?
    return 0;
}

/// Change Queue Pair status to RTR (Ready to receive)
static int qp_change_state_rtr(ibv_qp* qp, app_data* data) {
    ibv_qp_attr* attr;
    attr = static_cast<ibv_qp_attr*>(malloc(sizeof(ibv_qp_attr)));
    memset(attr, 0, sizeof(ibv_qp_attr));

    attr->qp_state = IBV_QPS_RTR;
    attr->path_mtu = IBV_MTU_2048;
    attr->dest_qp_num = data->remote_connection->qpn;
    attr->rq_psn = data->remote_connection->psn;
    attr->max_dest_rd_atomic = 1;
    attr->min_rnr_timer = 12;
    attr->ah_attr.is_global = 0;
    attr->ah_attr.dlid = data->remote_connection->lid;
    attr->ah_attr.sl = sl;
    attr->ah_attr.src_path_bits = 0;
    attr->ah_attr.port_num = data->ib_port;

    bool rt = ibv_modify_qp(qp, attr,
                IBV_QP_STATE                |
                IBV_QP_AV                   |
                IBV_QP_PATH_MTU             |
                IBV_QP_DEST_QPN             |
                IBV_QP_RQ_PSN               |
                IBV_QP_MAX_DEST_RD_ATOMIC   |
                IBV_QP_MIN_RNR_TIMER);
    ASSERT_MSG(rt == 0, "Could not modify QP to RTR state");
	free(attr);
    return 0;
}

/// Change Queue Pair status to RTS (Ready to send)
/// QP status has to be RTR before changing it to RTS
static int qp_change_state_rts(ibv_qp* qp, app_data* data) {
    // Change qp status to RTR first
    qp_change_state_rtr(qp, data);

    ibv_qp_attr* attr;
    attr = static_cast<ibv_qp_attr*>(malloc(sizeof(ibv_qp_attr)));
    memset(attr, 0, sizeof(ibv_qp_attr));

	attr->qp_state = IBV_QPS_RTS;
    attr->timeout = 14;
    attr->retry_cnt = 7;
    attr->rnr_retry = 7;    /* infinite retry */
    attr->sq_psn = data->local_connection.psn;
    attr->max_rd_atomic = 1;
    bool rt = ibv_modify_qp(qp, attr,
                IBV_QP_STATE            |
                IBV_QP_TIMEOUT          |
                IBV_QP_RETRY_CNT        |
                IBV_QP_RNR_RETRY        |
                IBV_QP_SQ_PSN           |
                IBV_QP_MAX_QP_RD_ATOMIC);
    ASSERT_MSG(rt == 0, "Could not modify QP to RTS state");
    free(attr);
    return 0;
}


// 1. init context
static app_context* init_ctx(app_data* data) {
    // init app_context
    app_context* ctx;
    ctx = static_cast<app_context*>(malloc(sizeof(app_context)));
    memset(ctx, 0, sizeof(app_context));

    ibv_device* ib_dev;

    ctx->size = data->size;
    ctx->tx_depth = data->tx_depth;

    // Get sys pagesize
    int page_size = sysconf(_SC_PAGESIZE);
    std::cout << "The pagesize: " << page_size << std::endl;
    posix_memalign(&ctx->buf, page_size, ctx->size*2);
    ASSERT_MSG(ctx->buf != nullptr, "Cound not allocate working buffer ctx->buf");
    memset(ctx->buf, 0, ctx->size*2);

    ibv_device** dev_list;
    dev_list = ibv_get_device_list(nullptr);  // Get device list
    ASSERT_MSG(dev_list != nullptr, "No IB-device available. ibv_get_device_list return nullptr");

    data->ib_dev = dev_list[0];  // Get the first device
    ASSERT_MSG(data->ib_dev != nullptr, "IB-device could not be assigned. Maybe dev_list array is empty");

    ctx->context = ibv_open_device(data->ib_dev);  // Create context
    ASSERT_MSG(ctx->context != nullptr, "Counld not create context");

    ctx->pd = ibv_alloc_pd(ctx->context); // Create Protection Domain
    ASSERT_MSG(ctx->pd != nullptr, "Counld not allocate protection domain");

    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, ctx->size*2, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE); // Create Memory Region
    ASSERT_MSG(ctx->mr != nullptr, "Counld not allocate mr, Do you have root access?");

    ctx->ch = ibv_create_comp_channel(ctx->context); // Create Completion Channel
    ASSERT_MSG(ctx->ch != nullptr, "Counld not create completion channel");

    ctx->rcq = ibv_create_cq(ctx->context, 1, nullptr, nullptr, 0); // Create Receive Completion Queue
    ASSERT_MSG(ctx->rcq != nullptr, "Could not create receive completion queue");

    ctx->scq = ibv_create_cq(ctx->context, ctx->tx_depth, ctx, ctx->ch, 0);  // Create Send Completion Queue
    ASSERT_MSG(ctx->scq != nullptr, "Could not create send completion queue");

    ibv_qp_init_attr qp_init_attr;  // Init Queue Pair
    qp_init_attr.qp_context = nullptr;
    qp_init_attr.srq = nullptr;
    qp_init_attr.send_cq = ctx->scq;
    qp_init_attr.recv_cq = ctx->rcq;
    qp_init_attr.qp_type = IBV_QPT_RC;
    // qp_init_attr.cap = {static_cast<uint32_t>(ctx->tx_depth), 1, 1, 1, 0};  // {max_send_wr, max_recv_wr, max_send_sge, max_recv_sge, max_inline_data}
    qp_init_attr.cap.max_send_wr = ctx->tx_depth;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.cap.max_inline_data = 0;
    ctx->qp = ibv_create_qp(ctx->pd, &qp_init_attr);  // Create Queue Pair
    ASSERT_MSG(ctx->qp != nullptr, "Could not create queue pair");

    RDMA::Util::qp_change_state_init(ctx->qp, data);

    return ctx;
}

// 2. set local ib connection
static void set_local_ib_connection(app_context* ctx, app_data* data) {
    ibv_port_attr attr;
	bool rt = ibv_query_port(ctx->context, data->ib_port, &attr);
    ASSERT_MSG(rt == 0, "Could not get port attributes, ibv_query_port");

	data->local_connection.lid = attr.lid;
	data->local_connection.qpn = ctx->qp->qp_num;
	data->local_connection.psn = lrand48() & 0xffffff;
	data->local_connection.rkey = ctx->mr->rkey;
	data->local_connection.vaddr = (uintptr_t)ctx->buf + ctx->size;
}

// set tcp connection
static int tcp_client_connect(app_data *data)
{
	addrinfo *res, *t;
	// addrinfo hints = {
	// 	.ai_family		= AF_UNSPEC,
	// 	.ai_socktype	= SOCK_STREAM
	// };
	addrinfo hints;
    memset(&hints, 0, sizeof(hints));
	hints.ai_family		= AF_UNSPEC;
    hints.ai_socktype	= SOCK_STREAM;

	char *service;
	int n;
	int sockfd = -1;
	sockaddr_in sin;

	bool rt = asprintf(&service, "%d", data->port);
    ASSERT_MSG(rt >= 0, "Error writing port-number to port-string");

	rt = getaddrinfo(data->servername, service, &hints, &res);
    ASSERT_MSG(rt >= 0, "getaddrinfo threw error");

	for(t = res; t; t = t->ai_next){
		sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
	    ASSERT_MSG(sockfd >= 0, "Could not create client socket");
		rt = connect(sockfd,t->ai_addr, t->ai_addrlen);
	    ASSERT_MSG(rt >= 0, "Could not connect to server");	
	}
	freeaddrinfo(res);
	
	return sockfd;
}

static int tcp_server_listen(struct app_data *data){
    addrinfo *res, *t;
	// struct addrinfo hints = {
	// 	.ai_flags		= AI_PASSIVE,
	// 	.ai_family		= AF_UNSPEC,
	// 	.ai_socktype	= SOCK_STREAM	
	// };
	addrinfo hints;
    memset(&hints, 0, sizeof(hints));
	hints.ai_flags		= AI_PASSIVE;
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;

	char *service;
	int sockfd = -1;
	int n, connfd;
	sockaddr_in sin;

	asprintf(&service, "%d", data->port);

    n = getaddrinfo(NULL, service, &hints, &res);

	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);

    bind(sockfd,res->ai_addr, res->ai_addrlen);
	
	listen(sockfd, 1);

	connfd = accept(sockfd, NULL, 0);

	freeaddrinfo(res);

	return connfd;
}

static int tcp_exch_ib_connection_info(struct app_data *data){
	char msg[sizeof "0000:000000:000000:00000000:0000000000000000"];
	int parsed;

	ib_connection *local = &data->local_connection;

	sprintf(msg, "%04x:%06x:%06x:%08x:%016Lx", 
				local->lid, local->qpn, local->psn, local->rkey, local->vaddr);

	if(write(data->sockfd, msg, sizeof msg) != sizeof msg){
		perror("Could not send connection_details to peer");
		return -1;
	}	

	if(read(data->sockfd, msg, sizeof msg) != sizeof msg){
		perror("Could not receive connection_details to peer");
		return -1;
	}

	if(!data->remote_connection){
		free(data->remote_connection);
	}

	bool rt = data->remote_connection = static_cast<ib_connection*>(malloc(sizeof(struct ib_connection)));
	ASSERT_MSG(rt != 0, "Could not allocate memory for remote_connection connection");

	ib_connection *remote = data->remote_connection;

	parsed = sscanf(msg, "%x:%x:%x:%x:%Lx", 
						&remote->lid, &remote->qpn, &remote->psn, &remote->rkey, &remote->vaddr);
	
	if(parsed != 5){
		fprintf(stderr, "Could not parse message from peer");
		free(data->remote_connection);
	}
	
	return 0;
}

static void print_ib_connection(char *conn_name, struct ib_connection *conn){
	printf("%s: LID %#04x, QPN %#06x, PSN %#06x RKey %#08x VAddr %#016Lx\n", 
			conn_name, conn->lid, conn->qpn, conn->psn, conn->rkey, conn->vaddr);
}

void print_conn(const ib_connection& conn) {
    std::cout << conn.lid << " " << conn.qpn << " " << conn.psn 
        << " " << conn.rkey << " " << conn.vaddr << std::endl;
}

static void exchange_ib_info(const ib_connection& local_conn, ib_connection* remote_conn, const std::string& remote_host) {
    zmq::context_t ctx;

    zmq::socket_t pull(ctx, ZMQ_PULL);
    pull.bind("tcp://*:12123");

    zmq::socket_t push(ctx, ZMQ_PUSH);
    push.connect("tcp://"+remote_host+":12123");

    zmq::message_t msg(sizeof(struct ib_connection));
    memcpy(msg.data(), &local_conn, sizeof(struct ib_connection));
    push.send(msg);
    // print_conn(local_conn);

    pull.recv(remote_conn, sizeof(struct ib_connection), 0);
    // print_conn(*remote_conn);
}

static void ready_to_write(const std::string& remote_host) {
    zmq::context_t ctx;

    zmq::socket_t pull(ctx, ZMQ_PULL);
    pull.bind("tcp://*:12123");

    zmq::socket_t push(ctx, ZMQ_PUSH);
    push.connect("tcp://"+remote_host+":12123");

    zmq::message_t msg, recv_msg;  // dummy message
    push.send(msg);

    pull.recv(&recv_msg);
}

static void rdma_read(app_data* data) {
    char *chPtr = reinterpret_cast<char*>(data->local_connection.vaddr);
    while(1){
        if(strlen(chPtr) > 0){
            break;
        }
    }
    printf("Printing local buffer: %s\n" ,chPtr);
}
static void rdma_write(app_context *ctx, app_data *data){
	ctx->sge_list.addr      = (uintptr_t)ctx->buf;
   	ctx->sge_list.length    = ctx->size;
   	ctx->sge_list.lkey      = ctx->mr->lkey;

    memset(&(ctx->wr), 0, sizeof(ibv_send_wr));
  	ctx->wr.wr.rdma.remote_addr = data->remote_connection->vaddr;
    ctx->wr.wr.rdma.rkey        = data->remote_connection->rkey;
    ctx->wr.wr_id       = RDMA_WRID;
    ctx->wr.sg_list     = &ctx->sge_list;
    ctx->wr.num_sge     = 1;
    ctx->wr.opcode      = IBV_WR_RDMA_WRITE;
    ctx->wr.send_flags  = IBV_SEND_SIGNALED;
    ctx->wr.next        = nullptr;

    ibv_send_wr *bad_wr = nullptr;

    bool rt = ibv_post_send(ctx->qp,&ctx->wr,&bad_wr);
    ASSERT_MSG(rt == 0, "ibv_post_send failed. This is bad mkay");
}

static void destroy_ctx(app_context *ctx){
	bool rt = ibv_destroy_qp(ctx->qp);
	ASSERT_MSG(rt == 0, "Could not destroy queue pair, ibv_destroy_qp");
	
	rt = ibv_destroy_cq(ctx->scq);
    ASSERT_MSG(rt == 0, "Could not destroy send completion queue, ibv_destroy_cq");

	rt = ibv_destroy_cq(ctx->rcq);
    ASSERT_MSG(rt == 0, "Coud not destroy receive completion queue, ibv_destroy_cq");
	
	rt = ibv_destroy_comp_channel(ctx->ch);
    ASSERT_MSG(rt == 0, "Could not destory completion channel, ibv_destroy_comp_channel");

	rt = ibv_dereg_mr(ctx->mr);
    ASSERT_MSG(rt == 0, "Could not de-register memory region, ibv_dereg_mr");

	rt = ibv_dealloc_pd(ctx->pd);
    ASSERT_MSG(rt == 0, "Could not deallocate protection domain, ibv_dealloc_pd");	

	free(ctx->buf);
	free(ctx);
}

}  // namespace Util
}  // namespace RDMA
