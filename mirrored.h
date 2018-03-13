#pragma once

#include <utility>
#include <cstring>

// Wrapper class to replicate an object onto all nodelets after it has been constructed
template <typename T>
struct mirrored : public T
{
    // Wrapper constructor to copy T to each nodelet after running the requested constructor
    template<typename... Args>
    explicit mirrored (Args&&... args)
    // Call T's constructor with forwarded args
    : T(std::forward<Args>(args)...)
    {
        // Get pointer to constructed T
        T* local = static_cast<T*>(mw_get_nth(this, NODE_ID()));
        // Replicate to each remote nodelet
        for (long i = 0; i < NODELETS(); ++i) {
            T * remote = static_cast<T*>(mw_get_nth(this, i));
            if (local == remote) { continue; }
            // Copy local to remote
            memcpy(remote, local, sizeof(T));
        }
    }

    // Overrides default new to always allocate replicated storage for instances of this class
    static void *
    operator new(std::size_t sz)
    {
        return mw_mallocrepl(sz);
    }

    // Overrides default delete to safely free replicated storage
    static void
    operator delete(void * ptr)
    {
        mw_free(ptr);
    }

    // Initializes all copies to the same value
    void
    operator=(const T& rhs)
    {
        for (long i = 0; i < NODELETS(); ++i) {
            T * remote = static_cast<T*>(mw_get_nth(this, i));
            // Copy local to remote
            memcpy(remote, rhs, sizeof(T));
        }
    }

    T& get_nth(long n)
    {
        assert(n < NODELETS());
        return *static_cast<T*>(mw_get_nth(this, n));
    }

};

template <>
struct mirrored<int64_t>
{
    int64_t val;
    // Wrapper constructor to copy T to each nodelet after running the requested constructor
    // Call T's constructor with forwarded args
    mirrored<int64_t>(int64_t x)
    {
        mw_replicated_init(&val, x);
    }

    operator int64_t const () {
        return val;
    }

    // Initializes all copies to the same value
    void
    operator=(int64_t rhs)
    {
        mw_replicated_init(&val, rhs);
    }
};
