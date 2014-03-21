// Boost.Control.Monad
//

#ifndef BOOST_MONADS_CONTROLMONAD_HPP
#define BOOST_MONADS_CONTROLMONAD_HPP

#include <functional>
#include "monad.hpp"

namespace boost { namespace monads {

template <typename Fun, typename... Args>
auto call(Fun&& fun, Args&&... args)
    -> decltype(std::forward<Fun>(fun)(std::forward<Args>(args)...))
{
    return std::forward<Fun>(fun)(std::forward<Args>(args)...);
}

// ContMonad
template <typename Cont, typename Callable>
auto run_cont(Cont&& cont, Callable&& c)
    -> decltype(call(std::forward<Cont>(cont), std::forward<Callable>(c)))
{
    return call(std::forward<Cont>(cont), std::forward<Callable>(c));
}

struct identity_
{
    template <typename T>
    T&& operator() (T&& x) const
    {
        return x;
    }
};
identity_ identity() { return identity_{}; }

namespace detail {
// hack around the fact that we cannot return lambdas.

template <typename F, typename BToR>
struct take_a_return_r_t_storing_f_and_b_to_r {
    F f;
    BToR b_to_r;
    template <typename A>
    auto operator()(A&& a) const
        -> decltype(call(call(f, std::forward<A>(a)), b_to_r))
    {
        return call(call(f, std::forward<A>(a)), b_to_r);
    }
};

template <typename S, typename F>
struct take_b_to_r_return_r_storing_s_and_f {
    S s;
    F f;
    template <typename BToR>
    auto operator()(BToR&& b_to_r) const
        -> decltype(call(s, take_a_return_r_t_storing_f_and_b_to_r<F&&, BToR&&>{f, std::forward<BToR>(b_to_r)}))
    {
        return call(s, take_a_return_r_t_storing_f_and_b_to_r<F&&, BToR&&>{f, std::forward<BToR>(b_to_r)});
    }
};
} // namespace detail

template <typename T>
class cont_monad
{
    T wrapped;
public:
    cont_monad(T&& wrapped)
        : wrapped(std::forward<T>(wrapped))
    {
    }

    template <typename Callable>
    auto operator()(Callable&& callable) const
        -> decltype(wrapped(std::forward<Callable>(callable)))
    {
        return wrapped(std::forward<Callable>(callable));
    }

    // first argument  s :: cont_monad ((a->r)->r)
    // second argument f :: a -> (cont_monad ((b->r)->r))
    // return type       :: cont_monad ((b->r)->r)
    template <typename MakeContMonad>
    auto mbind(MakeContMonad&& f) const
        -> decltype(make_cont_monad(detail::take_b_to_r_return_r_storing_s_and_f<cont_monad, MakeContMonad>{*this,std::forward<MakeContMonad>(f)}))
    {
        using Ret = detail::take_b_to_r_return_r_storing_s_and_f<cont_monad, MakeContMonad>;
        return make_cont_monad(Ret{*this,std::forward<MakeContMonad>(f)});
    }
};

template <typename T>
cont_monad<typename std::decay<T>::type> make_cont_monad(T&& f)
{
    return cont_monad<typename std::decay<T>::type>(std::forward<T>(f));
}

namespace detail {
template <typename T>
struct apply_cps {
    T value;
    template <typename Callable>
    auto operator()(Callable&& callable) const
        -> decltype(std::forward<Callable>(callable)(value))
    {
        return std::forward<Callable>(callable)(value);
    }
};
} // namespace detail

struct cps;
struct cps {
    template <typename T>
    static auto mreturn(T&& x)
        -> cont_monad<detail::apply_cps<typename std::decay<T>::type> >
    {
        return make_cont_monad(detail::apply_cps<typename std::decay<T>::type>{x});
    }
};

namespace detail {
template <typename H, typename A>
struct always_call_h {
    H const& h;
    A a;
    template <typename... Args>
    auto operator()(Args&&...) const
        -> decltype(call(h, a))
    {
        return call(h, a);
    }
};

template <typename H>
struct a_and_h_to_cont {
    H const& h;
    template <typename A>
    auto operator()(A a) const
        -> decltype(make_cont_monad(always_call_h<H, A>{h, std::move(a)}))
    {
        return make_cont_monad(always_call_h<H, A>{h, std::move(a)});
    }
};

template <typename F>
struct call_cc_cont {
    F f;
    template <typename H>
    auto operator()(H&& h) const
        -> decltype(run_cont(call(f, a_and_h_to_cont<H>{h}), h))
    {   
        return run_cont(call(f, a_and_h_to_cont<H>{h}), h);
    }
};
} // namespace detail

template <typename CallCC>
auto call_cc(CallCC&& f)
    -> decltype(make_cont_monad(detail::call_cc_cont<typename std::decay<CallCC>::type>{f}))
{
    return make_cont_monad(detail::call_cc_cont<typename std::decay<CallCC>::type>{f});
}

template <typename R, typename A>
class type_erased_cont_monad {
    std::function<R(std::function<R(A)>)> wrapped;
public:
    type_erased_cont_monad(std::function<R(std::function<R(A)>)> wrapped)
        : wrapped(wrapped)
    {
    }

    template <typename T>
    type_erased_cont_monad(cont_monad<T> orig)
        : wrapped([=](std::function<R(A)> f){return orig(f);})
    {
    }


    R operator()(std::function<R(A)>&& f) const
    {
        using namespace std::placeholders;
        return wrapped(f);
    }

    template <typename MakeContMonad>
    auto mbind(MakeContMonad&& f) const
        -> decltype(make_cont_erased(detail::take_b_to_r_return_r_storing_s_and_f<type_erased_cont_monad, MakeContMonad>{*this,std::forward<MakeContMonad>(f)}))
    {
        using Ret = detail::take_b_to_r_return_r_storing_s_and_f<type_erased_cont_monad, MakeContMonad>;
        return make_cont_erased(Ret{*this,std::forward<MakeContMonad>(f)});
    }
};

template <typename R, typename A>
type_erased_cont_monad<R, A>
make_cont_erased(std::function<R(std::function<R(A)>)> f)
{
    return type_erased_cont_monad<R, A>(std::move(f));
}
template <typename R, typename A>
struct erased_cps;
template <typename R, typename A>
struct erased_cps {
    static auto mreturn(A a)
        -> type_erased_cont_monad<R, A>
    {
        std::function<R(std::function<R(A)>)> apply([=](std::function<R(A)> f){return f(a);});
        return make_cont_erased(std::move(apply));
    }
};

}} // namespace boost::monads

#endif // BOOST_MONADS_CONTROLMONAD_HPP
 
