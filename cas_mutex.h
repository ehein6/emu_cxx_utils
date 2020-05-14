#include "intrinsics.h"

namespace emu {

/**
 * Simple spinlock
 *
 * WARNING: This lock is only deadlock free if the following conditions hold
 * 1. A thread must not give up its execution slot (via migration or
 *    system call) while holding the lock.
 * 2. The number of threads contending on a single lock must not exceed the
 *     number of execution contexts per nodelet (currently 64). Otherwise
 *     the threads spinning on the lock may block other threads from migrating
 *     in.
 * More study is required to write a safer lock, but this works in simple cases.
 */
class cas_mutex
{
private:
    volatile long lock_;
public:
    cas_mutex() : lock_(0) {}
    void lock() {
        do {
            // Spin without doing CAS
            while (lock_ != 0) { RESCHEDULE(); }
            // Try to set to 1, retry if old value was not zero
        } while (0 != emu::atomic_cas(&lock_, 0L, 1L));
    }
    void unlock() {
        lock_ = 0;
    }
};

} // end namespace emu