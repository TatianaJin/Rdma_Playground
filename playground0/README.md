# Rdma Playground0

This playground follows the tutorial and examples from
[infiniband...](https://blog.zhaw.ch/icclab/infiniband-an-introduction-simple-ib-verbs-program-with-rdma-write/).

But later, I have found that another example is more useful, then I switch to follow that tutorial.

rdma.c is the source code provided by the tutorial.

rdma.cpp rdma_helper.hpp is written by me. Fix some issues in the examples provided.


## Usage

Currently, rdma can only be built in worker1:

    ./build.sh rdma.cpp

In worker1:

    ./rdma worker5 writer

In Worker5:
    
    ./rdma worker1 reader
    
The most important part is that writer can write only when writer and reader 
change states to ready to send (rts) and ready to recv (rtc)

Otherwise, when writer write something, reader cannot read (trapped in the read loop)...
