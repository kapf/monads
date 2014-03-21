Boost.Monads
============

This is a small library to show how monads could be implemented
in C++11 and what possibilities they offer.

It serves as an addition to my GSoC 2014 proposal.

Examples
--------

The examples directory contains some examples on how to use this
library.  Some excerpts:

    // unique_ptr as Maybe monad
    std::unique_ptr<int> maybe(new int(4));
    assert(*((boost::monads::monad_pipe(maybe) >>= inc) >>= inc).unpipe() == 6);

    // Continuations with call/cc
    mon::run_cont(
      (mon::monad_pipe(mon::call_cc(call_cc_test{3})) >>= loud_sqr).unpipe(),
      printer{});

    // Faster iteration over std::deque with continuations
    // continuation: 2.62ms, accumulate: 6.19ms
    boost::monads::run_cont(deque_foreacher<int>{d},
                            [&](int i){sum += i;});
    int sum=std::accumulate(d.begin(), d.end(), 0);

Library
-------

It's best to have a look at the `include` directory to see how monads
are defined in this demo.
