#pragma once

#include <utility>
#include <functional>
#include <memory>
#include <cstring>
#include <cassert>
#include <cilk/cilk.h>
#include <emu_c_utils/emu_c_utils.h>
#include "pointer_manipulation.h"
#include "execution_policy.h"
#include "out_of_memory.h"

/*
 * This header provides support for storing C++ objects in replicated memory.
 * There are several template wrappers to choose from depending on what kind of behavior you want
 * - repl_new     : Classes that inherit from repl_new will be allocated in replicated memory.
 *
 * - repl_shallow<T> : Call constructor locally, then shallow copy onto each remote nodelet.
 *                  The destructor will be called on only one copy, the other shallow copies ARE NOT DESTRUCTED.
 *                  The semantics of "shallow copy" depend on the type of T
 *                      - If T is trivially copyable (no nested objects or custom copy constructor),
 *                        remote copies will be initialized using memcpy()
 *                      - If T defines a "shallow copy constructor", this will be used to construct shallow copies
 *                        The "shallow copy constructor" has the following signature:
 *                            T(const T& other, emu::shallow_copy)
 *                        The second argument can be ignored.
 *
 * - repl<T>      : Implements repl_shallow-like functionality for primitive types (int, float, long*, etc.)
 *
 * - repl_deep<T> : Call constructor on every nodelet with same arguments.
 *                  Every copy will be destructed individually.
 */

namespace emu {
namespace detail {

template<typename T, typename Function>
void repl_for_each(
    sequenced_policy,
    long nlet_begin, long nlet_end,
    T &repl_ref, Function worker
) {
    for (long nlet = nlet_begin; nlet < nlet_end; ++nlet) {
        T &remote_ref = *emu::pmanip::get_nth(&repl_ref, nlet);
        worker(remote_ref);
    }
}

template<long Grain, class T, class Function>
void repl_for_each(
    parallel_policy<Grain> policy,
    long nlet_begin, long nlet_end,
    T &repl_ref, Function worker
) {
    constexpr long grain = Grain;
    // Recursive spawn
    for(;;) {
        // How many nodelets do we need to spawn on?
        auto nlet_count = nlet_end - nlet_begin;
        if (nlet_count <= grain) { break; }
        // Divide the nodelets in half
        long nlet_mid = nlet_begin + nlet_count / 2;
        // Spawn a thread to handle the upper half
        cilk_migrate_hint(emu::pmanip::get_nth(&repl_ref, nlet_mid));
        cilk_spawn repl_for_each(
            policy, nlet_mid, nlet_end, repl_ref, worker);
        // Recurse over the lower half
        nlet_end = nlet_mid;
    }
    // Serial execution
    repl_for_each(seq, nlet_begin, nlet_end, repl_ref, worker);
}

} // end namespace detail

template<class Policy, class T, class Function,
    // Disable if first argument is not an execution policy
    std::enable_if_t<is_execution_policy_v<Policy>, int> = 0
>
void repl_for_each(Policy policy, T & repl_ref, Function worker)
{
    assert(emu::pmanip::is_repl(&repl_ref));
    detail::repl_for_each(policy, 0, NODELETS(), repl_ref, worker);
}

template<class T, class Function>
void repl_for_each(T & repl_ref, Function worker)
{
    assert(emu::pmanip::is_repl(&repl_ref));
    detail::repl_for_each(seq, 0, NODELETS(), repl_ref, worker);
}

/**
 * Overrides default new to always allocate replicated storage for instances of this class.
 * repl_new is intended to be used as a parent class for distributed data structure types.
 */
class repl_new
{
public:
    // Overrides default new to always allocate replicated storage for instances of this class
    static void *
    operator new(std::size_t sz)
    {
        void * ptr = mw_mallocrepl(sz);
        if (!ptr) { EMU_OUT_OF_MEMORY(sz * NODELETS()); }
        return ptr;
    }

    // Overrides default delete to safely free replicated storage
    static void
    operator delete(void * ptr)
    {
        mw_free(ptr);
    }
};

/**
 * Tag type used to define an overload of the copy constructor that performs
 * shallow copies.
 */
struct shallow_copy {};

/**
 * repl_shallow<T> : Wrapper template to add replicated semantics to a class using shallow copies.
 *
 * Assignment:
 *     Will call the assignment operator on each remote copy.
 *
 * Construction:
 *     Will call T's constructor locally, then shallow copy T onto each remote nodelet.
 *     The semantics of "shallow copy" depend on the type of T.
 *
 *    - If T is trivially copyable (no nested objects or custom copy constructor),
 *    remote copies will be initialized using memcpy().
 *
 *    - If T defines a "shallow copy constructor", this will be used to construct shallow copies.
 *    The "shallow copy constructor" has the following signature:
 *     @code T(const T& other, emu::shallow_copy) @endcode
 *    The second argument can be ignored.
 *
 * Destruction:
 *     The destructor will be called on only one copy, the other shallow copies ARE NOT DESTRUCTED.
 *
 * All other operations (function calls, attribute accesses) will access the local copy of T.
 */
template <typename T>
class repl_shallow : public T, public repl_new
{
public:
    /**
     * Default shallow copy operation to be used by repl_shallow<T>
     * Only defined for trivially copyable (i.e. copy constructor == memcpy) types.
     * In this case there is no difference between a deep copy and a shallow copy.
     * @tparam T Type with trivial copy semantics
     * @param dst Pointer to sizeof(T) bytes of uninitialized memory
     * @param src Pointer to constructed T
     */
    template<typename U=T>
    void
    do_shallow_copy(
        typename std::enable_if<std::is_trivially_copyable<U>::value, U>::type * dst,
        const U * src)
    {
        memcpy(dst, src, sizeof(U));
    }

    /**
     * If T has a "shallow copy constructor" (copy constructor with additional dummy argument)
     * repl_shallow<T> will use this version instead. Otherwise SFINAE will make this one go away.
     * @tparam T Type with a "shallow copy constructor" (copy constructor with additional dummy argument)
     * @param dst Pointer to sizeof(T) bytes of uninitialized memory
     * @param src Pointer to constructed T
     */
    template<typename U=T>
    void
    do_shallow_copy(
        U * dst,
        const U * src)
    {
        new (dst) U(*src, shallow_copy());
    }

    T* get()       { return this; }

    /**
     * Returns a reference to the copy of T on the Nth nodelet
     * @param n nodelet ID
     * @return Reference the copy of T on the Nth nodelet
     */
    T& get_nth(long n)
    {
        assert(n < NODELETS());
        return *emu::pmanip::get_nth(this, n);
    }

    const T& get_nth(long n) const
    {
        assert(n < NODELETS());
        return *emu::pmanip::get_nth(this, n);
    }

    // Wrapper constructor to copy T to each nodelet after running the requested constructor
    template<typename... Args>
    explicit repl_shallow (Args&&... args)
    // Call T's constructor with forwarded args
    : T(std::forward<Args>(args)...)
    {
        // Get pointer to constructed T
        T* local = &get_nth(NODE_ID());
        // Replicate to each remote nodelet
        for (long i = 0; i < NODELETS(); ++i) {
            T * remote = &get_nth(i);
            if (local == remote) { continue; }
            // Shallow copy copy local to remote
            do_shallow_copy(remote, local);
        }
    }

    // Copy constructor
    repl_shallow(const repl_shallow& other)
    : T(other, shallow_copy())
    {
        for (long i = 0; i < NODELETS(); ++i) {
            do_shallow_copy(&get_nth(i), &other.get_nth(i));
        }
    }

    repl_shallow(const repl_shallow& other, shallow_copy) = delete;

    friend void swap(repl_shallow& lhs, repl_shallow& rhs)
    {
        using std::swap;
        for (long i = 0; i < NODELETS(); ++i) {
            swap(lhs.get_nth(i), rhs.get_nth(i));
        }
    }

    // Initializes all copies to the same value
    repl_shallow&
    operator=(const T& rhs)
    {
        for (long i = 0; i < NODELETS(); ++i) {
            T * remote = &get_nth(i);
            // Assign value to remote copy
            *remote = rhs;
        }
        return *this;
    }
};

/**
 * Replicated wrapper for primitive types
 * Same behavior as repl_shallow<T>, but simpler since there are no custom constructors or deep copies.
 */
template<typename T>
class repl : public repl_new
{
private:
    T val;
public:
    // Returns a reference to the copy of T on the Nth nodelet
    T& get_nth(long n)
    {
        assert(n < NODELETS());
        return *pmanip::get_nth(&val, n);
    }

    // Default constructor
    repl<T>() = default;

    repl(const repl& other)
    {
        operator=(other.val);
    }

    // Wrapper constructor to initialize T on each nodelet
    repl<T>(T x)
    {
        operator=(x);
    }

    T* operator&() { return &val; }

    // Make it easy to convert back to T
    operator T& ()
    {
        return val;
    }

    // Make it easy to convert back to T
    operator const T& () const
    {
        return val;
    }

    T& get() { return val; }

    // Initializes all copies to the same value
    repl&
    operator=(const T& rhs)
    {
        assert(emu::pmanip::is_repl(this));
        for (long i = 0; i < NODELETS(); ++i) {
            get_nth(i) = rhs;
        }
        return *this;
    }

    // If T is a pointer type, allow users to dereference it
    template<typename U = T>
    typename std::enable_if<std::is_pointer<U>::value, U>::type
    operator->() { return val; }
};

/**
 * repl_deep<T> : Wrapper template to add replicated semantics to a class using distributed construction.
 *
 * Assignment:
 *     Will call the assignment operator on the local copy.
 *
 * Construction:
 *     Calls constructor on every nodelet's copy with the same arguments.
 *
 * Destruction:
 *     The destructor will be called on each copy individually.
 *
 * All other operations (function calls, attribute accesses) will access the local copy of T.
 */
template<typename T>
class repl_deep : public T, public repl_new
{
public:
    /**
     * Returns a reference to the copy of T on the Nth nodelet
     * @param n nodelet ID
     * @return Returns a reference to the copy of T on the Nth nodelet
     */
    T& get_nth(long n)
    {
        assert(n < NODELETS());
        return *emu::pmanip::get_nth(this, n);
    }

    // Constructor template - allows repl_deep<T> to be constructed just like a T
    template<typename... Args>
    explicit repl_deep(Args&&... args)
    // Call T's constructor with forwarded args
    : T(std::forward<Args>(args)...)
    {
#ifdef __le64__
        assert(emu::pmanip::get_view(this) == 0);
#endif
        // Pointer to the object on this nodelet, which has already been constructed
        T * local = &get_nth(NODE_ID());
        // For each nodelet...
        for (long n = 0; n < NODELETS(); ++n) {
            // Use placement-new to construct each remote object with forwarded arguments
            T * remote = &get_nth(n);
            if (local == remote) continue;
            new (remote) T(std::forward<Args>(args)...);
        }
    }

    ~repl_deep()
    {
        // Pointer to the object on this nodelet, which has already been destructed
        T * local = &get_nth(NODE_ID());
        // For each nodelet...
        for (long n = 0; n < NODELETS(); ++n) {
            // Explicitly call destructor to tear down each remote object
            T * remote = &get_nth(n);
            if (local == remote) continue;
            remote->~T();
        }
    }
};

// TODO implement repl_iterator, then this can be merged with regular reduce()
// TODO missing impl for repl_shallow and repl_deep
template<typename T, typename F>
T repl_reduce(repl<T>& ref, F reduce)
{
    T value = ref.get_nth(0);
    for (long nlet = 1; nlet < NODELETS(); ++nlet) {
        value = reduce(value, ref.get_nth(nlet));
    }
    return value;
}

template<typename T, typename F>
T repl_reduce(T& ref, F reduce)
{
    assert(emu::pmanip::is_repl(&ref));
    T value = emu::pmanip::get_nth(&ref, 0);
    for (long nlet = 1; nlet < NODELETS(); ++nlet) {
        value = reduce(value, emu::pmanip::get_nth(&ref, nlet));
    }
    return value;
}

template<typename T>
void
repl_swap(T& lhs, T& rhs)
{
    using std::swap;
    if (pmanip::is_repl(&lhs) && pmanip::is_repl(&rhs)) {
        for (long nlet = 0; nlet < NODELETS(); ++nlet) {
            swap(
                *pmanip::get_nth(&lhs, nlet),
                *pmanip::get_nth(&rhs, nlet)
            );
        }
    } else {
        swap(lhs, rhs);
    }
}

/**
 * Returns a smart pointer to a replicated instance of T
 * @param args Arguments to forward to T's constructor
 * @return a smart pointer to a replicated instance of T
 */
template<typename T, typename ...Args>
std::unique_ptr<emu::repl<T>> make_repl( Args&& ...args )
{
    return std::make_unique<emu::repl<T>>( std::forward<Args>(args)... );
}

/**
 * Returns a smart pointer to a replicated (shallow copies) instance of T
 * @param args Arguments to forward to T's constructor
 * @return a smart pointer to a replicated (shallow copies) instance of T
 */
template<typename T, typename ...Args>
std::unique_ptr<emu::repl_shallow<T>> make_repl_shallow( Args&& ...args )
{
    return std::make_unique<emu::repl_shallow<T>>( std::forward<Args>(args)... );
}

/**
 * Returns a smart pointer to a replicated (deep copies) instance of T
 * @param args Arguments to forward to T's constructor
 * @return a smart pointer to a replicated (deep copies) instance of T
 */
template<typename T, typename ...Args>
std::unique_ptr<emu::repl_deep<T>> make_repl_deep( Args&& ...args )
{
    return std::make_unique<emu::repl_deep<T>>( std::forward<Args>(args)... );
}

} // end namespace emu