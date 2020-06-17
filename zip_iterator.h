#pragma once

#include <cassert>
#include <iterator>
#include <tuple>
#include "intrinsics.h"

namespace emu {

// Could have used Boost's zip_iterator, but it has some problems:
// 1. Boost compares all elements of the iterator tuple. But zipping iterators
// together doesn't make sense unless the ranges are the same length, so we can
// get away with just comparing the first one. This greatly improves efficiency.
// Hopefully we can even optimize out the other end iterators, reducing register
// pressure.
// 2. We may need to add some special functionality, like converting an iterator
// to a pointer in a standard way for cilk_migrate_hint.
// 3. Adds a boost dependency. Talk to your kids about Boost, before someone
// else does.

// Keeping it simple for now. Eventually we should allow an arbitrary tuple of
// iterators, but this greatly increases the complexity of the code!
// Make it look like we implemented zip_iterator using a variadic template
template<class... Args>
class zip_iterator;


template<class Iter1, class Iter2>
class zip_iterator<Iter1, Iter2>
{
public:
    // Standard iterator typedefs for interop with C++ algorithms
    using self_type = zip_iterator;
    // FIXME think hard about these
    using iterator_category = typename std::iterator_traits<Iter1>::iterator_category;
    using value_type = std::tuple<
        typename std::iterator_traits<Iter1>::value_type,
        typename std::iterator_traits<Iter2>::value_type
    >;
    using difference_type = typename std::iterator_traits<Iter1>::difference_type;
    using pointer = std::tuple<
        typename std::iterator_traits<Iter1>::pointer,
        typename std::iterator_traits<Iter2>::pointer
    >;
    using reference = std::tuple<
        typename std::iterator_traits<Iter1>::reference,
        typename std::iterator_traits<Iter2>::reference
    >;
private:
    Iter1 iter1_;
    Iter2 iter2_;
public:

    explicit zip_iterator(Iter1 iter1, Iter2 iter2)
        : iter1_(iter1)
        , iter2_(iter2)
    {}

    reference  operator*()        { return {*iter1_, *iter2_}; }
    reference  operator*() const  { return {*iter1_, *iter2_}; }
    pointer    operator->()       { return {iter1_, iter2_}; }
    pointer    operator->() const { return {iter1_, iter2_}; }
    reference  operator[](difference_type i)    { return *(*(this) + i); }

    self_type& operator+=(difference_type n)
    {
        iter1_ += n;
        iter2_ += n;
        return *this;
    }
    self_type& operator-=(difference_type n)    { return operator+=(-n); }
    self_type& operator++()                     { return operator+=(+1); }
    self_type& operator--()                     { return operator+=(-1); }
    self_type  operator++(int)            { self_type tmp = *this; this->operator++(); return tmp; }
    self_type  operator--(int)            { self_type tmp = *this; this->operator--(); return tmp; }

    // Compare first iterator only
    friend bool
    operator==(const self_type& lhs, const self_type& rhs) { return lhs.iter1_ == rhs.iter1_; }
    friend bool
    operator!=(const self_type& lhs, const self_type& rhs) { return lhs.iter1_ != rhs.iter1_; }
    friend bool
    operator< (const self_type& lhs, const self_type& rhs) { return lhs.iter1_ <  rhs.iter1_; }
    friend bool
    operator> (const self_type& lhs, const self_type& rhs) { return lhs.iter1_ >  rhs.iter1_; }
    friend bool
    operator<=(const self_type& lhs, const self_type& rhs) { return lhs.iter1_ <= rhs.iter1_; }
    friend bool
    operator>=(const self_type& lhs, const self_type& rhs) { return lhs.iter1_ >= rhs.iter1_; }

    // Add/subtract integer to iterator
    friend self_type
    operator+ (const self_type& iter, difference_type n)
    {
        self_type tmp = iter;
        tmp += n;
        return tmp;
    }
    friend self_type
    operator+ (difference_type n, const self_type& iter)
    {
        self_type tmp = iter;
        tmp += n;
        return tmp;
    }
    friend self_type
    operator- (const self_type& iter, difference_type n)
    {
        self_type tmp = iter;
        tmp -= n;
        return tmp;
    }
    friend self_type
    operator- (difference_type n, const self_type& iter)
    {
        self_type tmp = iter;
        tmp -= n;
        return tmp;
    }

    // Difference between iterators
    friend difference_type
    operator- (const self_type& lhs, const self_type& rhs)
    {
        return lhs.iter1_ - rhs.iter1_;
    }

    template<size_t I, class I1, class I2>
    friend auto std::get(emu::zip_iterator<I1, I2>& self);

    // TODO Provide overloads for atomic increment
};


template<class Iter1, class Iter2, class Iter3>
class zip_iterator<Iter1, Iter2, Iter3>
{
public:
    // Standard iterator typedefs for interop with C++ algorithms
    using self_type = zip_iterator;
    // FIXME think hard about these
    using iterator_category = typename std::iterator_traits<Iter1>::iterator_category;
    using value_type = std::tuple<
        typename std::iterator_traits<Iter1>::value_type,
        typename std::iterator_traits<Iter2>::value_type,
        typename std::iterator_traits<Iter3>::value_type
    >;
    using difference_type = typename std::iterator_traits<Iter1>::difference_type;
    using pointer = std::tuple<
        typename std::iterator_traits<Iter1>::pointer,
        typename std::iterator_traits<Iter2>::pointer,
        typename std::iterator_traits<Iter3>::pointer
    >;
    using reference = std::tuple<
        typename std::iterator_traits<Iter1>::reference,
        typename std::iterator_traits<Iter2>::reference,
        typename std::iterator_traits<Iter3>::reference
    >;
private:
    Iter1 iter1_;
    Iter2 iter2_;
    Iter3 iter3_;
public:

    explicit zip_iterator(Iter1 iter1, Iter2 iter2, Iter3 iter3)
        : iter1_(iter1)
        , iter2_(iter2)
        , iter3_(iter3)
    {}

    reference  operator*()        { return {*iter1_, *iter2_, *iter3_}; }
    reference  operator*() const  { return {*iter1_, *iter2_, *iter3_}; }
    pointer    operator->()       { return {iter1_, iter2_, iter3_}; }
    pointer    operator->() const { return {iter1_, iter2_, iter3_}; }
    // FIXME
    reference  operator[](difference_type i)    { return *(*(this) + i); }

    self_type& operator+=(difference_type n)
    {
        iter1_ += n;
        iter2_ += n;
        iter3_ += n;
        return *this;
    }
    self_type& operator-=(difference_type n)    { return operator+=(-n); }
    self_type& operator++()                     { return operator+=(+1); }
    self_type& operator--()                     { return operator+=(-1); }
    self_type  operator++(int)            { self_type tmp = *this; this->operator++(); return tmp; }
    self_type  operator--(int)            { self_type tmp = *this; this->operator--(); return tmp; }

    // Compare first iterator only
    friend bool
    operator==(const self_type& lhs, const self_type& rhs) { return lhs.iter1_ == rhs.iter1_; }
    friend bool
    operator!=(const self_type& lhs, const self_type& rhs) { return lhs.iter1_ != rhs.iter1_; }
    friend bool
    operator< (const self_type& lhs, const self_type& rhs) { return lhs.iter1_ <  rhs.iter1_; }
    friend bool
    operator> (const self_type& lhs, const self_type& rhs) { return lhs.iter1_ >  rhs.iter1_; }
    friend bool
    operator<=(const self_type& lhs, const self_type& rhs) { return lhs.iter1_ <= rhs.iter1_; }
    friend bool
    operator>=(const self_type& lhs, const self_type& rhs) { return lhs.iter1_ >= rhs.iter1_; }

    // Add/subtract integer to iterator
    friend self_type
    operator+ (const self_type& iter, difference_type n)
    {
        self_type tmp = iter;
        tmp += n;
        return tmp;
    }
    friend self_type
    operator+ (difference_type n, const self_type& iter)
    {
        self_type tmp = iter;
        tmp += n;
        return tmp;
    }
    friend self_type
    operator- (const self_type& iter, difference_type n)
    {
        self_type tmp = iter;
        tmp -= n;
        return tmp;
    }
    friend self_type
    operator- (difference_type n, const self_type& iter)
    {
        self_type tmp = iter;
        tmp -= n;
        return tmp;
    }

    // Difference between iterators
    friend difference_type
    operator- (const self_type& lhs, const self_type& rhs)
    {
        return lhs.iter1_ - rhs.iter1_;
    }

    template<size_t I, class I1, class I2, class I3>
    friend auto std::get(emu::zip_iterator<I1, I2, I3>& self);

    // TODO Provide overloads for atomic increment
};

// Convenience functions for creating a zip iterator

template<class Iterator1, class Iterator2>
zip_iterator<Iterator1, Iterator2>
make_zip_iterator(Iterator1 iter1, Iterator2 iter2)
{
    return zip_iterator<Iterator1, Iterator2>(iter1, iter2);
}

template<class Iterator1, class Iterator2, class Iterator3>
zip_iterator<Iterator1, Iterator2, Iterator3>
make_zip_iterator(Iterator1 iter1, Iterator2 iter2, Iterator3 iter3)
{
    return zip_iterator<Iterator1, Iterator2, Iterator3>(iter1, iter2, iter3);
}


} // end namespace emu


// Specializations of std::get, so we can fetch the stored iterators
// as if this were a tuple
namespace std {

template<size_t I, class Iter1, class Iter2>
auto get(emu::zip_iterator<Iter1, Iter2>& self)
{
    static_assert(I < 2);
    if constexpr (I == 0) { return self.iter1_; }
    if constexpr (I == 1) { return self.iter2_; }
}

template<size_t I, class Iter1, class Iter2, class Iter3>
auto get(emu::zip_iterator<Iter1, Iter2, Iter3>& self)
{
    static_assert(I < 3);
    if constexpr (I == 0) { return self.iter1_; }
    if constexpr (I == 1) { return self.iter2_; }
    if constexpr (I == 2) { return self.iter3_; }
}

} // end namespace std
