#include "CalibrateMenu.hpp"

CalibrateMenu::CalibrateMenu()
{
    // NVS is not yet mounted at static-init time. loadAllMenuSettings() in
    // setup() will load the persisted value before hardware initialisation.
    this->value = (double)LOADCELL_SCALE_FACTOR;
    this->name = "Calibrate Menu";
    this->menuId = CALIBRATE;
};

CalibrateMenu CalibrateMenu::instance = CalibrateMenu();

CalibrateMenu &CalibrateMenu::getCalibrateMenu()
{
    return instance;
}

// implementation for displayMenu and increment
void CalibrateMenu::displayMenu(U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2)
{
    // set current menu to active
    // setActiveMenu(menuId);
    // set global variables
    // TODO: move away from these
}

void CalibrateMenu::handleEncoderChange(int encoderDelta)
{
    // no op
}

void CalibrateMenu::setValue(double newValue)
{
    this->value = newValue;
    bool ok = menuPreferences.begin("scale", false);
    if (ok)
    {
        bool written = menuPreferences.putDouble("calibration", newValue);
        menuPreferences.end();
        Serial.printf("CalibrateMenu: saved scale factor %.4f (written=%d)\n", newValue, written);
    }
    else
    {
        Serial.printf("CalibrateMenu: NVS begin() failed, scale factor %.4f NOT saved\n", newValue);
    }
}

void CalibrateMenu::handleEncoderClick(AiEsp32RotaryEncoder rotaryEncoder)
{
    // Use the global scaleWeight maintained by updateScale — avoids concurrent
    // HX711 access and Kalman filter state corruption from this task.
    if (scaleWeight < 10.0)
    {
        // Guard: refuse to calibrate if nothing meaningful is on the scale
        Serial.printf("CalibrateMenu: calibration aborted, scaleWeight=%.2f (place 100g weight first)\n", scaleWeight);
        DeviceState::setGrinderState(STATUS_IN_MENU);
        DeviceState::setActiveMenu(MAIN_MENU);
        return;
    }
    double newCalibrationValue = value * (scaleWeight / 100.0);

    setValue(newCalibrationValue);
    DeviceState::setGrinderState(STATUS_IN_MENU);
    DeviceState::setActiveMenu(MAIN_MENU);
}
