#pragma once
#include <Arduino.h>

// Timing constants
const uint8_t WORK_DURATION = 25;  // in minutes
const uint8_t SHORT_BREAK_DURATION = 5;  // in minutes
const uint8_t LONG_BREAK_DURATION = 15;  // in minutes
const uint8_t POMODOROS_BEFORE_LONG_BREAK = 4;
// const uint8_t IDLE_TIMEOUT_MINUTES = 1;  // in minutes
const uint8_t IDLE_TIMEOUT_BATTERY_MINUTES = 5;  // Aggressive timeout on battery
const uint8_t IDLE_TIMEOUT_USB_MINUTES = 30;     // Longer timeout on USB


// Alert configurations (defaults)
const uint8_t DEFAULT_ALARM_DURATION = 2;  // in seconds
const uint8_t ALERT_BLINK_COUNT = 5;       // Number of times to blink

// Task management constants
const uint8_t MAX_TASKS = 12;  // Maximum number of tasks to track

enum class TimerState {
    IDLE,
    WORK,
    WIND_UP,
    SHORT_BREAK,
    LONG_BREAK,
    PAUSED_WORK,
    PAUSED_SHORT_BREAK,
    PAUSED_LONG_BREAK,
    ALERT
};

// Add to existing enums
enum class MenuState {
    CLOSED,
    MENU_LIST,      // Showing menu items
    EDITING_VALUE   // Editing a specific value
};

enum class MenuItem {
    POMODORO_LENGTH,
    SHORT_BREAK_LENGTH,
    LONG_BREAK_LENGTH,
    POMODOROS_BEFORE_LONG_BREAK,
    MANAGE_TASKS,
    EDIT_COMPLETED_POMODOROS,
    EDIT_INTERRUPTED_POMODOROS,
    // IDLE_TIMEOUT_MINUTES,
    IDLE_TIMEOUT_BATTERY,        // NEW
    IDLE_TIMEOUT_USB,            // NEW
    IDLE_SLEEP_ON_USB,           // NEW - toggle to disable sleep on USB
    BRIGHTNESS,
    ENABLE_WINDUP,       // Toggle wind-up mode    
    ALARM_DURATION,      // Alarm length in seconds
    ALARM_VIBRATION,     // Toggle vibration
    ALARM_FLASH,         // Toggle screen flash
    MENU_ITEM_COUNT      // Keep this last for iteration
};

class TimerCore {
private:
    // Menu
    MenuState menuState;
    MenuItem currentMenuItem;
    uint8_t editingValue;  // Temporary value while editing

    // Timer state variables
    TimerState state;
    TimerState previousState;
    uint32_t startTime;
    uint32_t pausedTime;
    uint32_t remainingTime;
    uint32_t duration;
    uint32_t idleStartTime;

    // Timer settings
    uint8_t workDuration;
    uint8_t shortBreakDuration;
    uint8_t longBreakDuration;
    uint8_t completedSessions;
    uint8_t pomodorosBeforeLongBreak;
    uint8_t pomodorosSinceLastLongBreak;
    
    // Power settings
    // uint8_t idleTimeoutDuration;
    uint8_t idleTimeoutBattery;
    uint8_t idleTimeoutUSB;
    bool sleepOnUSB;  // If false, never sleep when on USB power
    uint8_t brightnessLevel;

    // Alarm settings (NEW)
    uint8_t alarmDuration;        // in seconds
    bool alarmVibrationEnabled;
    bool alarmFlashEnabled;

    // Alert handling
    uint32_t alertStartTime;
    bool alertActive;
    uint8_t blinkCount;
    

    // Task tracking
    uint8_t currentTaskId;
    uint8_t totalTasks;
    uint8_t completedPomodoros[MAX_TASKS];
    uint8_t interruptedPomodoros[MAX_TASKS];

    // EEPROM functions
    void saveState();
    void loadState();

    // Wind-up feature
    bool windupEnabled;
    uint32_t windupValue;  // Current wound-up seconds (0 to workDuration*60)

public:
    TimerCore();
    void openMenu();
    void closeMenu();
    void navigateMenu(int8_t direction);  // +1 or -1
    void selectMenuItem();
    void adjustValue(int8_t direction);
    void confirmValue();
    MenuState getMenuState() const { return menuState; }
    MenuItem getCurrentMenuItem() const { return currentMenuItem; }
    uint8_t getEditingValue() const { return editingValue; }

    void resetSaveState();  // function to reset EEPROM

    // Timer controls
    void startWork();
    void startBreak();
    void pause();
    void resume();
    void reset();
    void update();
    void interrupt();  // New: handle interruptions
    bool checkIdleTimeout(float batteryVoltage);  // Returns true if we should sleep
    void resetIdleTimer() { idleStartTime = millis(); }

    // Task management
    void addTask();
    void selectTask(uint8_t taskId);
    void resetTaskStats();
    
    // Alert methods
    void startAlert();
    void updateAlert();
    bool isAlertActive() const { return alertActive; }
    uint8_t getBlinkCount() const { return blinkCount; }
    

    // Long Break
    uint8_t getPomodorosSinceLastLongBreak() const { return pomodorosSinceLastLongBreak; }

    // Wind-up controls
    void startWindup();
    void cancelWindup();
    void incrementWindup(int8_t direction);
    void startWorkFromWindup();  // Start work with current windup value
    
    // Wind-up getters
    bool getWindupEnabled() const { return windupEnabled; }
    uint32_t getWindupValue() const { return windupValue; }
    uint32_t getWindupPercentage() const;
    
    // Wind-up setters
    void setWindupEnabled(bool enabled);

    // deepsleep
    
    bool isOnUSBPower(float batteryVoltage) const;

    // Setters
    void setPomodorosBeforeLongBreak(uint8_t count);  
    void setWorkDuration(uint8_t minutes);
    void setShortBreakDuration(uint8_t minutes);
    void setLongBreakDuration(uint8_t minutes);
    // void setIdleTimeoutDuration(uint8_t minutes); 
    void setIdleTimeoutBattery(uint8_t minutes);
    void setIdleTimeoutUSB(uint8_t minutes);
    void setSleepOnUSB(bool enabled);
    void setBrightnessLevel(uint8_t level);
    void setTotalTasks(uint8_t count);
    void setTaskCompletedPomodoros(uint8_t taskId, uint8_t count);
    void setTaskInterruptedPomodoros(uint8_t taskId, uint8_t count);
    void setAlarmDuration(uint8_t seconds);           // NEW
    void setAlarmVibration(bool enabled);             // NEW
    void setAlarmFlash(bool enabled);                 // NEW

    // Getters
    TimerState getState() const { return state; }
    uint32_t getRemainingSeconds() const { return remainingTime; }
    uint32_t getRemainingMinutes() const { return remainingTime / 60; }
    uint32_t getRemainingSecondsInMinute() const { return remainingTime % 60; }
    uint8_t getCompletedSessions() const { return completedSessions; }
    uint8_t getCurrentTaskId() const { return currentTaskId; }
    uint8_t getTotalTasks() const { return totalTasks; }
    uint8_t getTaskCompletedPomodoros(uint8_t taskId) const { return completedPomodoros[taskId]; }
    uint8_t getTaskInterruptedPomodoros(uint8_t taskId) const { return interruptedPomodoros[taskId]; }
    bool isWorkPeriod() const { return state == TimerState::WORK || state == TimerState::PAUSED_WORK; }
    bool isBreakPeriod() const { 
        return state == TimerState::SHORT_BREAK || state == TimerState::LONG_BREAK || 
               state == TimerState::PAUSED_SHORT_BREAK || state == TimerState::PAUSED_LONG_BREAK; 
    }
    uint8_t getWorkDuration() const { return workDuration; }
    uint8_t getShortBreakDuration() const { return shortBreakDuration; }
    uint8_t getLongBreakDuration() const { return longBreakDuration; }
    // uint8_t getIdleTImeoutDuration() const { return idleTimeoutDuration; }
    bool getSleepOnUSB() const { return sleepOnUSB; }
    uint8_t getIdleTimeoutBattery() const { return idleTimeoutBattery; }
    uint8_t getIdleTimeoutUSB() const { return idleTimeoutUSB; }
    uint8_t getPomodorosBeforeLongBreak() const { return pomodorosBeforeLongBreak; }
    uint8_t getBrightnessLevel() const { return brightnessLevel; }
    uint8_t getAlarmDuration() const { return alarmDuration; }           
    bool getAlarmVibration() const { return alarmVibrationEnabled; }     
    bool getAlarmFlash() const { return alarmFlashEnabled; }             
    uint32_t getIdleStartTime() const { return idleStartTime; }


};