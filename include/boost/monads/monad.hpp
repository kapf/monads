// Boost.Monads
//

#ifndef BOOST_MONADS_MONAD_HPP
#define BOOST_MONADS_MONAD_HPP

#include <utility>

namespace boost { namespace monads {

// The Haskell definition of a monad contains the following functions:
//   (>>=) :: forall a b. m a -> (a -> m b) -> m b
//   (>>) :: forall a b. m a -> m b -> m b
//   return :: a -> m a
//   fail :: String -> m a
//
// Note the following:
//  (1) (>>) can be defined in terms of >>= and return
//  (2) fail is basically a hack and not part of the mathematical definition of a monad.
//           It was added only to express pattern-match failure in a do expression.
//
// So I propose for now that Monads only have to support >>= and return.
// We may later change that to allow more optimization possibilities, such as
// impure monads that don't have to return a copy on each mbind call.
//
// template <typename T>
// struct monad_archetype {
//     // (>>=) :: m a -> (a -> m b) -> m b -- "return a copy with fun applied on each element"
//     template <typename F>
//     auto mbind(F&& fun) const -> monad_archetype<decltype(fun(std::declval<T>()))>;
//     
//     // return :: a -> m a -- "construct a monad with one element"
//     static monad_archetype<T> mreturn(T const&&);
// };
// 
// // if mreturn is a free function, the return type is indicated with monad_type<return type>
// template <typename> struct monad_type;
// 
// template <typename T, typename F>
// auto mbind(monad_archetype<T> const& monad, F&& fun)
//     -> monad_archetype<decltype(fun(std::declval<T>()))>;
//
// template <typename T>
// monad_archetype<T> mreturn(monad_type<monad_archetype<T> >, T&&);
//
//
// A monad needs the functions mbind and mreturn.
// The overloading priorities are as follows:
//   (1) mbind/mreturn member function
//   (2) mbind/mreturn free functions found via adl
//   (3) default definitions inside boost::monad

template <typename T> struct monad_type {};

    namespace extend {
    }

namespace detail {

  struct not_a_real_type{};
  void do_mbind(not_a_real_type);
  void do_mreturn(not_a_real_type);

template<int I> struct choice : choice<I-1> {};
template<> struct choice<0> {};

using make_choice   = choice<3>;
using first_choice  = choice<2>;
using second_choice = choice<1>;
using third_choice  = choice<0>;

  using namespace boost::monads::extend;

template <typename M, typename F>
auto mbind_(first_choice, M const& monad, F&& fun)
    -> decltype(monad.mbind(std::forward<F>(fun)))
{
    return monad.mbind(std::forward<F>(fun));
}

template <typename M, typename F>
auto mbind_(second_choice, M const& monad, F&& fun)
    -> decltype(boost_mbind(monad, std::forward<F>(fun)))
{
    return boost_mbind(monad, std::forward<F>(fun));
}

template <typename M, typename F>
auto mbind_(third_choice, M const& monad, F&& fun)
    -> decltype(detail::do_mbind(monad, std::forward<F>(fun)))
{
    return detail::do_mbind(monad, std::forward<F>(fun));
}

template <typename M, typename T>
auto mreturn_(first_choice, monad_type<M>, T&& elem)
    -> decltype(M::mreturn(std::forward<T>(elem)))
{
    return M::mreturn(std::forward<T>(elem));
}

template <typename M, typename T>
auto mreturn_(second_choice, monad_type<M>, T&& elem)
    -> decltype(mreturn(monad_type<M>{}, std::forward<T>(elem)))
{
    return mreturn(monad_type<M>{}, std::forward<T>(elem));
}

template <typename M, typename T>
auto mreturn_(third_choice, monad_type<M>, T&& elem)
    -> decltype(detail::do_mreturn(monad_type<M>{}, std::forward<T>(elem)))
{
    return detail::do_mreturn(monad_type<M>{}, std::forward<T>(elem));
}
} // namespace detail


template <typename M, typename F>
auto mbind(M const& monad, F&& fun)
    -> decltype(detail::mbind_(detail::make_choice{}, monad, std::forward<F>(fun)))
{
    return detail::mbind_(detail::make_choice{}, monad, std::forward<F>(fun));
}

template <typename M, typename T>
auto mreturn(T&& elem)
    -> decltype(detail::mreturn_(detail::make_choice{}, monad_type<M>{}, std::forward<T>(elem)))
{
    return detail::mreturn_(detail::make_choice{}, monad_type<M>{}, std::forward<T>(elem));
}

template <typename M>
struct monad_piper {
    template <typename... Args>
    monad_piper(Args&&... args) : monad(std::forward<Args>(args)...) {}
    M monad;

    M unpipe() { return std::forward<M>(monad); }
};

template <typename M, typename N>
auto operator>>=(monad_piper<M>&& lhs, N&& rhs)
    -> monad_piper<decltype(mbind(lhs.monad, rhs))>
{
    using Ret=monad_piper<decltype(mbind(lhs.monad, rhs))>;
    return Ret(mbind(lhs.monad, rhs));
}

template <typename M>
monad_piper<M> monad_pipe(M&& m)
{
    return monad_piper<M>(m);
}

}} // namespace boost::monads

#endif // BOOST_MONADS_MONAD_HPP
 
