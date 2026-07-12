#include <apexPthreadUtils.h> // Declares PthreadPrioInheritMutex

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <type_traits>

TEST(PthreadPrioInheritMutex, TryLockSucceedsWhenUnlocked) {
    PthreadPrioInheritMutex mutex;
    std::unique_lock<PthreadPrioInheritMutex> lock(mutex, std::try_to_lock);
    EXPECT_TRUE(lock.owns_lock());
}

TEST(PthreadPrioInheritMutex, TryLockFailsWhenAlreadyLocked) {
    PthreadPrioInheritMutex mutex;
    std::unique_lock<PthreadPrioInheritMutex> first(mutex, std::try_to_lock);
    ASSERT_TRUE(first.owns_lock());

    std::unique_lock<PthreadPrioInheritMutex> second(mutex, std::try_to_lock);
    EXPECT_FALSE(second.owns_lock());
}

TEST(PthreadPrioInheritMutex, TryLockSucceedsAgainAfterUnlock) {
    PthreadPrioInheritMutex mutex;
    {
        std::unique_lock<PthreadPrioInheritMutex> first(mutex, std::try_to_lock);
        ASSERT_TRUE(first.owns_lock());
    } // first's destructor releases the mutex here

    std::unique_lock<PthreadPrioInheritMutex> second(mutex, std::try_to_lock);
    EXPECT_TRUE(second.owns_lock());
}

TEST(PthreadPrioInheritMutex, BlockingLockWaitsForRelease) {
    PthreadPrioInheritMutex mutex;
    std::unique_lock<PthreadPrioInheritMutex> holder(mutex, std::try_to_lock);
    ASSERT_TRUE(holder.owns_lock());

    std::atomic<bool> acquired{false};
    std::thread waiter([&mutex, &acquired] {
        std::unique_lock<PthreadPrioInheritMutex> lock(mutex); // blocking lock(), not try_to_lock
        acquired = true;
    });

    // Give the waiter a chance to actually block on the still-held mutex.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(acquired.load());

    holder.unlock();
    waiter.join();
    EXPECT_TRUE(acquired.load());
}

TEST(PthreadPrioInheritMutex, IsNeitherCopyableNorMovable) {
    EXPECT_FALSE(std::is_copy_constructible_v<PthreadPrioInheritMutex>);
    EXPECT_FALSE(std::is_move_constructible_v<PthreadPrioInheritMutex>);
}
