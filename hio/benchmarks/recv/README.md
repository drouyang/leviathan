= Echo: server based on non-blocking socket benchmark =
Author: Jiannan Ouyang <ouyang@cs.pitt.edu>
Date: 03/2016

== Usage ==
* server side: $./echo
* client side: $./mutilate/mutilate --server 136.142.119.102 --valuesize=1024000 --threads=10 --affinity -t 5

== Server ==
Receive a message, and respond "Done".

select based implementation: echo.c
epoll based implementation: epoll-echo.c (not working with HIO)

== Client ==

Derived from mutilate (https://github.com/leverich/mutilate), 
a memcached client load generator that measures tail latency.

Here mutilate is modified to test again the echo server, 
which measures the latencies of send and receive messages.

=== How mutilate works ===
* Core parameter is target qps (-q) and number of threads (-T). Request
  frequency is calculated from the target QPS and the number of working
  threads, for both master-only mode and agent mode.

* Should use --affinity to pin threads, should run with --noload

* Use libzmq to do inter-process communication

* Inter-arrival distribution is exponential by default, use --iadist to adjust it.

* Agent mode  

** master sends options to agents, agents reply number of threads to spawn
(--threads * --lambda_mul); --lambda_mul is used to adjust workload share
between agents.

** master sends total number of connections lambda_denom (weighted by
lambda_mul), each client adjust its lambda (qps / lambda_denon *
args.lambda_mul); 

** everyone executes do_mutilate(), which calls Connection->actions()

