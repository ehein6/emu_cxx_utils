#pragma once
#include <mutex>

/**
 * What we need: An efficient thread-safe local allocator
 *
 * Why can't you just use...
 * malloc(): limited to the first 1GB on each nodelet, not enough
 * mw_localmalloc(): steals memory from every nodelet.
 *
 *
 * Concept:
 * Create a free list on each nodelet. Use mw_mallocstripe to populate the list.
 *
 * On alloc:
 * 1. Acquire a lock on the target nodelet
 * 2. Try to satisfy the allocation from the local free list
 * 3. Not enough room? Acquire a global lock
 * 4. Check the local free list again
 * 5. Call mw_mallocstripe to get some more memory everywhere
 * 6. Add memory to every local free list. Need to acquire each local lock once.
 * 6. Satisfy the original allocation from the local free list
 * 7. Unlock global lock
 * 8. Unlock local lock
 *
 *
 *
 * We still have a deadlock condition here:
 * Node A runs out of room, grabs global lock and tries to get more space
 * Node B also runs out of room, many threads are spinning on the local lock
 * Thread from node A migrates to Node B, attempting to refill the free list
 * But it can't because Node A is full of spinning threads
 * I think this goes away on the ring
 *
 *
 *
 *
 *
 *
*/
#include "cas_mutex.h"
#include <experimental/memory_resource>

namespace emu {

class localmalloc_resource : public std::experimental::pmr::memory_resource
{
    virtual void*
    do_allocate(std::size_t bytes, std::size_t alignment) final
    {
        // TODO handle alignment
        return mw_localmalloc(bytes, this);
    }

    virtual void
    do_deallocate(void * p, std::size_t bytes, std::size_t alignment) final
    {
        mw_localfree(p);
    }

    virtual bool
    do_is_equal(const std::experimental::pmr::memory_resource&) const noexcept final
    {
        return true;
    }
};

//class atomic_monotonic_buffer_resource : public std::experimental::pmr::memory_resource
//{
//private:
//    std::byte* pool_;
//
//    virtual void*
//    do_allocate(std::size_t bytes, std::size_t alignment) final
//    {
//        // TODO handle alignment
//        return emu::atomic_addms(&pool_, bytes);
//    }
//
//    virtual void
//    do_deallocate(void * p, std::size_t bytes, std::size_t alignment) final
//    {
//        // Do nothing
//    }
//
//    virtual bool
//    do_is_equal(const std::experimental::pmr::memory_resource&) const noexcept final
//    {
//        return true;
//    }
//};



//
//class nlet_local_freelist // TODO inherit from std::pmr::memory_resource
//{
//private:
//
//    struct block {
//        block* next;
//        block* prev;
//        size_t size_;
//        std::byte * data_;
//    };
//
//    cas_mutex lock_;
//    block * head_;
//
//    // Link block b into the free list between prev and next
//    void add(block * b, block * prev, block * next)
//    {
//        next->prev = b;
//        b->next = next;
//        b->prev = prev;
//        prev->next = b;
//    }
//
//    // Unlink block b from the free list
//    void remove(block * b)
//    {
//        b->prev->next = b->next;
//        b->next->prev = b->prev;
//        b->prev = nullptr;
//        b->next = nullptr;
//    }
//
//    // Add sz bytes to all copies of the resource
//    void grow_all(size_t sz) {
//        std::byte * repl_data = static_cast<std::byte*>(mw_mallocrepl(sz));
//        // FIXME handle out of memory
//
//        for (long nlet = 0; nlet < NODELETS(); ++nlet) {
//            auto data = emu::pmanip::get_nth(repl_data, nlet);
//            auto self = emu::pmanip::get_nth(this, nlet);
//            self->add_block(data, sz);
//        }
//    }
//
//public:
//
//    // Add more memory to this free list
//    void add_block(std::byte * data, size_t sz) {
//        block * b = reinterpret_cast<block*>(data); // TODO handle alignment
//        b->size_ = sz - sizeof(block);
//        b->data_ = data + sizeof(block);
//
//        // FIXME
//    }
//
//
//    // Allocate sz bytes
//    void *
//    allocate(size_t bytes)
//    {
//        std::byte * ptr = nullptr;
//        block * b = nullptr;
//        if (bytes == 0) { return nullptr; }
//
//        // TODO try to shrink the critical section
//        // Everything should work fine if two threads select different blocks
//        lock_.lock();
//
//        // Walk the list, looking for a big enough block
//        for (b = head_; b; b = b->next) {
//            if (b->size_ >= bytes) {
//                ptr = b->data_;
//                break;
//            }
//        }
//
//        if (!ptr) {
//            // Get more memory from the
//            grow_all(bytes);
//        }
//
//        remove(b);
//
//
//
//
//        lock_.unlock();
//        return ptr;
//    }
//
//    // Free the allocated memory
//    void deallocate(void * ptr, size_t bytes)
//    {
//        if (!ptr) { return; }
//
//        block * freed_b = reinterpret_cast<block*>(
//            static_cast<std::byte*>(ptr) - offsetof(block, data_)
//        );
//
//        { // critical section
//            std::lock_guard(this->lock_);
//
//            block *b;
//            for (b = head_; b; b = b->next) {
//                if (b > freed_b) {
//                    add(freed_b, b->prev, b);
//                    break;
//                }
//            }
//            add(freed_b,)
//        }
//
//    }
//
//
//
//};


} // end namespace emu








