/*
  Copyright 2021 Karl Wiberg

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef FRZ_ASSERT_HH_
#define FRZ_ASSERT_HH_

/*

  This file defines three families of assertion macros. They behave differently
  in release mode (NDEBUG defined) and in debug mode (NDEBUG not defined).

  Release mode:

    FRZ_CHECK(p) evaluates `p` and terminates the process unless `p` is true.

    FRZ_ASSERT(p) does nothing.

    FRZ_ASSUME(p) may or may not evaluate `p`. The compiler is allowed to
    assume that `p` is true, and may use that when optimizing.

  Debug mode:

    FRZ_CHECK(p), FRZ_ASSERT(p), and FRZ_ASSUME(p) all evaluate `p` and
    terminate the process unless `p` is true.

  So, FRZ_CHECK(p) behaves the same in release and debug mode, always evaluates
  `p`, and always terminates if `p` is false. Use when a clean crash is
  necessary even in release builds. Since `p` is always evaluated, it may have
  visible side effects.

  FRZ_ASSERT(p) is a standard assert. Use it in the majority of cases. Since
  `p` is not evaluated in release mode, it should not have side effects.

  FRZ_ASSUME(p) is a standard assert, but additionally allows the compiler to
  assume that `p` is true in release mode. This may allow the compiler to
  optimize better. Use only with simple, side-effect free predicates.

  Each principal assertion macro has six companions. They are

    FRZ_ASSERT_EQ(a, b): asserts that a == b
    FRZ_ASSERT_NE(a, b): asserts that a != b
    FRZ_ASSERT_LT(a, b): asserts that a < b
    FRZ_ASSERT_LE(a, b): asserts that a <= b
    FRZ_ASSERT_GE(a, b): asserts that a >= b
    FRZ_ASSERT_GT(a, b): asserts that a > b

  and similar for FRZ_CHECK_* and FRZ_ASSUME_*. They are functionally
  equivalent, to calling the corresponding principal macro (e.g., FRZ_CHECK(x
  == 12) instead of FRZ_CHECK_EQ(x, 12)), but produce better error messages
  since they can print the values of `a` and `b`.

*/

#include <concepts>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace frz {
namespace frz_assert_impl {

#ifdef NDEBUG
inline constexpr bool kAssertActive = false;
#else
inline constexpr bool kAssertActive = true;
#endif

// TODO(github.com/kwiberg/frz/issues/5): Use std::source_location instead when
// stable versions of GCC and/or clang support it.
struct SourceLocation {
    const char* file;
    int line;
    friend std::ostream& operator<<(std::ostream& out, SourceLocation loc) {
        return out << loc.file << ":" << loc.line;
    }
};
#define FRZ_IMPL_CURRENT_LOCATION() \
    (::frz::frz_assert_impl::SourceLocation{.file = __FILE__, .line = __LINE__})

[[noreturn]] inline void AssertFail(SourceLocation loc, std::string_view expr) {
    std::cerr << "*** ASSERTION FAILURE at " << loc << ": " << expr
              << std::endl;
    std::abort();
}

[[noreturn]] void AssertFail(SourceLocation loc, std::string_view op,
                             std::string_view a_expr, std::string_view b_expr,
                             const auto& a, const auto& b) {
    std::cerr << "*** ASSERTION FAILURE at " << loc << ": " << op << std::endl
              << "  Left-hand side is " << a_expr << std::endl
              << "    ==> " << a << std::endl
              << "  Right-hand side is " << b_expr << std::endl
              << "    ==> " << b << std::endl;
    std::abort();
}

template <std::integral T>
T CheckCast(SourceLocation loc, std::string_view target_type_name,
            std::integral auto x) {
    if (std::cmp_less(x, std::numeric_limits<T>::min()) ||
        std::cmp_greater(x, std::numeric_limits<T>::max())) {
        std::cerr << "*** ASSERTION FAILURE at " << loc
                  << ": out of range for type " << target_type_name << std::endl
                  << "  Input value is  " << x << std::endl;
        std::abort();
    } else {
        return static_cast<T>(x);
    }
}

#define FRZ_IMPL_COMPARATOR(name, op, int_fun)                     \
    struct name {                                                  \
        static constexpr bool Good(const auto& a, const auto& b) { \
            if constexpr (std::is_integral_v<                      \
                              std::remove_cvref_t<decltype(a)>> && \
                          std::is_integral_v<                      \
                              std::remove_cvref_t<decltype(b)>>) { \
                return int_fun(a, b);                              \
            } else {                                               \
                return a op b;                                     \
            }                                                      \
        }                                                          \
        static constexpr std::string_view kName = #op;             \
    }
FRZ_IMPL_COMPARATOR(Eq, ==, std::cmp_equal);
FRZ_IMPL_COMPARATOR(Ne, !=, std::cmp_not_equal);
FRZ_IMPL_COMPARATOR(Lt, <, std::cmp_less);
FRZ_IMPL_COMPARATOR(Le, <=, std::cmp_less_equal);
FRZ_IMPL_COMPARATOR(Ge, >=, std::cmp_greater_equal);
FRZ_IMPL_COMPARATOR(Gt, >, std::cmp_greater);

}  // namespace frz_assert_impl

#ifdef __GNUC__
#define FRZ_IMPL_ASSUME(p)           \
    do {                             \
        if (!(p)) {                  \
            __builtin_unreachable(); \
        }                            \
    } while (0)
#else
#define FRZ_IMPL_ASSUME(p) \
    do {                   \
    } while (0)
#endif

#define FRZ_CHECK(p)                                                        \
    do {                                                                    \
        if (!(p)) {                                                         \
            ::frz::frz_assert_impl::AssertFail(FRZ_IMPL_CURRENT_LOCATION(), \
                                               #p);                         \
        }                                                                   \
    } while (0)

#define FRZ_IMPL_CHECK_OP(op, a, b)                                         \
    do {                                                                    \
        if (!op::Good(a, b)) {                                              \
            ::frz::frz_assert_impl::AssertFail(FRZ_IMPL_CURRENT_LOCATION(), \
                                               op::kName, #a, #b, a, b);    \
        }                                                                   \
    } while (0)

#define FRZ_CHECK_EQ(a, b) FRZ_IMPL_CHECK_OP(::frz::frz_assert_impl::Eq, a, b)
#define FRZ_CHECK_NE(a, b) FRZ_IMPL_CHECK_OP(::frz::frz_assert_impl::Ne, a, b)
#define FRZ_CHECK_LT(a, b) FRZ_IMPL_CHECK_OP(::frz::frz_assert_impl::Lt, a, b)
#define FRZ_CHECK_LE(a, b) FRZ_IMPL_CHECK_OP(::frz::frz_assert_impl::Le, a, b)
#define FRZ_CHECK_GE(a, b) FRZ_IMPL_CHECK_OP(::frz::frz_assert_impl::Ge, a, b)
#define FRZ_CHECK_GT(a, b) FRZ_IMPL_CHECK_OP(::frz::frz_assert_impl::Gt, a, b)

#define FRZ_CHECK_CAST(T, x) \
    (::frz::frz_assert_impl::CheckCast<T>(FRZ_IMPL_CURRENT_LOCATION(), #T, (x)))

#define FRZ_ASSERT(p)                                          \
    do {                                                       \
        if constexpr (::frz::frz_assert_impl::kAssertActive) { \
            FRZ_CHECK(p);                                      \
        }                                                      \
    } while (0)

#define FRZ_IMPL_ASSERT_OP(op, a, b)                           \
    do {                                                       \
        if constexpr (::frz::frz_assert_impl::kAssertActive) { \
            FRZ_IMPL_CHECK_OP(op, a, b);                       \
        }                                                      \
    } while (0)

#define FRZ_ASSERT_EQ(a, b) FRZ_IMPL_ASSERT_OP(::frz::frz_assert_impl::Eq, a, b)
#define FRZ_ASSERT_NE(a, b) FRZ_IMPL_ASSERT_OP(::frz::frz_assert_impl::Ne, a, b)
#define FRZ_ASSERT_LT(a, b) FRZ_IMPL_ASSERT_OP(::frz::frz_assert_impl::Lt, a, b)
#define FRZ_ASSERT_LE(a, b) FRZ_IMPL_ASSERT_OP(::frz::frz_assert_impl::Le, a, b)
#define FRZ_ASSERT_GE(a, b) FRZ_IMPL_ASSERT_OP(::frz::frz_assert_impl::Ge, a, b)
#define FRZ_ASSERT_GT(a, b) FRZ_IMPL_ASSERT_OP(::frz::frz_assert_impl::Gt, a, b)

#define FRZ_ASSERT_CAST(T, x)                                       \
    (::frz::frz_assert_impl::kAssertActive ? FRZ_CHECK_CAST(T, (x)) \
                                           : static_cast<T>(x))

#define FRZ_ASSUME(p)                                          \
    do {                                                       \
        if constexpr (::frz::frz_assert_impl::kAssertActive) { \
            FRZ_CHECK(p);                                      \
        } else {                                               \
            FRZ_IMPL_ASSUME(p);                                \
        }                                                      \
    } while (0)

#define FRZ_IMPL_ASSUME_OP(op, a, b)                           \
    do {                                                       \
        if constexpr (::frz::frz_assert_impl::kAssertActive) { \
            FRZ_IMPL_CHECK_OP(op, a, b);                       \
        } else {                                               \
            FRZ_IMPL_ASSUME(op::Good(a, b));                   \
        }                                                      \
    } while (0)

#define FRZ_ASSUME_EQ(a, b) FRZ_IMPL_ASSUME_OP(::frz::frz_assert_impl::Eq, a, b)
#define FRZ_ASSUME_NE(a, b) FRZ_IMPL_ASSUME_OP(::frz::frz_assert_impl::Ne, a, b)
#define FRZ_ASSUME_LT(a, b) FRZ_IMPL_ASSUME_OP(::frz::frz_assert_impl::Lt, a, b)
#define FRZ_ASSUME_LE(a, b) FRZ_IMPL_ASSUME_OP(::frz::frz_assert_impl::Le, a, b)
#define FRZ_ASSUME_GE(a, b) FRZ_IMPL_ASSUME_OP(::frz::frz_assert_impl::Ge, a, b)
#define FRZ_ASSUME_GT(a, b) FRZ_IMPL_ASSUME_OP(::frz::frz_assert_impl::Gt, a, b)

}  // namespace frz

#endif  // FRZ_ASSERT_HH_
