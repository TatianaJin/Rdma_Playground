# pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <endian.h>
#include <byteswap.h>
#include <getopt.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>
#include <iostream>

#include <vector>
#include "zmq.hpp"

// struct for config
struct config_t
{
  const char *dev_name;         /* IB device name */
  char *server_name;            /* server host name */
  u_int32_t tcp_port;           /* server TCP port */
  int ib_port;                  /* local IB port to work with */
  int gid_idx;                  /* gid index to use */
};

/* structure to exchange data which is needed to connect the QPs */
struct cm_con_data_t
{
  uint64_t addr;                /* Buffer address */
  uint32_t rkey;                /* Remote key */
  uint32_t qp_num;              /* QP number */
  uint16_t lid;                 /* LID of the IB port */
  uint8_t gid[16];              /* gid */
} __attribute__ ((packed));
/* structure of system resources */

struct dev_resource {
  struct ibv_device_attr device_attr;   /* Device attributes */
  struct ibv_port_attr port_attr;       /* IB port attributes */
  struct ibv_context *ib_ctx;   /* device handle */

  struct ibv_pd *pd;            /* PD handle */
  struct ibv_mr *mr;            /* MR handle for buf */
  char *buf;                    /* memory buffer pointer, used for RDMA and send*/

};

struct QP {
  struct cm_con_data_t remote_props;  /* values to connect to remote side */
  struct ibv_pd *pd;            /* PD handle */
  struct ibv_cq *cq;            /* CQ handle */
  struct ibv_qp *qp;            /* QP handle */
  struct ibv_mr *mr;            /* MR handle for buf */

  struct dev_resource *dev;

};

class RdmaResourcePair {
public:
    RdmaResourcePair(char* mem, uint64_t size);

    /// APIs
    
    /// This function is used to exchange QP info
    void exchange_info(const std::string& remote_name, int port);

    // int RdmaFetchAdd(int t_id,int m_id,char *local,uint64_t add,uint64_t remote_offset);
    // int RdmaCmpSwap(int t_id,int m_id,char *local,uint64_t compare,uint64_t swap,int size,uint64_t remote_offset);
    // int RdmaRead(int t_id,int m_id,char *local,int size,uint64_t remote_offset);
    int RdmaWrite(char *local,int size,uint64_t remote_offset);
    int RdmaRead(char *local,int size,uint64_t remote_offset);

    // Get the register buffer
    char* get_buf();
    void busy_read(char** res);

    // TODO
    // This function is to be used later
    int poll_completion();
    // This function is to be used later
    int post_receive(char* local, int size);
    // This function is to be used later
    void rdma_read(char** res);

    // for debug
    void barrier(const std::string& remote_name);
private:
    int rdmaOp(char* local,int size,uint64_t remote_offset,int op);
    void init();

    // for debug
    void print_conn(const cm_con_data_t& );

    // device
    dev_resource* dev;
    // qp
    QP* res;

    uint64_t size;             // The size of the rdma region,should be the same across machines!
    uint64_t off ;             // The offset to send message
    char *buffer;         // The ptr of registed buffer
    
};
