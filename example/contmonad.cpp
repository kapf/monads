#include <boost/monads/monad.hpp>
#include <boost/monads/controlmonad.hpp>

#include <memory>
#include <iostream>
#include <cassert>

namespace mon = boost::monads;

auto square(int i)
  -> decltype(mon::mreturn<mon::cps>(i*i))
{
  return mon::mreturn<mon::cps>(i*i);
}

struct printer {
  printer(const char *msg = nullptr) : msg(msg) {}
  const char *msg;
  void operator()(int i) const
  {
    if (msg)
      std::cout << msg << ": ";
    std::cout << i << '\n';
  }
};

struct assert_equal {
  int equal_to;
  void operator()(int i) const
  {
    if (i != equal_to)
      std::cerr << "assert failed: " << i << " == " << equal_to << '\n';
  }
};

auto loud_square(int i)
  -> decltype(mon::mreturn<mon::cps>(i*i))
{
  std::cout << "evaluate square of " << i<< " => " << i*i << '\n';
  return mon::mreturn<mon::cps>(i*i);
}

struct call_cc_test {
  int i;
  template <typename K>
  mon::type_erased_cont_monad<void,int> operator()(K&& k) const
  { // chaining with >>= would work too, no need for a `real' return
    if (i == 2) {
      std::cout << "i == 2\n";
      return k(-1);
    }
    std::cout << "still here...\n";
    return mon::mreturn<mon::erased_cps<void,int> >(i*i);
  }
};

int main()
{
  auto inc = [](int i){return mon::mreturn<mon::cps>(i+1);};
  auto sqr = [](int i){return square(i);};
  auto loud_sqr = [](int i){return loud_square(i);};
  struct loud_sqr_ {
    auto operator()(int i) const -> decltype(loud_square(i))
    { return loud_square(i); }
  };
  {
    std::cout << run_cont(square(7), mon::identity()) << '\n';
    run_cont(square(7), printer{});
    run_cont(square(7), assert_equal{49});
    mon::run_cont((((mon::monad_pipe(square(2)) >>= inc) >>= sqr) >>= inc).unpipe(),
                  assert_equal{((2*2)+1)*5+1});
  }
  {
    mon::run_cont(mon::mbind(square(2), inc), assert_equal{5});
  }
  {
    auto x = loud_square(7);
    std::cout << "midway there\n";
    run_cont(x, printer{"result (strict)"});
    std::cout <<"---\n";
    auto y = (mon::monad_pipe(mon::mreturn<mon::cps>(7))>>=loud_sqr_{}).unpipe();
    std::cout << "midway there\n";
    run_cont(y, printer{"result (lazy)"});
  }
  {
    
    using namespace std::placeholders;
    std::cout << "---------\n";
    (mon::monad_pipe(mon::call_cc(call_cc_test{2}))
     >>= loud_sqr_{}).unpipe()(printer{});
    std::cout << "---------\n";
    mon::run_cont((mon::monad_pipe(mon::call_cc(call_cc_test{3}))
                                  >>= loud_sqr).unpipe(), printer{});
    std::cout << "---------\n";
  }
}
