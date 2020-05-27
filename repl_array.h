#pragma once

#include <emu_c_utils/emu_c_utils.h>
#include "out_of_memory.h"
#include "replicated.h"

namespace emu {

template<class T>
class repl_array {
    using self_type = repl_array;

private:
    repl<T *> data_;
    repl<long> size_;

    T* allocate(long size)
    {
        T* ptr = reinterpret_cast<T *>(mw_mallocrepl(sizeof(T) * size));
        if (!ptr) { EMU_OUT_OF_MEMORY(sizeof(T) * size * NODELETS()); }
        return ptr;
    }

public:
    // Default constructor
    repl_array()
        : data_(nullptr)
        , size_(0)
    {}

    // Constructor : create a repl_array with `size` elements on each nodelet
    explicit repl_array(long size)
        : data_(allocate(size))
        , size_(size)
    {}

    // Shallow copy constructor
    repl_array(const repl_array &other, emu::shallow_copy)
        : data_(other.data_), size_(other.size_)
    {}

    // Destructor
    ~repl_array() {
        if (data_) { mw_free(data_); }
    }

    // Copy constructor
    repl_array(const repl_array &other)
        : data_(allocate(other.size_))
        , size_(other.size_)
    {
        // TODO upgrade to emu::parallel::copy
        for(long nlet = 0; nlet < NODELETS(); ++nlet) {
            memcpy(
                data_.get_nth(nlet),
                other.data_.get_nth(nlet),
                size_ * sizeof(T)
            );
        }
    }

    // Swop overload
    friend void swap(self_type& lhs, self_type& rhs)
    {
        repl_swap(lhs.data_, rhs.data_);
        repl_swap(lhs.size_, rhs.size_);
    }

    // Assignment operator - using copy-and-swap idiom
    self_type& operator=(self_type other)
    {
        swap(*this, other);
        return *this;
    }

    // Move constructor (using copy-and-swap idiom)
    repl_array(repl_array&& other) noexcept : repl_array()
    {
        swap(*this, other);
    }

    T *get_nth(long n) {
        return data_.get_nth(n);
    }

    const T *get_nth(long n) const {
        return data_.get_nth(n);
    }

    T * get_localto(void const* other) {
        return data_.get_localto(other);
    }

    const T * get_localto(void const * other) const {
        return data_.get_localto(other);
    }

    const T* data() const { return data_; }
    T* data() { return data_; }
    long size() const { return size_; }

    void resize(long new_size)
    {
        if (new_size > size_) {
            // Allocate new array
            auto new_ptr = allocate(new_size);
            if (data_) {
                // TODO upgrade to emu::parallel::copy
                for(long nlet = 0; nlet < NODELETS(); ++nlet) {
                    memcpy(
                        emu::pmanip::get_nth(new_ptr, nlet),
                        data_.get_nth(nlet),
                        size_ * sizeof(T)
                    );
                }
                // Deallocate old array
                mw_free(data_);
            }
            // Save new pointer
            data_ = new_ptr;
        }
        // Update the size
        size_ = new_size;
    }
};

} // end namespace emu