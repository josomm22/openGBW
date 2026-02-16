#pragma once

#include "../Menu.hpp"

// Menu for purging the grinder
class PurgeMenu : public Menu<bool>
{
private:
    // private constructor for singleton
    PurgeMenu();
    static PurgeMenu instance;

public:
    // Accessor
    static PurgeMenu &getPurgeMenu();

    void displayMenu(U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2) override;

    void handleEncoderChange(int encoderDelta) override;

    void handleEncoderClick(AiEsp32RotaryEncoder encoder) override;
};
