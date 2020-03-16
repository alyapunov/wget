/*
 * Copyright (c) 2020, Aleksandr Lyapunov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

// Construct an array of given type T by given initializers,
// grouping them if possible to pass to the T constructor.
// For example makeArray<std::string>(str1, size1, str2, str3, size3)
// will return std::array<std::string, 3> { {str1, size2}, {str2}, {str3, size3} };

template <class T, class ...ARGS>
auto makeArray(ARGS&& ...aArgs);

// Implementation

namespace details {

template <class T, class ArgsTuple, class Indexes, class Grouped>
struct IndexGrouper;

template<class T, class ...Done>
struct IndexGrouper<T, std::tuple<>, std::index_sequence<>, std::tuple<Done...>>
{
    using type = std::tuple<Done...>;
};

template <class T, class U, size_t I, class ...Done>
struct IndexGrouper<T, std::tuple<U>, std::index_sequence<I>, std::tuple<Done...>>
{
    using R = std::integer_sequence<size_t, I>;
    using type = std::tuple<Done..., R>;
};

template <class T, class A1, class A2, class ...AN, size_t I1, size_t I2, size_t ...IN, class ...Done>
struct IndexGrouper<T, std::tuple<A1, A2, AN...>, std::index_sequence<I1, I2, IN...>, std::tuple<Done...>>
{
    using R1 = std::integer_sequence<size_t, I1>;
    using R2 = std::integer_sequence<size_t, I1, I2>;
    using type1 = typename IndexGrouper<T, std::tuple<A2, AN...>, std::index_sequence<I2, IN...>, std::tuple<Done..., R1>>::type;
    using type2 = typename IndexGrouper<T, std::tuple<AN...>, std::index_sequence<IN...>, std::tuple<Done..., R2>>::type;
    using type = std::conditional_t<std::is_constructible_v<T, A1&&, A2&&>, type2, type1>;
};

template <class T, class A1, class A2, class A3, class ...AN, size_t I1, size_t I2, size_t I3, size_t ...IN, class ...Done>
struct IndexGrouper<T, std::tuple<A1, A2, A3, AN...>, std::index_sequence<I1, I2, I3, IN...>, std::tuple<Done...>>
{
    using R1 = std::integer_sequence<size_t, I1>;
    using R2 = std::integer_sequence<size_t, I1, I2>;
    using R3 = std::integer_sequence<size_t, I1, I2, I3>;
    using type1 = typename IndexGrouper<T, std::tuple<A2, A3, AN...>, std::index_sequence<I2, I3, IN...>, std::tuple<Done..., R1>>::type;
    using type2 = typename IndexGrouper<T, std::tuple<A3, AN...>, std::index_sequence<I3, IN...>, std::tuple<Done..., R2>>::type;
    using type3 = typename IndexGrouper<T, std::tuple<AN...>, std::index_sequence<IN...>, std::tuple<Done..., R3>>::type;
    using type = std::conditional_t<std::is_constructible_v<T, A1&&, A2&&, A3&&>, type3,
        std::conditional_t<std::is_constructible_v<T, A1&&, A2&&>, type2, type1>>;
};

template <class T, size_t ...I, class Tuple>
T constructByIndexes(std::integer_sequence<size_t, I...>, Tuple& t)
{
    return T(std::forward<typename std::tuple_element_t<I, Tuple>>(std::get<I>(t))...);
}

template <class T, class ...ArgGroups, class ...ARGS>
auto makeArray(std::tuple<ArgGroups...>, ARGS&&... aArgs) -> std::array<T, sizeof...(ArgGroups)>
{
    std::tuple<ARGS&&...> sArgs(std::forward<ARGS>(aArgs)...);
    return std::array<T, sizeof...(ArgGroups)>{constructByIndexes<T>(ArgGroups{}, sArgs)...};
}

} // namespace details {

template <class T, class ...ARGS>
inline auto makeArray(ARGS&&... aArgs)
{
    using tuple_t = std::tuple<ARGS&&...>;
    using ind_t = std::make_integer_sequence<size_t, sizeof ...(ARGS)>;
    using grouped_t = typename details::IndexGrouper<T, tuple_t, ind_t, std::tuple<>>::type;
    return details::makeArray<T>(grouped_t{}, std::forward<ARGS>(aArgs)...);
}
