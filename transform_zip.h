#pragma once

#include <type_traits>
#include <boost/iterator/zip_iterator.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>

#include "execution_policy.h"

namespace emu::parallel {

template<class ExecutionPolicy, class ForwardIt1, class ForwardIt2,
    class UnaryOperation,
    // Disable if first argument is not an execution policy
    std::enable_if_t<execution::is_execution_policy_v<ExecutionPolicy>, int> = 0
>
void
transform(
    ExecutionPolicy policy,
    ForwardIt1 first1, ForwardIt1 last1,
    ForwardIt2 first2,
    UnaryOperation unary_op)
{
    // Create a zip_iterator to combine the two ranges
    auto last2 = first2 + (last1 - first1);
    auto first = boost::make_zip_iterator(
        std::make_tuple(first1, first2));
    auto last = boost::make_zip_iterator(
        std::make_tuple(last1, last2));
    // Forward to for_each
    for_each(policy, first, last, [unary_op](auto tuple) {
        std::get<1>(tuple) = unary_op(std::get<0>(tuple));
    });
}

template<class ForwardIt1, class ForwardIt2, class UnaryOperation>
void
transform(
    ForwardIt1 first1, ForwardIt1 last1,
    ForwardIt2 first2,
    UnaryOperation unary_op)
{
    transform(execution::default_policy,
        first1, last1, first2, unary_op);
}

template<class ExecutionPolicy, class ForwardIt1, class ForwardIt2,
    class ForwardIt3, class BinaryOperation,
    // Disable if first argument is not an execution policy
    std::enable_if_t<execution::is_execution_policy_v<ExecutionPolicy>, int> = 0
>
void
transform(
    ExecutionPolicy policy,
    ForwardIt1 first1, ForwardIt1 last1,
    ForwardIt2 first2,
    ForwardIt3 first3,
    BinaryOperation binary_op)
{
    // Create a zip_iterator to combine all three ranges
    auto last2 = first2 + (last1-first1);
    auto last3 = first3 + (last1-first1);
    auto first = boost::make_zip_iterator(
        std::make_tuple(first1, first2, first3));
    auto last = boost::make_zip_iterator(
        std::make_tuple(last1, last2, last3));
    // Forward to for_each
    for_each(policy, first, last, [binary_op](auto tuple){
        std::get<2>(tuple) = binary_op(std::get<0>(tuple), std::get<1>(tuple));
    });
}

template<class ForwardIt1, class ForwardIt2,
    class ForwardIt3, class BinaryOperation>
void
transform(
    ForwardIt1 first1, ForwardIt1 last1,
    ForwardIt2 first2,
    ForwardIt3 first3,
    BinaryOperation binary_op)
{
    transform(execution::default_policy,
        first1, last1, first2, first3, binary_op);
}

} // end namespace emu::parallel
