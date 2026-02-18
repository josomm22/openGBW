#include "OffsetMenu.hpp"

OffsetMenu::OffsetMenu()
{
    // NVS is not yet mounted at static-init time. loadAllMenuSettings() will
    // load the persisted value before hardware initialisation.
    this->value = (double)COFFEE_DOSE_OFFSET;
    this->name = "Offset Menu";
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
    menuPreferences.putDouble("offset", newValue);
    menuPreferences.end();
    Serial.print("Offset set to: ");
    Serial.println(newValue);
}

// TODO: remove this or remove the increment function above
void OffsetMenu::handleEncoderChange(int encoderDelta)
{
    double newValue = value + ((float)encoderDelta) / 100;
    if (abs(newValue) >= 18.0)
    {
        newValue = 18.0; // Prevent nonsensical offsets
    }
    setValue(newValue);
}

void OffsetMenu::handleEncoderClick(AiEsp32RotaryEncoder rotaryEncoder)
{
    DeviceState::setGrinderState(STATUS_IN_MENU);
    DeviceState::setActiveMenu(MAIN_MENU);
}