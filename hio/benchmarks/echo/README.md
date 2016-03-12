= Echo: server based on non-blocking socket benchmark =
Author: Jiannan Ouyang <ouyang@cs.pitt.edu>
Date: 03/2016

== Server ==

select based implementation: select-echo.c
epoll based implementation: epoll-echo.c (developing)

== Client ==

Derived from mutilate (https://github.com/leverich/mutilate), 
a memcached client load generator that measures tail latency.

Here mutilate is modified to test again the echo server, 
which measures the latencies of send and receive messages.

== Usage ==
