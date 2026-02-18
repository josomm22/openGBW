#include "scale.hpp"
#include <MathBuffer.h>
#include "grind/GrindController.hpp"

HX711 loadcell;
SimpleKalmanFilter kalmanFilter(2.0, 2.0, 0.5);

#define ABS(a) (((a) > 0) ? (a) : ((a) * -1))

TaskHandle_t ScaleTask;
TaskHandle_t ScaleStatusTask;

double setWeight = 0;     // desired amount of coffee
double offset = 0;        // stop x grams prior to set weight
double scaleFactor = 1.0; // load cell multiplier to make 100g read as 100g.
bool grindMode = false;   // false for impulse to start/stop grinding, true for continuous on while grinding
bool scaleMode = false;   // use as regular scale with timer if true

double scaleWeight = 0;     // current weight
bool grinderActive = false; // needed for continuous mode
unsigned long scaleLastUpdatedAt = 0;
unsigned long lastSignificantWeightChangeAt = 0;
unsigned long lastTareAt = 0; // if 0, should tare load cell, else represent when it was last tared
bool scaleReady = false;
bool lastTriggerButtonPressed = false;
bool lastScaleMode = false;

// Forward declarations for GrindController callbacks
void grinderToggle();
void tareScale();

GrindController controller(
    []()
    { grinderToggle(); },
    []()
    { tareScale(); },
    [](double newOff)
    { offsetMenu.setValue(newOff); });

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
    grindMode = grindModeMenu.getValue();
    scaleMode = scaleModeMenu.getValue();

    if (lastScaleMode && !scaleMode)
    {
      grinderActive = false;
      digitalWrite(GRINDER_ACTIVE_PIN, 0);
    }
    lastScaleMode = scaleMode;

    tenSecAvg = controller.weightAverageSince((int64_t)millis() - 10000);

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

    // Handle purge mode
    if (grinderState == STATUS_IN_SUBMENU && DeviceState::getActiveMenu() == PURGE)
    {
      if (triggerButtonEdge)
      {
        Serial.println("Purge button pressed - running grinder for 3 seconds");
        digitalWrite(GRINDER_ACTIVE_PIN, 1);
        delay(3000);
        digitalWrite(GRINDER_ACTIVE_PIN, 0);
        Serial.println("Purge complete - returning to main screen");
        DeviceState::setActiveMenu(NONE);
        DeviceState::setGrinderState(STATUS_EMPTY);
      }
      delay(50);
      continue;
    }

    // Auto-tare when in empty state and scale drifts slightly
    if (grinderState == STATUS_EMPTY)
    {
      if (millis() - lastTareAt > TARE_MIN_INTERVAL && ABS(tenSecAvg) > 0.2 && tenSecAvg < 3 && scaleWeight < 3)
      {
        // tare if: not tared recently, more than 0.2 away from 0, less than 3 grams total (also works for negative weight)
        lastTareAt = 0;
      }
    }

    // Advance the grind-by-weight state machine
    GrinderState newState = controller.update(
        grinderState, scaleWeight, scaleReady,
        setWeight, offset, triggerButtonEdge, scaleMode);
    if (newState != grinderState)
    {
      DeviceState::setGrinderState(newState);
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
      controller.pushWeight(scaleWeight);
      scaleReady = true;
      Serial.printf("Scale weight: %.2f g\n", scaleWeight);
    }
    else
    {
      Serial.println("HX711 not found.");
      scaleReady = false;
      // Set scaleLastUpdatedAt to allow display to show error instead of "Initializing..."
      if (scaleLastUpdatedAt == 0)
      {
        scaleLastUpdatedAt = millis();
      }
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
