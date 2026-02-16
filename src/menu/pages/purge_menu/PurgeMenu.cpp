#include "PurgeMenu.hpp"

PurgeMenu::PurgeMenu()
{
    this->value = false;
    this->name = "Purge Menu";
    this->menuId = PURGE;
};

PurgeMenu PurgeMenu::instance = PurgeMenu();

PurgeMenu &PurgeMenu::getPurgeMenu()
{
    return instance;
}

void PurgeMenu::displayMenu(U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2)
{
    // Display is handled elsewhere
}

void PurgeMenu::handleEncoderChange(int encoderDelta)
{
    // No encoder interaction needed for purge menu
}

void PurgeMenu::handleEncoderClick(AiEsp32RotaryEncoder rotaryEncoder)
{
    // Return to main menu on encoder click
    DeviceState::setActiveMenu(MAIN_MENU);
    DeviceState::setGrinderState(STATUS_IN_MENU);
}
