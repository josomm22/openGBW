#include "OffsetMenu.hpp"

OffsetMenu::OffsetMenu()
{
    // NVS is not yet mounted at static-init time. loadAllMenuSettings() will
    // load the persisted value before hardware initialisation.
    this->value = (double)GRIND_MOTOR_LATENCY_MS;
    this->name = "Latency Menu";
    this->menuId = OFFSET;
};

OffsetMenu OffsetMenu::instance = OffsetMenu();

OffsetMenu &OffsetMenu::getOffsetMenu()
{
    return instance;
}

// implementation for displayMenu and increment
void OffsetMenu::displayMenu(U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2)
{
    // set current menu to active
    // setActiveMenu(menuId);
    // set global variables
    // TODO: move away from these
}

void OffsetMenu::setValue(double newValue)
{
    this->value = newValue;
    menuPreferences.begin("scale", false);
    menuPreferences.putDouble("latency", newValue);
    menuPreferences.end();
    Serial.print("Offset set to: ");
    Serial.println(newValue);
}

// TODO: remove this or remove the increment function above
void OffsetMenu::handleEncoderChange(int encoderDelta)
{
    double newValue = value + ((float)encoderDelta) * 5.0; // 5ms per click
    if (newValue < 50.0)  newValue = 50.0;
    if (newValue > 500.0) newValue = 500.0;
    setValue(newValue);
}

void OffsetMenu::handleEncoderClick(AiEsp32RotaryEncoder rotaryEncoder)
{
    DeviceState::setGrinderState(STATUS_IN_MENU);
    DeviceState::setActiveMenu(MAIN_MENU);
}