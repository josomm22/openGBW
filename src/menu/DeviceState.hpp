#pragma once

// Enum for device states
enum GrinderState
{
    STATUS_EMPTY = 0,
    STATUS_GRINDING_IN_PROGRESS = 1,
    STATUS_GRINDING_FINISHED = 2,
    STATUS_GRINDING_FAILED = 3,
    STATUS_IN_MENU = 4,
    STATUS_IN_SUBMENU = 5
};

// Enum for menu IDs
enum MenuId
{
    NONE = 0,
    MAIN_MENU = 1,
    CALIBRATE = 2,
    OFFSET = 3,
    SCALE_MODE = 4,
    GRINDING_MODE = 5,
    SLEEP = 6,
    EXIT = 7,
    RESET = 8,
    PURGE = 9
};

class DeviceState
{
private:
    static MenuId activeMenu;
    static GrinderState grinderState;

public:
    // Setters
    static void setActiveMenu(MenuId activeMenu);
    static void setGrinderState(GrinderState grinderState);

    // Getters
    static MenuId getActiveMenu();
    static GrinderState getGrinderState();
};