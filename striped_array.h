#pragma once

#include <emu_c_utils/emu_c_utils.h>

#include "replicated.h"
#include "out_of_memory.h"
//#include "copy.h"

namespace emu {

/**
 * Encapsulates a striped array ( @c mw_malloc1dlong).
 * @tparam T Element type. Must be a 64-bit type (generally @c long or a pointer type).
 */
template<typename T>
class striped_array
{
    static_assert(sizeof(T) == 8, "emu_striped_array can only hold 64-bit data types");
    using self_type = striped_array;

private:
    repl<T*> ptr_;
    repl<long> n_;

    T* allocate_storage(long size)
    {
        auto ptr = reinterpret_cast<T*>(
            mw_malloc1dlong(static_cast<size_t>(size)));
        if (!ptr) { EMU_OUT_OF_MEMORY(size * sizeof(long)); }
        return ptr;
    }

    void free_storage()
    {
        if (ptr_) {
            mw_free(ptr_);
            ptr_ = nullptr;
        }
    }

public:
    using value_type = T;
    using iterator = T*;
    using const_iterator = T const*;

    iterator begin ()               { return ptr_; }
    iterator end ()                 { return ptr_ + n_; }
    const_iterator begin () const   { return ptr_; }
    const_iterator end () const     { return ptr_ + n_; }
    const_iterator cbegin () const  { return ptr_; }
    const_iterator cend () const    { return ptr_ + n_; }

    T* data() { return ptr_; }
    const T* data() const { return ptr_; }

    T& front()              { return ptr_[0]; }
    const T& front() const  { return ptr_[0]; }
    T& back()               { return ptr_[n_ - 1]; }
    const T& back() const   { return ptr_[n_ - 1]; }

    // Default constructor
    striped_array()
        : ptr_(nullptr)
        , n_(0)
    {}

    /**
     * Constructs a emu_striped_array<T>
     * @param n Number of elements
     */
    explicit striped_array(long n)
        : ptr_(allocate_storage(n))
        , n_(n)
    {}

    // Shallow copy constructor (used for repl<T>)
    striped_array(const self_type& other, shallow_copy)
        : ptr_(other.ptr_)
        , n_(other.n_)
    {}

    // Destructor
    ~striped_array()
    {
        free_storage();
    }

    // Swap overload
    friend void
    swap(self_type& first, self_type& second)
    {
        repl_swap(first.n_, second.n_);
        repl_swap(first.ptr_, second.ptr_);
    }

    // Copy constructor
    striped_array(const self_type & other)
        : ptr_(allocate_storage(other.n_))
        , n_(other.n_)
    {
        // TODO upgrade to emu::parallel::copy
        memcpy(ptr_, other.ptr_, (size_t)(n_ * sizeof(T)));
    }

    // Assignment operator (using copy-and-swap idiom)
    self_type& operator= (self_type other)
    {
        swap(*this, other);
        return *this;
    }

    // Move constructor (using copy-and-swap idiom)
    striped_array(self_type&& other) noexcept : striped_array()
    {
        swap(*this, other);
    }

    T&
    operator[] (long i)
    {
        return ptr_[i];
    }

    const T&
    operator[] (long i) const
    {
        return ptr_[i];
    }

    long size() const { return n_; }

    void resize(long new_size)
    {
        // Do we need to reallocate?
        if (new_size > n_) {
            // Allocate new array
            auto new_ptr = allocate_storage(new_size);
            if (ptr_) {
                // Copy elements over into new array
                // TODO upgrade to emu::parallel::copy
                memcpy(new_ptr, ptr_, (size_t)(n_ * sizeof(T)));
                // Deallocate old array
                free_storage();
            }
            // Save new pointer
            ptr_ = new_ptr;
        }
        // Update size
        n_ = new_size;
    }

    void clear()
    {
        free_storage();
        n_ = 0;
    }
};

} // end namespace emu