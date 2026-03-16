#pragma once

#include <functional>
#include <MathBuffer.h>
#include "../menu/DeviceState.hpp"
#include "grind_constants.hpp"

/**
 * Internal phases of the grind-by-weight algorithm.
 * Invisible to callers — the public GrinderState stays the same.
 */
enum class GrindPhase {
    PREDICTIVE,    // grinder running; measuring flow rate, dynamic stop
    SETTLING,      // grinder off; waiting for weight to stabilise
    PULSE_DECIDE,  // evaluate error; decide whether to pulse or finish
    PULSE_ACTIVE,  // grinder running for a timed correction pulse
    PULSE_SETTLING // waiting for scale to stabilise after pulse
};

/**
 * GrindController encapsulates the grind-by-weight state machine and the
 * MathBuffer weight history that drives it.
 *
 * Hardware interactions (relay, tare, NVS persistence) are injected as
 * std::function callbacks so the class can be tested on the native platform
 * without any ESP32 dependencies.
 *
 * Algorithm overview:
 *  1. PREDICTIVE — run grinder, measure flow rate, stop when expected coast
 *     will carry the weight to the target.
 *  2. SETTLING — wait for the scale to stabilise.
 *  3. PULSE_DECIDE — if still below target by more than GRIND_PULSE_TOLERANCE_G,
 *     fire a short correction pulse; otherwise finish.
 *  4. PULSE_ACTIVE / PULSE_SETTLING — execute the pulse and wait again.
 *  Repeat from PULSE_DECIDE up to GRIND_MAX_PULSE_ATTEMPTS times.
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
     * @param setWeight     Target dose weight (g).
     * @param offset        Unused by the predictive algorithm; kept for API compatibility.
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

    GrindPhase grindPhase = GrindPhase::PREDICTIVE;
    unsigned long phaseStartAt = 0;
    double lastFlowRate = 0.0;
    int pulseAttempts = 0;
    unsigned long pulseEndAt = 0;
};
