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
            weightHistory.reset(); // discard pre-tare readings
            delay(TARE_WAIT_MS);   // yield so updateScale task can finish tare
            startedGrindingAt = millis();
            finishedGrindingAt = 0;
            grindPhase = GrindPhase::PREDICTIVE;
            phaseStartAt = millis();
            pulseAttempts = 0;
            lastFlowRate = 0.0;
            grinderToggleFn();
            return STATUS_GRINDING_IN_PROGRESS;
        }
        return STATUS_EMPTY;
    }

    // ── STATUS_GRINDING_IN_PROGRESS ───────────────────────────────────────
    if (currentState == STATUS_GRINDING_IN_PROGRESS)
    {
        // 1. Hard timeout — grinder may still be running if in PREDICTIVE or PULSE_ACTIVE
        if (!scaleMode && now - startedGrindingAt > MAX_GRINDING_TIME)
        {
            Serial.println("Failed because grinding took too long");
            if (grindPhase == GrindPhase::PREDICTIVE || grindPhase == GrindPhase::PULSE_ACTIVE)
            {
                grinderToggleFn();
            }
            return STATUS_GRINDING_FAILED;
        }

        // 2. Stall detection — only while grinder is running (PREDICTIVE phase)
        //    After GRIND_STALL_WINDOW_MS with < 1g change → something is wrong.
        if (!scaleMode &&
            grindPhase == GrindPhase::PREDICTIVE &&
            now - startedGrindingAt > GRIND_STALL_WINDOW_MS &&
            ABS(scaleWeight - weightHistory.firstValueOlderThan((int64_t)now - GRIND_STALL_WINDOW_MS)) < 1)
        {
            Serial.println("Failed because no change in weight was detected");
            grinderToggleFn();
            return STATUS_GRINDING_FAILED;
        }

        // 3. Scale mode: simple threshold, no predictive logic
        if (scaleMode)
        {
            if (weightHistory.maxSince((int64_t)now - 200) >= setWeight)
            {
                finishedGrindingAt = now;
                grinderToggleFn();
                return STATUS_GRINDING_FINISHED;
            }
            return STATUS_GRINDING_IN_PROGRESS;
        }

        // ── PREDICTIVE phase ──────────────────────────────────────────────
        if (grindPhase == GrindPhase::PREDICTIVE)
        {
            // Need at least GRIND_FLOW_WINDOW_MS of history before computing flow rate.
            if (now - startedGrindingAt >= (unsigned long)(GRIND_FLOW_WINDOW_MS + 200))
            {
                double olderWeight = weightHistory.firstValueOlderThan((int64_t)now - GRIND_FLOW_WINDOW_MS);
                double flowRate = (scaleWeight - olderWeight) / (GRIND_FLOW_WINDOW_MS / 1000.0);
                if (flowRate >= GRIND_FLOW_THRESHOLD_GPS)
                {
                    lastFlowRate = flowRate;
                }

                if (lastFlowRate >= GRIND_FLOW_THRESHOLD_GPS)
                {
                    double motorLatencyMs = (offset > 0) ? offset : GRIND_MOTOR_LATENCY_MS;
                    double coastEstimate = (motorLatencyMs / 1000.0) * lastFlowRate;
                    // Floor: never stop more than 2g below target, even with very high flow
                    if (coastEstimate > 2.0) coastEstimate = 2.0;
                    double stopAt = setWeight - coastEstimate;

                    if (scaleWeight >= stopAt)
                    {
                        Serial.printf("Predictive stop at %.2fg (flow=%.2fg/s, coast=%.2fg, stopAt=%.2fg)\n",
                                     scaleWeight, lastFlowRate, coastEstimate, stopAt);
                        grinderToggleFn();
                        grindPhase = GrindPhase::SETTLING;
                        phaseStartAt = now;
                    }
                }
            }
            return STATUS_GRINDING_IN_PROGRESS;
        }

        // ── SETTLING / PULSE_SETTLING phase ──────────────────────────────
        if (grindPhase == GrindPhase::SETTLING || grindPhase == GrindPhase::PULSE_SETTLING)
        {
            unsigned long elapsed = now - phaseStartAt;
            if (elapsed < GRIND_SETTLING_MS)
            {
                return STATUS_GRINDING_IN_PROGRESS;
            }
            // Scale stable when max-min over the settling window is small.
            // Allow up to 2× settling time for a noisy reading to calm down.
            double hi = weightHistory.maxSince((int64_t)now - GRIND_SETTLING_MS);
            double lo = weightHistory.minSince((int64_t)now - GRIND_SETTLING_MS);
            if ((hi - lo) > 0.5 && elapsed < GRIND_SETTLING_MS * 2)
            {
                return STATUS_GRINDING_IN_PROGRESS;
            }
            grindPhase = GrindPhase::PULSE_DECIDE;
            phaseStartAt = now;
            return STATUS_GRINDING_IN_PROGRESS;
        }

        // ── PULSE_DECIDE phase ────────────────────────────────────────────
        if (grindPhase == GrindPhase::PULSE_DECIDE)
        {
            double settledWeight = weightHistory.averageSince((int64_t)now - GRIND_SETTLING_MS);
            double error = setWeight - settledWeight;
            Serial.printf("Pulse decide: settled=%.2fg, error=%.2fg, attempts=%d\n",
                         settledWeight, error, pulseAttempts);

            if (error <= GRIND_PULSE_TOLERANCE_G || pulseAttempts >= GRIND_MAX_PULSE_ATTEMPTS)
            {
                Serial.printf("Grinding finished: final=%.2fg after %d pulses\n", settledWeight, pulseAttempts);
                finishedGrindingAt = now;
                return STATUS_GRINDING_FINISHED;
            }

            double flowRate = (lastFlowRate >= GRIND_FLOW_THRESHOLD_GPS)
                                  ? lastFlowRate
                                  : GRIND_PULSE_FLOW_FALLBACK_GPS;
            double motorLatencyMs = (offset > 0) ? offset : GRIND_MOTOR_LATENCY_MS;
            double pulseDurationMs = motorLatencyMs + (error / flowRate) * 1000.0;
            if (pulseDurationMs > GRIND_PULSE_MAX_DURATION_MS) pulseDurationMs = GRIND_PULSE_MAX_DURATION_MS;
            if (pulseDurationMs < GRIND_MOTOR_LATENCY_MS) pulseDurationMs = GRIND_MOTOR_LATENCY_MS;

            Serial.printf("Firing pulse %.0fms (error=%.2fg, flow=%.2fg/s)\n", pulseDurationMs, error, flowRate);
            grinderToggleFn(); // turn ON
            pulseAttempts++;
            pulseEndAt = now + (unsigned long)pulseDurationMs;
            grindPhase = GrindPhase::PULSE_ACTIVE;
            phaseStartAt = now;
            return STATUS_GRINDING_IN_PROGRESS;
        }

        // ── PULSE_ACTIVE phase ────────────────────────────────────────────
        if (grindPhase == GrindPhase::PULSE_ACTIVE)
        {
            if (now >= pulseEndAt)
            {
                grinderToggleFn(); // turn OFF
                grindPhase = GrindPhase::PULSE_SETTLING;
                phaseStartAt = now;
            }
            return STATUS_GRINDING_IN_PROGRESS;
        }

        return STATUS_GRINDING_IN_PROGRESS;
    }

    // ── STATUS_GRINDING_FINISHED ──────────────────────────────────────────
    if (currentState == STATUS_GRINDING_FINISHED)
    {
        if (scaleWeight < 5.0)
        {
            Serial.println("Going back to empty");
            startedGrindingAt = 0;
            return STATUS_EMPTY;
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
