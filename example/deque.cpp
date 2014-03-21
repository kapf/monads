#include <boost/monads/monad.hpp>
#include <boost/monads/controlmonad.hpp>

#include <cassert>
#include <numeric>
#include <iostream>
#include <chrono>
#include <deque>

// this only works with stdlibc++

template <typename T>
struct deque_foreacher {
  std::deque<T>& deq;

  template <typename K>
  void operator()(K&& k) const {
    using namespace std;

    _Deque_iterator<T, const T&, const T*> __first = deq.begin();
    _Deque_iterator<T, const T&, const T*> __last = deq.end();
    
    typedef typename _Deque_iterator<T, T&, T*>::_Self _Self;
    typedef typename _Self::difference_type difference_type;
    
    difference_type __len = __last - __first;
    while (__len > 0) {
      const difference_type __clen = __first._M_last - __first._M_cur;
      for (auto* segment_first = __first._M_cur, 
               * segment_last  = __first._M_last;
           segment_first != segment_last; ++segment_first) {
        k(*segment_first);
      }
      __first += __clen;
      __len -= __clen;
    }
  }
};

template <typename F>
void time_call(const char* msg, F&& f)
{
  using namespace std::chrono;
  auto start = high_resolution_clock::now();
  f();
  auto end = high_resolution_clock::now();
  std::cout << msg << ": "
            << duration_cast<nanoseconds>(end - start).count()
            << "ns\n";
}

int main()
{
  std::deque<int> d;
  const int size = 10*1000*1000;
  srand(0);
  for (int i=0; i<size; ++i)
    d.push_back(rand());

  int master_sum = std::accumulate(d.begin(), d.end(), 0);
  for (int i=0; i<10; ++i) {
    time_call("accumulate  ", [&]() {
        int sum=std::accumulate(d.begin(), d.end(), 0);
        assert(sum == master_sum);
      });
  
    time_call("foreach loop", [&]() {
        int sum=0;
        for (int i : d)
          sum += i;
        assert(sum == master_sum);
      });
    time_call("continuation", [&]() {
        int sum=0;
        boost::monads::run_cont(deque_foreacher<int>{d},
                                [&](int i){sum += i;});
        assert(sum == master_sum);
      });
  }
}

/*
Just for reference, here's the output using g++ (GCC) 4.8.2:

accumulate  : 5354118ns
foreach loop: 5601285ns
continuation: 3022173ns
accumulate  : 5320084ns
foreach loop: 5016697ns
continuation: 2909400ns
accumulate  : 5995062ns
foreach loop: 5032623ns
continuation: 2653683ns
accumulate  : 5520358ns
foreach loop: 4975916ns
continuation: 2628977ns
accumulate  : 5308461ns
foreach loop: 4991878ns
continuation: 2617513ns
accumulate  : 5769553ns
foreach loop: 5339515ns
continuation: 2820368ns
accumulate  : 5769728ns
foreach loop: 5011447ns
continuation: 2658527ns
accumulate  : 5393961ns
foreach loop: 5000950ns
continuation: 2642141ns
accumulate  : 5316114ns
foreach loop: 4966526ns
continuation: 2665640ns
accumulate  : 6192621ns
foreach loop: 4972837ns
continuation: 2622478ns
 */
