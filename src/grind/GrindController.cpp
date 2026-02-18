#include "GrindController.hpp"

#define ABS(a) (((a) > 0) ? (a) : ((a) * -1))

GrindController::GrindController(ToggleFn toggleFn, TareFn tareFn, SaveOffsetFn saveOffsetFn)
    : grinderToggleFn(toggleFn), tareFn(tareFn), saveOffsetFn(saveOffsetFn)
{
}

void GrindController::pushWeight(double weight)
{
    weightHistory.push(weight);
}

double GrindController::weightAverageSince(int64_t cutoffMs)
{
    return weightHistory.averageSince(cutoffMs);
}

GrinderState GrindController::update(
    GrinderState currentState,
    double scaleWeight,
    bool scaleReady,
    double setWeight,
    double offset,
    bool triggerEdge,
    bool scaleMode)
{
    unsigned long now = millis();

    // ── STATUS_EMPTY ──────────────────────────────────────────────────────
    if (currentState == STATUS_EMPTY)
    {
        if (triggerEdge && !scaleMode)
        {
            Serial.println("Manual grind trigger button pressed");
            Serial.println("Taring scale before grinding");
            tareFn();
            delay(800);                   // yield so updateScale task can run the tare (5 measures ~500ms) before starting
            startedGrindingAt = millis(); // re-sample after delay so timers are accurate
            finishedGrindingAt = 0;
            newOffset = true;
            grinderToggleFn();
            return STATUS_GRINDING_IN_PROGRESS;
        }
        return STATUS_EMPTY;
    }

    // ── STATUS_GRINDING_IN_PROGRESS ───────────────────────────────────────
    if (currentState == STATUS_GRINDING_IN_PROGRESS)
    {
        // 1. Hard time-out
        if (!scaleMode && now - startedGrindingAt > MAX_GRINDING_TIME)
        {
            Serial.println("Failed because grinding took too long");
            grinderToggleFn();
            return STATUS_GRINDING_FAILED;
        }

        // 3. Stall detection: less than 1 g change over the last 4 s
        //    4 s gives slow or spooling-up grinders time to deliver the first gram.
        //    ABS guards against a slightly-negative post-tare baseline tripping the check.
        if (!scaleMode &&
            now - startedGrindingAt > 4000 &&
            ABS(scaleWeight - weightHistory.firstValueOlderThan((int64_t)now - 4000)) < 1)
        {
            Serial.println("Failed because no change in weight was detected");
            grinderToggleFn();
            return STATUS_GRINDING_FAILED;
        }

        // 4. Completion check
        double currentOffset = scaleMode ? 0.0 : offset;
        if (weightHistory.maxSince((int64_t)now - 200) >= setWeight + currentOffset)
        {
            Serial.println("Finished grinding");
            finishedGrindingAt = now;
            grinderToggleFn();
            return STATUS_GRINDING_FINISHED;
        }

        return STATUS_GRINDING_IN_PROGRESS;
    }

    // ── STATUS_GRINDING_FINISHED ──────────────────────────────────────────
    if (currentState == STATUS_GRINDING_FINISHED)
    {
        // Wait for cup to be removed
        if (scaleWeight < 5.0)
        {
            Serial.println("Going back to empty");
            startedGrindingAt = 0;
            return STATUS_EMPTY;
        }

        // Auto-adjust offset once, 1.5 s after the grinder stopped
        double currentWeight = weightHistory.averageSince((int64_t)now - 500);
        if (currentWeight != setWeight &&
            now - finishedGrindingAt > 1500 &&
            newOffset)
        {
            double newOffsetVal = offset + setWeight - currentWeight;
            if (ABS(newOffsetVal) >= setWeight)
            {
                newOffsetVal = COFFEE_DOSE_OFFSET;
            }
            saveOffsetFn(newOffsetVal);
            newOffset = false;
        }

        return STATUS_GRINDING_FINISHED;
    }

    // ── STATUS_GRINDING_FAILED ────────────────────────────────────────────
    if (currentState == STATUS_GRINDING_FAILED)
    {
        if (scaleWeight >= GRINDING_FAILED_WEIGHT_TO_RESET)
        {
            Serial.println("Going back to empty");
            return STATUS_EMPTY;
        }
        return STATUS_GRINDING_FAILED;
    }

    // All other states (STATUS_IN_MENU, STATUS_IN_SUBMENU) pass through
    return currentState;
}
