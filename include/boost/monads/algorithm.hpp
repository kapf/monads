// Boost.Monads.Algorithm
//

#ifndef BOOST_MONADS_ALGORITHM_HPP
#define BOOST_MONADS_ALGORITHM_HPP

#include "monad.hpp"
#include <functional>

namespace boost { namespace monads {

struct identity_
{
    template <typename T>
    T operator() (T&& x) const
    {
        return x;
    }
};
identity_ identity() { return identity_{}; }

namespace detail {
template <typename M, typename F>
struct return_after_f {
    F f;
    template <typename A>
    auto operator()(A&& a)
        -> decltype(mreturn<M>(f(std::forward<A>(a))))
    {
        return mreturn<M>(f(std::forward<A>(a)));
    }
};
}

template <typename ResM, typename M, typename F>
auto fmap(M&& m_a, F&& a_to_b)
    -> decltype(mbind(std::forward<M>(m_a), detail::return_after_f<ResM, typename std::decay<F>::type>{std::forward<F>(a_to_b)}))
{
    return mbind(std::forward<M>(m_a), detail::return_after_f<ResM,
                 typename std::decay<F>::type>{std::forward<F>(a_to_b)});
}

template <typename M_M_a>
auto join(M_M_a&& m)
    -> decltype(mbind(std::forward<M_M_a>(m), identity()))
{
    return mbind(std::forward<M_M_a>(m), identity());
}

namespace detail {
template <typename M, typename F>
struct bind_first_for_fmap {
    F f;
    template <typename A>
    auto operator()(A&& a)const
        -> decltype(fmap<M>(std::forward<A>(a), f))
    {
        return fmap<M>(std::forward<A>(a), f);
    }
};
}

template <typename M, typename F>
auto liftm(F&& f)
    -> detail::bind_first_for_fmap<M, typename std::decay<F>::type>
{
    return detail::bind_first_for_fmap<M, typename std::decay<F>::type>{std::forward<F>(f)};
}

}} // namespace boost::monads

#endif // BOOST_MONADS_ALGORITHM_HPP
