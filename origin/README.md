# Rdma Origin Codes From User Manual

The codes here is from:
[Rdma user manual](http://www.mellanox.com/related-docs/prod_software/RDMA_Aware_Programming_user_manual.pdf), 

## Usage

    ./build.sh origin.c

In one server (e.g. worker1), act as server:

    ./origin

In another server (e.g. worker5), act as client:

    ./origin worker1

Make sure to start server first. 

Some modifications are needed to make the source codes from the manual runs.

Still don't know why it will report "completion wasn't found in the CQ after timeout, poll completion failed".
