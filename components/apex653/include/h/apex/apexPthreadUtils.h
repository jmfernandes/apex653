/// @file apexPthreadUtils.h
/// @brief Internal C++ concurrency helpers. Not part of the public ABI, so unlike the
///        apex*.h service headers, this is free to use full C++ (no extern "C", no C
///        compatibility requirement).

#ifndef POSIX_APEX_PTHREAD_H
#define POSIX_APEX_PTHREAD_H

#include <pthread.h>

/// @brief A pthread_mutex_t configured with the PRIO_INHERIT protocol (to avoid priority
///        inversion), exposing the standard Lockable/TryLockable interface so it can be used
///        with std::unique_lock instead of hand-rolled lock/unlock bookkeeping.
class PthreadPrioInheritMutex
{
public:
    PthreadPrioInheritMutex()
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
        pthread_mutex_init(&mutex_, &attr);
        pthread_mutexattr_destroy(&attr);
    }

    ~PthreadPrioInheritMutex()
    {
        pthread_mutex_destroy(&mutex_);
    }

    PthreadPrioInheritMutex(const PthreadPrioInheritMutex&) = delete;
    PthreadPrioInheritMutex& operator=(const PthreadPrioInheritMutex&) = delete;
    PthreadPrioInheritMutex(PthreadPrioInheritMutex&&) = delete;
    PthreadPrioInheritMutex& operator=(PthreadPrioInheritMutex&&) = delete;

    void lock() { pthread_mutex_lock(&mutex_); }
    bool try_lock() { return pthread_mutex_trylock(&mutex_) == 0; }
    void unlock() { pthread_mutex_unlock(&mutex_); }

private:
    pthread_mutex_t mutex_{};
};

#endif /* POSIX_APEX_PTHREAD_H */
