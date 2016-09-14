# Rdma Playground1

This Rdma playground follows:

[the Drtm Rdma module](https://github.com/SJTU-IPADS/drtm/tree/master/memstore),

[Rdma user manual](http://www.mellanox.com/related-docs/prod_software/RDMA_Aware_Programming_user_manual.pdf), 

[RDMAMojo blog](http://www.rdmamojo.com/2013/01/26/ibv_post_send/).

RdmaResoucePair is implemented to support one pair of QP. RDMA_WRITE and RDMA_READ examples done.

## Usage

    make

In one server (e.g. worker1):

    ./rdma_test worker5 writer

In another server (e.g. worker5)

    ./rdma_test worker1 reader
