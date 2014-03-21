#include <boost/monads/monad.hpp>

#include <memory>
#include <iostream>
#include <cassert>

namespace std { // need ADL
template <typename T, typename F>
std::unique_ptr<T> boost_mbind(std::unique_ptr<T> const& p, F&& fun)
{
  if (!p) return std::unique_ptr<T>();
  return fun(*p);
}
}

int main()
{
  std::unique_ptr<int> maybe(new int(4));
  auto inc = [](int i){return std::unique_ptr<int>(new int(i+1));};
  auto nullify = [](int){return std::unique_ptr<int>();};

  assert(boost::monads::detail::mbind_(boost::monads::detail::make_choice{}, std::unique_ptr<int>(), inc).get() == 0);
  assert(*boost::monads::mbind(maybe, inc) == 5);
  assert(*maybe == 4); // inc does not alter original
  assert(*(boost::monads::monad_pipe(maybe) >>= inc).unpipe() == 5);
  assert(*((boost::monads::monad_pipe(maybe) >>= inc) >>= inc).unpipe() == 6);
  assert((((boost::monads::monad_pipe(maybe) >>= inc) >>= nullify) >>= inc).unpipe().get() == 0);
}
