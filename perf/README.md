# Performance

The following files implement a very silly HTTP/1.1 server using various
event notification libraries, such as [libevent](http://libevent.org),
[libuv](https://libuv.org), [libev](http://software.schmorp.de/pkg/libev.html)
and of course, [litev](https://engler.unveil2.org/software/litev).

The purpose of this is to test the performance of *litev* compared to
competing libraries using various HTTP benchmarking tools, such as *ab* and
*wrk*.
