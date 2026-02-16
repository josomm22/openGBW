#include "scale.hpp"
#include <MathBuffer.h>

HX711 loadcell;
SimpleKalmanFilter kalmanFilter(2.0, 2.0, 0.5);

#define ABS(a) (((a) > 0) ? (a) : ((a) * -1))

TaskHandle_t ScaleTask;
TaskHandle_t ScaleStatusTask;

double setWeight = 0;     // desired amount of coffee
double setCupWeight = 0;  // cup weight set by user
double offset = 0;        // stop x grams prior to set weight
double scaleFactor = 1.0; // load cell multiplier to make 100g read as 100g.
bool grindMode = false;   // false for impulse to start/stop grinding, true for continuous on while grinding
bool scaleMode = false;   // use as regular scale with timer if true

double scaleWeight = 0;     // current weight
bool grinderActive = false; // needed for continuous mode
MathBuffer<double, 100> weightHistory;
unsigned long scaleLastUpdatedAt = 0;
unsigned long lastSignificantWeightChangeAt = 0;
unsigned long lastTareAt = 0; // if 0, should tare load cell, else represent when it was last tared
bool scaleReady = false;
double cupWeightEmpty = 0; // measured actual cup weight
unsigned long startedGrindingAt = 0;
unsigned long finishedGrindingAt = 0;
bool newOffset = false;
bool lastTriggerButtonPressed = false;
bool lastScaleMode = false;

void tareScale()
{
  Serial.println("Taring scale");
  loadcell.tare(TARE_MEASURES);
  // Reset Kalman filter to eliminate drift
  kalmanFilter.setProcessNoise(0.5);
  kalmanFilter.setMeasurementError(2.0);
  kalmanFilter.setEstimateError(2.0);
  lastTareAt = millis();
}

void grinderToggle()
{
  if (!scaleMode)
  {
    if (grindMode)
    {
      grinderActive = !grinderActive;
      digitalWrite(GRINDER_ACTIVE_PIN, grinderActive);
    }
    else
    {
      digitalWrite(GRINDER_ACTIVE_PIN, 1);
      delay(100);
      digitalWrite(GRINDER_ACTIVE_PIN, 0);
    }
  }
}

void scaleStatusLoop(void *p)
{
  double tenSecAvg;
  for (;;)
  {
    bool triggerButtonPressed = digitalRead(GRIND_TRIGGER_BUTTON_PIN) == LOW;
    bool triggerButtonEdge = triggerButtonPressed && !lastTriggerButtonPressed;
    lastTriggerButtonPressed = triggerButtonPressed;

    // recalibrate scale if new calibration has been set
    double newScaleFactor = calibrateMenu.getValue();
    if (newScaleFactor != scaleFactor)
    {
      loadcell.set_scale(newScaleFactor);
      scaleFactor = newScaleFactor;
    }
    // Update settings
    setWeight = closedMenu.getValue();
    offset = offsetMenu.getValue();
    setCupWeight = cupMenu.getValue();
    grindMode = grindModeMenu.getValue();
    scaleMode = scaleModeMenu.getValue();

    if (lastScaleMode && !scaleMode)
    {
      grinderActive = false;
      digitalWrite(GRINDER_ACTIVE_PIN, 0);
    }
    lastScaleMode = scaleMode;

    tenSecAvg = weightHistory.averageSince((int64_t)millis() - 10000);

    // TODO: add GrinderState for sleep
    if (ABS(tenSecAvg - scaleWeight) > SIGNIFICANT_WEIGHT_CHANGE)
    {
      lastSignificantWeightChangeAt = millis();
    }

    GrinderState grinderState = DeviceState::getGrinderState();
    if (scaleMode)
    {
      if (grinderState != STATUS_IN_MENU && grinderState != STATUS_IN_SUBMENU)
      {
        DeviceState::setGrinderState(STATUS_EMPTY);
        startedGrindingAt = 0;
        finishedGrindingAt = 0;
      }

      bool shouldRunGrinder = triggerButtonPressed && scaleReady;
      if (shouldRunGrinder != grinderActive)
      {
        grinderActive = shouldRunGrinder;
        digitalWrite(GRINDER_ACTIVE_PIN, grinderActive);
      }

      if (millis() - lastTareAt > TARE_MIN_INTERVAL && ABS(tenSecAvg) > 0.2 && tenSecAvg < 3 && scaleWeight < 3)
      {
        lastTareAt = 0;
      }
      delay(50);
      continue;
    }

    if (grinderState == STATUS_EMPTY)
    {
      if (triggerButtonEdge && !scaleMode)
      {
        Serial.println("Manual grind trigger button pressed");
        cupWeightEmpty = weightHistory.averageSince((int64_t)millis() - 500);
        DeviceState::setGrinderState(STATUS_GRINDING_IN_PROGRESS);
        newOffset = true;
        startedGrindingAt = millis();
        grinderToggle();
        continue;
      }

      if (millis() - lastTareAt > TARE_MIN_INTERVAL && ABS(tenSecAvg) > 0.2 && tenSecAvg < 3 && scaleWeight < 3)
      {
        // tare if: not tared recently, more than 0.2 away from 0, less than 3 grams total (also works for negative weight)
        lastTareAt = 0;
      }

      // Start grinding if both min and max weight detected in the past 1s are less than cup weight + detection tolerance.
      if (ABS(weightHistory.minSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE &&
          ABS(weightHistory.maxSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE)
      {
        // using average over last 500ms as empty cup weight
        Serial.println("Starting grinding");
        cupWeightEmpty = weightHistory.averageSince((int64_t)millis() - 500);
        DeviceState::setGrinderState(STATUS_GRINDING_IN_PROGRESS);

        if (!scaleMode)
        {
          newOffset = true;
          startedGrindingAt = millis();
        }

        grinderToggle();
        continue;
      }
    }
    else if (grinderState == STATUS_GRINDING_IN_PROGRESS)
    {
      if (!scaleReady)
      {

        grinderToggle();
        DeviceState::setGrinderState(STATUS_GRINDING_FAILED);
      }
      if (scaleMode && startedGrindingAt == 0 && scaleWeight - cupWeightEmpty >= 0.1)
      {
        Serial.printf("Started grinding at: %d\n", millis());
        startedGrindingAt = millis();
        continue;
      }

      if (millis() - startedGrindingAt > MAX_GRINDING_TIME && !scaleMode)
      {
        Serial.println("Failed because grinding took too long");

        grinderToggle();
        DeviceState::setGrinderState(STATUS_GRINDING_FAILED);
        continue;
      }

      if (
          millis() - startedGrindingAt > 2000 &&                                  // started grinding at least 2s ago
          scaleWeight - weightHistory.firstValueOlderThan(millis() - 2000) < 1 && // less than a gram has been grinded in the last 2 second
          !scaleMode)
      {
        Serial.println("Failed because no change in weight was detected");

        grinderToggle();
        DeviceState::setGrinderState(STATUS_GRINDING_FAILED);
        continue;
      }

      if (weightHistory.minSince((int64_t)millis() - 200) < cupWeightEmpty - CUP_DETECTION_TOLERANCE && !scaleMode)
      {
        Serial.printf("Failed because weight too low, min: %f, min value: %f\n", weightHistory.minSince((int64_t)millis() - 200), CUP_WEIGHT + CUP_DETECTION_TOLERANCE);

        grinderToggle();
        DeviceState::setGrinderState(STATUS_GRINDING_FAILED);
        continue;
      }
      double currentOffset = offset;
      if (scaleMode)
      {
        currentOffset = 0;
      }
      if (weightHistory.maxSince((int64_t)millis() - 200) >= cupWeightEmpty + setWeight + currentOffset)
      {
        Serial.println("Finished grinding");
        finishedGrindingAt = millis();

        grinderToggle();
        DeviceState::setGrinderState(STATUS_GRINDING_FINISHED);
        continue;
      }
    }
    else if (grinderState == STATUS_GRINDING_FINISHED)
    {
      double currentWeight = weightHistory.averageSince((int64_t)millis() - 500);
      if (scaleWeight < 5)
      {
        Serial.println("Going back to empty");
        startedGrindingAt = 0;
        DeviceState::setGrinderState(STATUS_EMPTY);
        continue;
      }
      // Update grind weight offset if necessary
      else if (currentWeight != setWeight + cupWeightEmpty && millis() - finishedGrindingAt > 1500 && newOffset)
      {
        // TODO: move this to an offsetMenu function. Something like updateOffsetAfterGrind()
        offset = offset + setWeight + cupWeightEmpty - currentWeight;
        if (ABS(offset) >= setWeight)
        {
          offset = COFFEE_DOSE_OFFSET;
        }
        offsetMenu.setValue(offset);
        newOffset = false;
      }
    }
    else if (grinderState == STATUS_GRINDING_FAILED)
    {
      if (scaleWeight >= GRINDING_FAILED_WEIGHT_TO_RESET)
      {
        Serial.println("Going back to empty");
        DeviceState::setGrinderState(STATUS_EMPTY);
        continue;
      }
    }
    delay(50);
  }
}

// TODO: split out grinder and scale into their own classes that other classes can get status from.
// Monitor all inputs in looping threads, handle all outputs in menus
void updateScale(void *parameter)
{
  float lastEstimate;

  for (;;)
  {
    if (lastTareAt == 0)
    {
      Serial.println("retaring scale");
      Serial.println("current offset");
      Serial.println(offset);
      tareScale();
    }
    if (loadcell.wait_ready_timeout(200))
    {
      lastEstimate = kalmanFilter.updateEstimate(loadcell.get_units(3));

      // Apply deadband: readings within ±0.15g treated as 0 to eliminate drift
      if (ABS(lastEstimate) < 0.15)
      {
        scaleWeight = 0.0;
      }
      else
      {
        scaleWeight = lastEstimate;
      }

      scaleLastUpdatedAt = millis();
      weightHistory.push(scaleWeight);
      scaleReady = true;
      Serial.printf("Scale weight: %.2f g\n", scaleWeight);
    }
    else
    {
      Serial.println("HX711 not found.");
      scaleReady = false;
    }
    delay(10);
  }
}

void setupScale()
{

  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  pinMode(GRINDER_ACTIVE_PIN, OUTPUT);
  digitalWrite(GRINDER_ACTIVE_PIN, 0);
  pinMode(GRIND_TRIGGER_BUTTON_PIN, INPUT_PULLUP);

  scaleFactor = calibrateMenu.getValue();
  setWeight = closedMenu.getValue();
  offset = offsetMenu.getValue();
  setCupWeight = cupMenu.getValue();
  scaleMode = scaleModeMenu.getValue();
  grindMode = grindModeMenu.getValue();

  loadcell.set_scale(scaleFactor);

  xTaskCreatePinnedToCore(
      updateScale, /* Function to implement the task */
      "Scale",     /* Name of the task */
      10000,       /* Stack size in words */
      NULL,        /* Task input parameter */
      0,           /* Priority of the task */
      &ScaleTask,  /* Task handle. */
      1);          /* Core where the task should run */

  xTaskCreatePinnedToCore(
      scaleStatusLoop,  /* Function to implement the task */
      "ScaleStatus",    /* Name of the task */
      10000,            /* Stack size in words */
      NULL,             /* Task input parameter */
      0,                /* Priority of the task */
      &ScaleStatusTask, /* Task handle. */
      1                 /* Core where the task should run */
  );
}
