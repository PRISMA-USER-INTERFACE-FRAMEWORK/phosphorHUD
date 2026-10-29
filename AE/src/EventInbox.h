#pragma once
#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

namespace Phosphor {
// Event callbacks may run off-thread. Retain bounded payloads until the game thread takes them.
template <class T> class EventInbox {
public:
    struct Batch { std::vector<T> events; std::size_t dropped = 0; };
    explicit EventInbox(std::size_t capacity) : capacity_(capacity) {}
    void Push(const T& event) {
        std::scoped_lock guard{lock_};
        if (!enabled_) return;
        if (batch_.events.size() >= capacity_) { ++batch_.dropped; return; }
        batch_.events.push_back(event);
    }
    Batch Take() {
        std::scoped_lock guard{lock_};
        return std::exchange(batch_, {});
    }
    void SetEnabled(bool enabled) {
        std::scoped_lock guard{lock_};
        enabled_ = enabled;
        if (!enabled_) batch_ = {};
    }
private:
    std::mutex lock_;
    Batch batch_;
    std::size_t capacity_;
    bool enabled_ = false;
};
}
