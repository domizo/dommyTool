#pragma once

#include <cmath>

namespace dommy {
namespace bullet {

enum class TimelineAction { Initialize, Reuse, Advance, RestartRequired };

inline TimelineAction timelineAction(bool ready, bool atStart,
                                     double deltaFrames, bool playback)
{
    if (atStart) return TimelineAction::Initialize;
    if (!ready || !std::isfinite(deltaFrames)) return TimelineAction::RestartRequired;
    if (deltaFrames == 0) return TimelineAction::Reuse;
    if (deltaFrames > 0 && (deltaFrames <= 1 || (playback && deltaFrames < 100)))
        return TimelineAction::Advance;
    return TimelineAction::RestartRequired;
}

// Shared by the Maya solver adapters. Native changes cannot be rolled
// back after a host failure: prevent retries from advancing a partial world.
class BulletWorldMutationGuard final {
public:
    explicit BulletWorldMutationGuard(bool& requiresStartFrame) noexcept
        : requiresStartFrame_(requiresStartFrame) {}
    BulletWorldMutationGuard(bool& requiresStartFrame, bool& reported) noexcept
        : requiresStartFrame_(requiresStartFrame), reported_(&reported) {}
    ~BulletWorldMutationGuard() noexcept {
        if (armed_) {
            requiresStartFrame_ = true;
            if (reported_) *reported_ = false;
        }
    }
    void arm() noexcept { armed_ = true; }
    void commit() noexcept { armed_ = false; }
    BulletWorldMutationGuard(const BulletWorldMutationGuard&) = delete;
    BulletWorldMutationGuard& operator=(const BulletWorldMutationGuard&) = delete;
private:
    bool& requiresStartFrame_;
    bool* reported_ = nullptr;
    bool armed_ = false;
};

} // namespace bullet
} // namespace dommy
