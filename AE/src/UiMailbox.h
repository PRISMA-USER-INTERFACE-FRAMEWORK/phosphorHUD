#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace Phosphor {
// Only copied data crosses this boundary. Never hold this mutex while calling the engine.
// One callback is queued/running; it coalesces newer requests and rejects results from old sessions.
template <class Request, class Result>
class UiMailbox {
public:
    struct Work { std::uint64_t epoch, revision; Request request; };
    bool Submit(Request request) {
        std::scoped_lock lock(mutex_);
        request_ = std::move(request);
        ++revision_;
        if (queued_) return false;
        queued_ = true;
        return true;
    }
    std::optional<Work> Begin() {
        std::scoped_lock lock(mutex_);
        if (!request_) { queued_ = false; return std::nullopt; }
        return Work{epoch_, revision_, *request_};
    }
    bool Complete(const Work& work, std::optional<Result> result) {
        std::scoped_lock lock(mutex_);
        if (work.epoch == epoch_ && result) result_ = std::move(result);
        // A request made while capture ran must not be lost (especially the restore request).
        if (request_ && work.revision != revision_) return true;
        queued_ = false;
        return false;
    }
    void Reset() {
        std::scoped_lock lock(mutex_);
        ++epoch_;
        ++revision_;
        request_.reset();
        result_.reset();
        // Keep the reservation until the already queued callback drains.
    }
    void CancelQueue() {
        std::scoped_lock lock(mutex_);
        queued_ = false;
    }
    std::optional<Result> Latest() const {
        std::scoped_lock lock(mutex_);
        return result_;
    }
private:
    mutable std::mutex mutex_;
    std::uint64_t epoch_ = 0, revision_ = 0;
    bool queued_ = false;
    std::optional<Request> request_;
    std::optional<Result> result_;
};
}
