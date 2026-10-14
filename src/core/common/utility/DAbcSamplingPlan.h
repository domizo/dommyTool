#pragma once

#include "DCheckedSize.h"
#include "DFrameSampling.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <iterator>
#include <set>
#include <vector>

namespace dommy {
namespace sampling {

enum AbcTimeSamplingMode {
    kAbcDefaultTimeSampling,
    kAbcUniformTimeSampling,
    kAbcCyclicTimeSampling,
    kAbcAcyclicTimeSampling
};

struct AbcFrameRange {
    AbcFrameRange()
        : startTime(0.0)
        , endTime(0.0)
        , strideTime(1.0)
        , preRoll(false)
    {
    }

    double startTime;
    double endTime;
    double strideTime;
    std::set<double> shutterSamples;
    bool preRoll;
};

struct AbcTimeSamplingPlan {
    AbcTimeSamplingPlan()
        : mode(kAbcDefaultTimeSampling)
        , period(0.0)
    {
    }

    AbcTimeSamplingMode mode;
    std::vector<double> sampleTimes;
    double period;
};

struct AbcSamplingPlan {
    AbcSamplingPlan()
        : hasExplicitTimeSampling(false)
        , setupFrame(0.0)
        , sampleEventCount(0U)
    {
    }

    bool hasExplicitTimeSampling;
    double setupFrame;
    std::size_t sampleEventCount;
    std::set<double> transformFrames;
    std::set<double> shapeFrames;
    std::set<double> preRollFrames;
    AbcTimeSamplingPlan transformTime;
    AbcTimeSamplingPlan shapeTime;
    AbcTimeSamplingPlan boundsTime;
};

namespace detail {

struct AbcPeriodicPattern {
    AbcPeriodicPattern()
        : enabled(false)
        , period(0.0)
    {
    }

    bool enabled;
    double period;
    std::vector<double> firstCycleTimes;
};

inline bool nearlyEqualFrame(double left, double right)
{
    const double scale =
        std::max(1.0, std::max(std::fabs(left), std::fabs(right)));
    return std::fabs(left - right) <=
        16.0 * std::numeric_limits<double>::epsilon() * scale;
}

inline bool checkedInsertFrame(std::set<double>& frames, double frame)
{
    if (!std::isfinite(frame)) {
        return false;
    }
    frames.insert(frame);
    return frames.size() <= kMaxExportFrameSamples;
}

inline bool isValidPattern(const AbcPeriodicPattern& pattern)
{
    if (!pattern.enabled || !std::isfinite(pattern.period) ||
        pattern.period <= 0.0 || pattern.firstCycleTimes.empty()) {
        return false;
    }

    for (std::size_t index = 0U;
         index < pattern.firstCycleTimes.size(); ++index) {
        if (!std::isfinite(pattern.firstCycleTimes[index])) {
            return false;
        }
        if (index > 0U &&
            !(pattern.firstCycleTimes[index - 1U] <
              pattern.firstCycleTimes[index])) {
            return false;
        }
    }

    const long double nextCycleStart =
        static_cast<long double>(pattern.firstCycleTimes.front()) +
        static_cast<long double>(pattern.period);
    return std::isfinite(nextCycleStart) &&
        static_cast<long double>(pattern.firstCycleTimes.back()) <
            nextCycleStart;
}

inline bool matchesPattern(const std::set<double>& frames,
                           const AbcPeriodicPattern& pattern)
{
    if (frames.empty() || !isValidPattern(pattern)) {
        return false;
    }

    const std::size_t samplesPerCycle = pattern.firstCycleTimes.size();
    std::size_t index = 0U;
    for (std::set<double>::const_iterator frame = frames.begin();
         frame != frames.end(); ++frame, ++index) {
        const std::size_t cycle = index / samplesPerCycle;
        const std::size_t sample = index % samplesPerCycle;
        const long double expected =
            static_cast<long double>(pattern.firstCycleTimes[sample]) +
            static_cast<long double>(pattern.period) *
                static_cast<long double>(cycle);
        if (!std::isfinite(expected) ||
            expected <
                -static_cast<long double>(std::numeric_limits<double>::max()) ||
            expected >
                static_cast<long double>(std::numeric_limits<double>::max()) ||
            !nearlyEqualFrame(*frame, static_cast<double>(expected))) {
            return false;
        }
    }
    return true;
}

inline bool buildTimeSamplingPlan(const std::set<double>& frames,
                                  bool useDefaultSampling,
                                  const AbcPeriodicPattern& pattern,
                                  AbcTimeSamplingPlan& output)
{
    if (frames.empty()) {
        return false;
    }

    AbcTimeSamplingPlan candidate;
    if (useDefaultSampling) {
        if (frames.size() != 1U) {
            return false;
        }
        candidate.mode = kAbcDefaultTimeSampling;
        candidate.sampleTimes.push_back(*frames.begin());
    } else if (matchesPattern(frames, pattern)) {
        candidate.mode = pattern.firstCycleTimes.size() == 1U
            ? kAbcUniformTimeSampling
            : kAbcCyclicTimeSampling;
        candidate.sampleTimes = pattern.firstCycleTimes;
        candidate.period = pattern.period;
    } else {
        candidate.mode = kAbcAcyclicTimeSampling;
        candidate.sampleTimes.assign(frames.begin(), frames.end());
    }

    output = candidate;
    return true;
}

inline bool appendRangeSamples(const AbcFrameRange& range,
                               bool sampleGeometryAtSubframes,
                               std::set<double>& transformFrames,
                               std::set<double>& shapeFrames)
{
    std::size_t frameCount = 0U;
    if (!checkedFrameCount(range.startTime, range.endTime, range.strideTime,
                           kMaxExportFrameSamples, frameCount)) {
        return false;
    }

    for (std::size_t frameIndex = 0U;
         frameIndex < frameCount; ++frameIndex) {
        double frame = 0.0;
        if (!frameAt(range.startTime, range.strideTime, frameIndex, frame)) {
            return false;
        }

        for (std::set<double>::const_iterator shutter =
                 range.shutterSamples.begin();
             shutter != range.shutterSamples.end(); ++shutter) {
            const long double sample =
                static_cast<long double>(frame) +
                static_cast<long double>(*shutter);
            if (!std::isfinite(sample) ||
                sample <
                    -static_cast<long double>(
                        std::numeric_limits<double>::max()) ||
                sample >
                    static_cast<long double>(
                        std::numeric_limits<double>::max())) {
                return false;
            }
            const double sampleFrame = static_cast<double>(sample);

            if (sampleGeometryAtSubframes) {
                if (!checkedInsertFrame(shapeFrames, sampleFrame)) {
                    return false;
                }
            } else {
                const double integerFrame = std::round(sampleFrame);
                if (std::fabs(sampleFrame - integerFrame) < 1.0e-4 &&
                    !checkedInsertFrame(shapeFrames, sampleFrame)) {
                    return false;
                }
            }

            if (!checkedInsertFrame(transformFrames, sampleFrame)) {
                return false;
            }
        }
    }
    return true;
}

inline bool appendPreRollSamples(const AbcFrameRange& range,
                                 std::set<double>& preRollFrames)
{
    std::set<double> ignoredShapeFrames;
    return appendRangeSamples(
        range, false, preRollFrames, ignoredShapeFrames);
}

inline bool makeTransformPattern(const AbcFrameRange& range,
                                 AbcPeriodicPattern& pattern)
{
    AbcPeriodicPattern candidate;
    candidate.enabled = true;
    candidate.period = range.strideTime;
    for (std::set<double>::const_iterator shutter =
             range.shutterSamples.begin();
         shutter != range.shutterSamples.end(); ++shutter) {
        const long double sample =
            static_cast<long double>(range.startTime) +
            static_cast<long double>(*shutter);
        if (!std::isfinite(sample) ||
            sample <
                -static_cast<long double>(
                    std::numeric_limits<double>::max()) ||
            sample >
                static_cast<long double>(
                    std::numeric_limits<double>::max())) {
            return false;
        }
        candidate.firstCycleTimes.push_back(static_cast<double>(sample));
    }
    if (!isValidPattern(candidate)) {
        return false;
    }
    pattern = candidate;
    return true;
}

} // namespace detail

inline bool buildAbcSamplingPlan(const std::vector<AbcFrameRange>& inputRanges,
                                 bool hasExplicitFrameRange,
                                 bool sampleGeometryAtSubframes,
                                 AbcSamplingPlan& output)
{
    if (inputRanges.empty()) {
        return false;
    }

    std::vector<AbcFrameRange> ranges(inputRanges);
    std::size_t sampleEventCount = 0U;
    std::vector<std::size_t> sampleRangeIndices;
    std::vector<std::size_t> preRollRangeIndices;

    for (std::size_t rangeIndex = 0U;
         rangeIndex < ranges.size(); ++rangeIndex) {
        AbcFrameRange& range = ranges[rangeIndex];
        if (range.shutterSamples.empty()) {
            range.shutterSamples.insert(0.0);
        }
        if (range.shutterSamples.size() > kMaxExportShutterSamples) {
            return false;
        }
        for (std::set<double>::const_iterator shutter =
                 range.shutterSamples.begin();
             shutter != range.shutterSamples.end(); ++shutter) {
            if (!std::isfinite(*shutter)) {
                return false;
            }
        }

        std::size_t frameCount = 0U;
        std::size_t rangeEventCount = 0U;
        std::size_t nextEventCount = 0U;
        if (!checkedFrameCount(
                range.startTime, range.endTime, range.strideTime,
                kMaxExportFrameSamples, frameCount) ||
            !checkedSampleEventCount(
                frameCount, range.shutterSamples.size(), rangeEventCount) ||
            !memory::checkedAdd(
                sampleEventCount, rangeEventCount, nextEventCount) ||
            nextEventCount > kMaxExportSampleEvents) {
            return false;
        }
        sampleEventCount = nextEventCount;

        if (range.preRoll) {
            preRollRangeIndices.push_back(rangeIndex);
        } else {
            sampleRangeIndices.push_back(rangeIndex);
        }
    }

    AbcSamplingPlan candidate;
    candidate.sampleEventCount = sampleEventCount;
    candidate.hasExplicitTimeSampling =
        hasExplicitFrameRange && !sampleRangeIndices.empty();

    for (std::size_t index = 0U;
         index < sampleRangeIndices.size(); ++index) {
        const AbcFrameRange& range = ranges[sampleRangeIndices[index]];
        if (!detail::appendRangeSamples(
                range, sampleGeometryAtSubframes,
                candidate.transformFrames, candidate.shapeFrames)) {
            return false;
        }

        // Preserve the historical whole-frame behavior: if no requested
        // shutter sample lands on an integer frame, setup still needs one
        // geometry sample at the first writable range start.
        if (candidate.shapeFrames.empty() &&
            !detail::checkedInsertFrame(
                candidate.shapeFrames, range.startTime)) {
            return false;
        }
        if (candidate.transformFrames.empty() &&
            !detail::checkedInsertFrame(
                candidate.transformFrames, range.startTime)) {
            return false;
        }
    }

    for (std::size_t index = 0U;
         index < preRollRangeIndices.size(); ++index) {
        const AbcFrameRange& range = ranges[preRollRangeIndices[index]];
        if (!detail::appendPreRollSamples(
                range, candidate.preRollFrames)) {
            return false;
        }
        if (candidate.preRollFrames.empty() &&
            !detail::checkedInsertFrame(
                candidate.preRollFrames, range.startTime)) {
            return false;
        }
    }

    if (candidate.transformFrames.empty() ||
        candidate.shapeFrames.empty()) {
        const double fallbackFrame = ranges.back().startTime;
        if (!detail::checkedInsertFrame(
                candidate.transformFrames, fallbackFrame) ||
            !detail::checkedInsertFrame(
                candidate.shapeFrames, fallbackFrame)) {
            return false;
        }
        candidate.hasExplicitTimeSampling = false;
    }

    candidate.setupFrame = std::min(
        *candidate.transformFrames.begin(), *candidate.shapeFrames.begin());

    // setup() writes both schemas. Include that implicit write in both frame
    // sets so the first actual value and first Alembic sample time agree.
    if (!detail::checkedInsertFrame(
            candidate.transformFrames, candidate.setupFrame) ||
        !detail::checkedInsertFrame(
            candidate.shapeFrames, candidate.setupFrame)) {
        return false;
    }

    detail::AbcPeriodicPattern transformPattern;
    detail::AbcPeriodicPattern shapePattern;
    if (candidate.hasExplicitTimeSampling) {
        const AbcFrameRange& firstRange =
            ranges[sampleRangeIndices.front()];
        if (!detail::makeTransformPattern(
                firstRange, transformPattern)) {
            return false;
        }

        if (sampleGeometryAtSubframes) {
            shapePattern = transformPattern;
        } else {
            shapePattern.enabled = true;
            shapePattern.period = std::max(1.0, firstRange.strideTime);
            shapePattern.firstCycleTimes.push_back(
                *candidate.shapeFrames.begin());
        }
    }

    const bool useDefaultSampling =
        !candidate.hasExplicitTimeSampling;
    if (!detail::buildTimeSamplingPlan(
            candidate.transformFrames, useDefaultSampling,
            transformPattern, candidate.transformTime) ||
        !detail::buildTimeSamplingPlan(
            candidate.shapeFrames, useDefaultSampling,
            shapePattern, candidate.shapeTime)) {
        return false;
    }

    // Bounds are published after either schema updates, not just transforms.
    // Reuse an existing timeline when it contains the entire union. Otherwise
    // keep the exact union acyclic, bounded by the two admitted frame sets.
    if (std::includes(candidate.transformFrames.begin(), candidate.transformFrames.end(),
                      candidate.shapeFrames.begin(), candidate.shapeFrames.end())) {
        candidate.boundsTime = candidate.transformTime;
    } else if (std::includes(candidate.shapeFrames.begin(), candidate.shapeFrames.end(),
                             candidate.transformFrames.begin(), candidate.transformFrames.end())) {
        candidate.boundsTime = candidate.shapeTime;
    } else {
        std::size_t maximumCount = 0U;
        if (!memory::checkedAdd(candidate.transformFrames.size(), candidate.shapeFrames.size(), maximumCount) ||
            maximumCount > 2U * kMaxExportFrameSamples) return false;
        candidate.boundsTime.mode = kAbcAcyclicTimeSampling;
        candidate.boundsTime.sampleTimes.reserve(maximumCount);
        std::set_union(candidate.transformFrames.begin(), candidate.transformFrames.end(),
                       candidate.shapeFrames.begin(), candidate.shapeFrames.end(),
                       std::back_inserter(candidate.boundsTime.sampleTimes));
    }

    output = candidate;
    return true;
}

} // namespace sampling
} // namespace dommy
