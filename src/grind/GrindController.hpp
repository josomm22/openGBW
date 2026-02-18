#pragma once

#include <functional>
#include <MathBuffer.h>
#include "../menu/DeviceState.hpp"
#include "grind_constants.hpp"

/**
 * GrindController encapsulates the grind-by-weight state machine and the
 * MathBuffer weight history that drives it.
 *
 * Hardware interactions (relay, tare, NVS persistence) are injected as
 * std::function callbacks so the class can be tested on the native platform
 * without any ESP32 dependencies.
 *
 * Thread-safety note: pushWeight() is called from the scale-reading task
 * while update() is called from the status-loop task.  On the target
 * hardware both tasks are pinned to core 1, so no additional locking is
 * needed.  For tests, everything runs on one thread.
 */
class GrindController
{
public:
    using ToggleFn = std::function<void()>;
    using TareFn = std::function<void()>;
    using SaveOffsetFn = std::function<void(double)>;

    /**
     * @param toggleFn      Called to start/stop the grinder relay.
     * @param tareFn        Called to zero the scale before a grind starts.
     * @param saveOffsetFn  Called with the newly computed offset after a grind.
     */
    GrindController(ToggleFn toggleFn, TareFn tareFn, SaveOffsetFn saveOffsetFn);

    /** Push a fresh weight reading from the scale task into the ring buffer. */
    void pushWeight(double weight);

    /**
     * Expose the rolling average of weight readings since cutoffMs.
     * Used by the status loop for auto-tare decisions and sleep detection.
     */
    double weightAverageSince(int64_t cutoffMs);

    /** Timestamps used by display.cpp to render grind duration. */
    unsigned long getStartedGrindingAt() const { return startedGrindingAt; }
    unsigned long getFinishedGrindingAt() const { return finishedGrindingAt; }

    /**
     * Advance the state machine by one iteration.
     *
     * @param currentState  The GrinderState read from DeviceState this tick.
     * @param scaleWeight   Latest instantaneous weight (g).
     * @param scaleReady    Whether the HX711 is responsive.
     * @param setWeight     Target dose weight (g), from the closed-loop menu.
     * @param offset        Pre-stop offset (g), from the offset menu (negative = stop early).
     * @param triggerEdge   True only on the rising edge of the trigger button.
     * @param scaleMode     True when operating as a plain scale (no auto-stop).
     * @return              The new GrinderState for this tick.
     */
    GrinderState update(
        GrinderState currentState,
        double scaleWeight,
        bool scaleReady,
        double setWeight,
        double offset,
        bool triggerEdge,
        bool scaleMode);

private:
    MathBuffer<double, 100> weightHistory;

    ToggleFn grinderToggleFn;
    TareFn tareFn;
    SaveOffsetFn saveOffsetFn;

    unsigned long startedGrindingAt = 0;
    unsigned long finishedGrindingAt = 0;
    bool newOffset = false;
};
