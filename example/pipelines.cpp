#include <boost/monads/monad.hpp>
#include <boost/monads/controlmonad.hpp>
#include <boost/monads/algorithm.hpp>
#include <boost/algorithm/string.hpp> // starts_with and trim

#include <memory>
#include <iostream>
#include <cassert>
#include <future>

namespace mon = boost::monads;

// to implement the pipeline proposal n3534 with my current incomplete
// library, we need do two things:
//  1) provide nice operators
//  2) implement some monads, here: std::future and a concurrent queue
//

// -----------------------------------------------------------------------------
// 1) provide nice operators

// n3534 hardcodes function<void(IN,OUT)>,
//                 function<void(IN,queue_back<OUT>)>,
//                 function<void(queue_front<IN>,OUT)> and
//                 function<void(queue_front<IN>,queue_back<OUT>)>
// for different interfaces.  As this cannot be done in general (what
// if IN should be queue_front<int>?) and for any monad, I use
// different operators for this: |, >>, << and ||.
//
// Notation: a := IN, b := OUT,
//           M1 a := queue_front<IN>
//           M2 b := queue_back<OUT>
//           x -> y := void(x, y) (for any x and y)
// Then we can pipeline using the following operators:
//    pipeline | (a -> b)          Haskell: (|)  :: M a -> (a -> b) -> M b;   (|)  = flip fmap
//    pipeline >> (a -> M b)       Haskell: (>>) :: M a -> (a -> M b) -> M b; (>>) = mbind)
//    pipeline << (M a -> b)       Haskell: (<<) :: M a -> (M a -> b) -> M b; (<<) m f = return $ f m
//    pipeline || (M a -> M b)     Haskell: (||) :: M a -> (M a -> M b) -> M b; (||) = flip ($)
//
// Note that "|" and "||" can be implemented more generally for
// Functors, but this demo focusses on Monads.

template <typename Monad, typename M_a> struct pipeliner;

template <typename Monad, typename M_a>
pipeliner<Monad, typename std::decay<M_a>::type> pipeline(M_a&& m);

template <typename Monad, typename M_a>
struct pipeliner
{
    M_a monad;

    M_a get() && { return std::move(monad); }

    pipeliner(M_a monad) : monad(std::move(monad)) {}

    template <typename MInToMOut>
    auto operator||(MInToMOut&& m_in_to_m_out)
        -> decltype(pipeline<Monad>(std::forward<MInToMOut>(m_in_to_m_out)(std::move(monad))))
    {
        return pipeline<Monad>(std::forward<MInToMOut>(m_in_to_m_out)(std::move(monad)));
    }

    template <typename InToOut>
    auto operator|(InToOut&& in_to_out)
        -> decltype(*this || mon::liftm<Monad>(std::forward<InToOut>(in_to_out)))
    {
        return *this || mon::liftm<Monad>(std::forward<InToOut>(in_to_out));
    }

    template <typename InToMOut>
    auto operator>>(InToMOut&& in_to_m_out)
        -> decltype(pipeline<Monad>(mon::join(std::move((*this | std::forward<InToMOut>(in_to_m_out)).monad))))
    {
        return pipeline<Monad>(mon::join(std::move((*this | std::forward<InToMOut>(in_to_m_out)).monad)));
    }

    template <typename MInToOut>
    auto operator<<(MInToOut&& m_in_to_out)
        -> decltype(pipeline<Monad>(mon::mreturn<Monad>(std::move((*this || std::forward<MInToOut>(m_in_to_out)).monad))))
    {
        return pipeline<Monad>(mon::mreturn<Monad>(std::move((*this || std::forward<MInToOut>(m_in_to_out)).monad)));
    }
};

template <typename Monad, typename M_a>
pipeliner<Monad, typename std::decay<M_a>::type> pipeline(M_a&& m)
{
    return pipeliner<Monad, typename std::decay<M_a>::type>(std::forward<M_a>(m));
}

template <typename Monad, typename T>
auto pipeline_from(T&& x)
    -> decltype(pipeline<Monad>(boost::monads::mreturn<Monad>(std::forward<T>(x))))
{
    return pipeline<Monad>(boost::monads::mreturn<Monad>(std::forward<T>(x)));
}

// -----------------------------------------------------------------------------
// 2) a) implement std::future as monad
// This should be done much more efficiently, this is only a proof of concept.

struct future_monad {
    template <typename T>
    static std::future<T> mreturn(T&& x)
    {
        // should return "make_ready_future"
        return std::async(std::launch::deferred, [](T&& x){return x;}, std::move(x));
    }
};

namespace std {
template <typename T, typename F>
auto boost_mbind(std::future<T> p, F &&fun)
    -> std::future<decltype(std::forward<F>(fun)(std::move(p).get()).get())> {
  struct move_captured_lambda
  {
      future<T> fut;
      F fun;
      decltype(std::forward<F>(fun)(std::move(p).get()).get()) operator()()
      {
        // should call future.then()
          return std::forward<F>(fun)(std::move(std::move(fut).get())).get();
      }
  };
  return std::async(std::launch::async,
                    move_captured_lambda{std::move(p), std::forward<F>(fun)});
}
} // namespace std

// -----------------------------------------------------------------------------
// 2) b) implement a concurrent queue as monad
// This should be done much more efficiently, this is only a proof of concept.

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

template <typename T>
class blocking_queue
{
    std::deque<T> queue;
    bool closed = false;
    std::mutex mutex;
    std::condition_variable cond;
public:
    typedef T value_type;

    template <typename T2>
    void push(T2&& value)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push_back(std::forward<T2>(value));
        }
        cond.notify_one();
    }
    bool pop(T& elem)
    {
        std::unique_lock<std::mutex> lock(mutex);

        cond.wait(lock, [=](){ return closed || !queue.empty(); });
        if (closed && queue.empty())
            return false;
        elem = std::move(queue.front());
        queue.pop_front();
        return true;
    }
    void close()
    {
        std::lock_guard<std::mutex> lock(mutex);
        closed = true;
        cond.notify_one();
    }
};

template <typename T>
using shared_blocking_queue = std::shared_ptr<blocking_queue<T> >;

struct segment_monad {
    template <typename T>
    static shared_blocking_queue<T> mempty()
    {
        auto q = std::make_shared<blocking_queue<T> >();
        q->close();
        return q;
    }

    template <typename T>
    static shared_blocking_queue<typename std::decay<T>::type> mreturn(T x)
    {
        auto q = std::make_shared<blocking_queue<T> >();
        q->push(std::forward<T>(x));
        q->close();
        return q;
    }

    template <typename Iter,
              typename T=typename std::iterator_traits<Iter>::value_type>
    static shared_blocking_queue<T>
    from_range(Iter from, Iter to)
    {
        auto out = std::make_shared<blocking_queue<T> >();
        std::thread([=](){
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                for (Iter f=from, t=to; f != t;)
                    out->push(*f++);
                out->close();
            }).detach();
        return out;
    }

    template <typename F, typename T>
    using RetVal = typename std::decay<decltype(std::declval<F>()(std::declval<T>()))>::type;
    template <typename T>
    using ElemType = typename T::element_type;
    template <typename T>
    using ValueType = typename T::value_type;

    template <typename T, typename F,
              typename U = ValueType<ElemType<RetVal<F, T> > > >
    friend shared_blocking_queue<U>
    boost_mbind(shared_blocking_queue<T> const &q, F fun) {
        auto out = std::make_shared<blocking_queue<U> >();
        std::thread([=](F fun){
                T in;
                U mid;
                while (q->pop(in)) {
                    auto q2 = fun(std::move(in));
                    while (q2->pop(mid)) {
                        out->push(std::move(mid));
                    }
                }
                out->close();
            }, std::move(fun)).detach();
        return out;
    }
};

#include <type_traits>
int main()
{
  namespace mon = boost::monads;
  auto pause = [](int ms){
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
      std::cout<<"thread id: "<<std::this_thread::get_id()<<std::endl;
  };
  auto xpause = [](const char*msg){
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      std::cout<< msg << "@"<<std::this_thread::get_id()<<std::endl;
  };
  {
      auto r = 
          pipeline_from<future_monad>(2)
          | [=](int i){pause(100);return i+1;}
          | [=](int i){pause(100);return 2*i;}
         || [=](std::future<int> i){return std::async([=](std::future<int> j){pause(100); return j.get()-1;}, std::move(i));};
      std::cout << "pipeline created in "; pause(0); std::cout << "--" << std::endl;
      std::cout << "pipeline result: "<<std::move(r).get().get() << '\n';
      // output: 
      //   pipeline created in thread id: 140342130247488
      //   --
      //   thread id: 140342113699584
      //   thread id: 140342130247488
      //   thread id: 140342104979200
      //   pipeline result: 5
  }
  {
      auto filter_spaces = [=](char c)
          { xpause("filter spaces"); return c==' ' ? segment_monad::mempty<char>() : segment_monad::mreturn(c); };
      auto parse_digit = [=](char c) { xpause("parse digit"); return c-'0'; };
      auto length_until_zero = [=](shared_blocking_queue<int> const& q)
          {  auto out = std::make_shared<blocking_queue<size_t> >();
             std::thread([=](shared_blocking_queue<int> in){
                     xpause("length until zero");
                     size_t s = 0;
                     for (int i; in->pop(i);)
                         if (i==0) { out->push(s); s=0; }
                         else s++;
                     out->close();
                 }, q).detach();
             return out;
          };
      auto product = [=](shared_blocking_queue<size_t> const& q) {
          return std::async([=](shared_blocking_queue<size_t> in){
                  xpause("product");
                  size_t product = 1, tmp;
                  while (in->pop(tmp)) product += tmp;
                  return product;
              }, q);
      };
      std::string s = "1 0 2 3 0 4 5 6 7 0";
      auto p = ((((pipeline<segment_monad>(segment_monad::from_range(begin(s), end(s)))
                   >> filter_spaces)
                  | parse_digit)
                 || length_until_zero)
                << product).get();
      std::cout << "pipeline initiated in "; pause(0);
      for (typename std::decay<decltype(*p)>::type::value_type c; p->pop(c);)
           std::cout << '|'<<c.get()<<'|'<<std::flush;
      std::cout << '\n';
      
      // output:
      //   pipeline initiated in thread id: 140673683539776
      //   product@140673683539776
      //   length until zero@140673632118528
      //   filter spaces@140673667000064
      //   filter spaces@140673667000064
      //   parse digit@140673640838912
      //   filter spaces@140673667000064
      //   filter spaces@140673667000064
      //   parse digit@140673640838912
      //   filter spaces@140673667000064
      //   filter spaces@140673667000064
      //   parse digit@140673640838912
      //   filter spaces@140673667000064
      //   filter spaces@140673667000064
      //   parse digit@140673640838912
      //   filter spaces@140673667000064
      //   filter spaces@140673667000064
      //   parse digit@140673640838912
      //   filter spaces@140673667000064
      //   filter spaces@140673667000064
      //   parse digit@140673640838912
      //   filter spaces@140673667000064
      //   filter spaces@140673667000064
      //   parse digit@140673640838912
      //   filter spaces@140673667000064
      //   filter spaces@140673667000064
      //   parse digit@140673640838912
      //   filter spaces@140673667000064
      //   filter spaces@140673667000064
      //   parse digit@140673640838912
      //   filter spaces@140673667000064
      //   parse digit@140673640838912
      //   |8|
  }
  {
      using std::begin; using std::end;
      const char* test[] =
          {"Error:1","all right","Error... not really an error",
           "Error", "Error:          trim   me      ", "Error: end of pipe"};
      (pipeline<segment_monad>(segment_monad::from_range(begin(test), end(test))) >>
       [](std::string const& s) { return boost::starts_with(s, "Error:") ? segment_monad::mreturn(s) : segment_monad::mempty<std::string>(); }
       | [](std::string const& s) { return s.substr(std::string("Error:").size()); }
       | [](std::string s) { boost::trim(s); return s; })
          << [](shared_blocking_queue<std::string> q) {
          for (std::string s; q->pop(s);)
              std::cout << s << '\n';
          return true; // currently needed
      };
      // output:
      //   1
      //   trim   me
      //   end of pipe
  }
}
