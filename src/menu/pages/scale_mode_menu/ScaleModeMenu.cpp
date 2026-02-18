#include "ScaleModeMenu.hpp"

ScaleModeMenu::ScaleModeMenu()
{
    // NVS is not yet mounted at static-init time. loadAllMenuSettings() will
    // load the persisted value before hardware initialisation.
    this->value = false;
    this->name = "Scale Mode Menu";
    this->menuId = SCALE_MODE;
};

ScaleModeMenu ScaleModeMenu::instance = ScaleModeMenu();

ScaleModeMenu &ScaleModeMenu::getScaleModeMenu()
{
    return instance;
}

// implementation for displayMenu and increment
void ScaleModeMenu::displayMenu(U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2)
{
    // set current menu to active
    // setActiveMenu(menuId);
    // set global variables
    // TODO: move away from these
}

void ScaleModeMenu::setValue(bool newValue)
{
    this->value = newValue;
    menuPreferences.begin("scale", false);
    menuPreferences.putBool("scaleMode", newValue);
    menuPreferences.end();
    Serial.print("ScaleMode set to: ");
    Serial.println(newValue);
}

// toggle value but don't save until user clicks the encoder
void ScaleModeMenu::handleEncoderChange(int encoderDelta)
{
    value = !value;
}

void ScaleModeMenu::handleEncoderClick(AiEsp32RotaryEncoder rotaryEncoder)
{
    setValue(value);
    DeviceState::setGrinderState(STATUS_IN_MENU);
    DeviceState::setActiveMenu(MAIN_MENU);
}