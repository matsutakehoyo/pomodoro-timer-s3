
#include "timer_core.h"
#include <Preferences.h>

TimerCore::TimerCore()
  : state(TimerState::IDLE),
    previousState(TimerState::IDLE),
    startTime(0),
    pausedTime(0),
    remainingTime(0),
    duration(0),
    workDuration(WORK_DURATION),
    shortBreakDuration(SHORT_BREAK_DURATION),
    longBreakDuration(LONG_BREAK_DURATION),
    completedSessions(0),
    totalTasks(1),
    currentTaskId(0),
    pomodorosBeforeLongBreak(POMODOROS_BEFORE_LONG_BREAK),
    pomodorosSinceLastLongBreak(0),
    idleTimeoutBattery(IDLE_TIMEOUT_BATTERY_MINUTES),
    idleTimeoutUSB(IDLE_TIMEOUT_USB_MINUTES),
    sleepOnUSB(true),
    idleStartTime(millis()),
    brightnessLevel(4),
    themeId(1),
    alarmDuration(DEFAULT_ALARM_DURATION),
    alarmVibrationEnabled(true),
    alarmFlashEnabled(true),
    alertStartTime(0),
    alertActive(false),
    blinkCount(0),
    windupEnabled(false),
    windupValue(0),
    menuState(MenuState::CLOSED),
    currentMenuItem(MenuItem::POMODORO_LENGTH),
    editingValue(0)
{


  // Initialize arrays
  for (int i = 0; i < MAX_TASKS; i++) {
    completedPomodoros[i] = 0;
    interruptedPomodoros[i] = 0;
  }

  Serial.println("TimerCore initializing...");
  loadState();  // Load saved state
  Serial.println("TimerCore initialization complete");
}




// For menu options
void TimerCore::setWorkDuration(uint8_t minutes) { 
    workDuration = minutes; 
    saveState();
}

void TimerCore::setShortBreakDuration(uint8_t minutes) { 
    shortBreakDuration = minutes; 
    saveState();
}

void TimerCore::setLongBreakDuration(uint8_t minutes) { 
    longBreakDuration = minutes; 
    saveState();
}

void TimerCore::setPomodorosBeforeLongBreak(uint8_t count) { 
    pomodorosBeforeLongBreak = count; 
    saveState();
}

// Power Setting
// void TimerCore::setIdleTimeoutDuration(uint8_t minutes) { 
//     idleTimeoutDuration = minutes; 
//     saveState();
// }
void TimerCore::setIdleTimeoutBattery(uint8_t minutes) { 
    idleTimeoutBattery = minutes; 
    saveState();
}

void TimerCore::setIdleTimeoutUSB(uint8_t minutes) { 
    idleTimeoutUSB = minutes; 
    saveState();
}

void TimerCore::setSleepOnUSB(bool enabled) {
    sleepOnUSB = enabled;
    saveState();
}



void TimerCore::setBrightnessLevel(uint8_t level) { 
    brightnessLevel = level; 
    saveState();
}

void TimerCore::setTheme(uint8_t theme) {
    themeId = theme;
    saveState();
}

// Windup toggle
void TimerCore::setWindupEnabled(bool enabled) {
    windupEnabled = enabled;
    saveState();
}

// wind-up method implementations:
void TimerCore::startWindup() {
   if (!windupEnabled) {
      startWork();  // If windup disabled, start work directly
      return;
   }
    
   state = TimerState::WIND_UP;
   windupValue = 0;
   Serial.println("Wind-up started - windupValue reset to 0");
}


void TimerCore::cancelWindup() {
    if (state == TimerState::WIND_UP) {
        state = TimerState::IDLE;
        windupValue = 0;
        Serial.println("Wind-up cancelled");
    }
}

void TimerCore::incrementWindup(int8_t direction) {
    if (state != TimerState::WIND_UP) return;
    
    uint32_t maxSeconds = workDuration * 60;
    // Increment by 1 minute (60 seconds) per encoder tick
    
    int32_t newValue = windupValue + (direction * 60);
    
    // Clamp to 0 and max
    if (newValue < 0) newValue = 0;
    if (newValue > maxSeconds) newValue = maxSeconds;
    
    windupValue = newValue;
    
    // Auto-start when fully wound up
    if (windupValue >= maxSeconds) {
        Serial.println("Fully wound up - auto-starting work");
        startWorkFromWindup();
    }
}

void TimerCore::startWorkFromWindup() {
    if (state != TimerState::WIND_UP) return;
    
    // Start work with the wound-up duration
    duration = windupValue;
    remainingTime = duration;
    startTime = millis();
    state = TimerState::WORK;
    windupValue = 0;  // Reset for next time
    
    Serial.printf("Work started from wind-up - Duration: %lu seconds\n", duration);
}

uint32_t TimerCore::getWindupPercentage() const {
    if (workDuration == 0) return 0;
    uint32_t maxSeconds = workDuration * 60;
    if (maxSeconds == 0) return 0;
    return (windupValue * 100) / maxSeconds;
}


// Alarm settings
void TimerCore::setAlarmDuration(uint8_t seconds) {
    alarmDuration = seconds;
    saveState();
}

void TimerCore::setAlarmVibration(bool enabled) {
    alarmVibrationEnabled = enabled;
    saveState();
}

void TimerCore::setAlarmFlash(bool enabled) {
    alarmFlashEnabled = enabled;
    saveState();
}

void TimerCore::setTotalTasks(uint8_t count) {
    totalTasks = count;
    // Make sure current task is valid
    if (currentTaskId >= totalTasks) {
        currentTaskId = totalTasks - 1;
    }
    saveState();
}

void TimerCore::setTaskCompletedPomodoros(uint8_t taskId, uint8_t count) {
    if (taskId < MAX_TASKS) {
        completedPomodoros[taskId] = count;
        saveState();
    }
}

void TimerCore::setTaskInterruptedPomodoros(uint8_t taskId, uint8_t count) {
    if (taskId < MAX_TASKS) {
        interruptedPomodoros[taskId] = count;
        saveState();
    }
}

void TimerCore::openMenu() {
    if (state == TimerState::IDLE) {
        menuState = MenuState::MENU_LIST;
        currentMenuItem = MenuItem::POMODORO_LENGTH;
        Serial.println("Menu opened");
    }
}

void TimerCore::closeMenu() {
    menuState = MenuState::CLOSED;
    Serial.println("Menu closed");
}

void TimerCore::navigateMenu(int8_t direction) {
    if (menuState != MenuState::MENU_LIST) return;
    
    int8_t newItem = static_cast<int8_t>(currentMenuItem) + direction;
    
    // Wrap around
    if (newItem < 0) {
        newItem = static_cast<int8_t>(MenuItem::MENU_ITEM_COUNT) - 1;
    } else if (newItem >= static_cast<int8_t>(MenuItem::MENU_ITEM_COUNT)) {
        newItem = 0;
    }
    
    currentMenuItem = static_cast<MenuItem>(newItem);
    Serial.printf("Menu item: %d\n", newItem);
}

void TimerCore::selectMenuItem() {
    if (menuState == MenuState::MENU_LIST) {
        menuState = MenuState::EDITING_VALUE;
        
        // Load current value based on menu item
        switch (currentMenuItem) {
            case MenuItem::POMODORO_LENGTH:
                editingValue = workDuration;
                break;
            case MenuItem::SHORT_BREAK_LENGTH:
                editingValue = shortBreakDuration;
                break;
            case MenuItem::LONG_BREAK_LENGTH:
                editingValue = longBreakDuration;
                break;
            case MenuItem::POMODOROS_BEFORE_LONG_BREAK:
                editingValue = pomodorosBeforeLongBreak;
                break;
            case MenuItem::MANAGE_TASKS:
                editingValue = totalTasks;
                break;
            case MenuItem::EDIT_COMPLETED_POMODOROS:
                editingValue = completedPomodoros[currentTaskId];
                break;
            case MenuItem::EDIT_INTERRUPTED_POMODOROS:
                editingValue = interruptedPomodoros[currentTaskId];
                break;                
            // case MenuItem::IDLE_TIMEOUT_MINUTES:
            //     editingValue = idleTimeoutDuration;
            //     break;
            case MenuItem::IDLE_TIMEOUT_BATTERY:
                editingValue = idleTimeoutBattery;
                break;
            case MenuItem::IDLE_TIMEOUT_USB:
                editingValue = idleTimeoutUSB;
                break;
            case MenuItem::IDLE_SLEEP_ON_USB:
                editingValue = sleepOnUSB ? 1 : 0;
                break;

            case MenuItem::BRIGHTNESS:
                editingValue = brightnessLevel;
                break;
            case MenuItem::THEME:
                editingValue = themeId;
                break;
            case MenuItem::ENABLE_WINDUP:
                editingValue = windupEnabled ? 1 : 0;
                break;                
            case MenuItem::ALARM_DURATION:
                editingValue = alarmDuration;
                break;
            case MenuItem::ALARM_VIBRATION:
                editingValue = alarmVibrationEnabled ? 1 : 0;
                break;
            case MenuItem::ALARM_FLASH:
                editingValue = alarmFlashEnabled ? 1 : 0;
                break;
            default:
                editingValue = 0;
        }
        Serial.printf("Editing value: %d\n", editingValue);
    }
}

void TimerCore::adjustValue(int8_t direction) {
    if (menuState != MenuState::EDITING_VALUE) return;
    
   switch (currentMenuItem) {
      case MenuItem::POMODORO_LENGTH:
         editingValue += direction;
         if (editingValue < 1) editingValue = 1;
         if (editingValue > 60) editingValue = 60;
         break;
      case MenuItem::SHORT_BREAK_LENGTH:
         editingValue += direction;
         if (editingValue < 1) editingValue = 1;
         if (editingValue > 15) editingValue = 15;
         break;            
      case MenuItem::LONG_BREAK_LENGTH:
         editingValue += direction;
         if (editingValue < 5) editingValue = 5;
         if (editingValue > 30) editingValue = 30;
         break;            
      case MenuItem::POMODOROS_BEFORE_LONG_BREAK:
         editingValue += direction;
         if (editingValue < 2) editingValue = 2;
         if (editingValue > 10) editingValue = 10;
         break;
      case MenuItem::MANAGE_TASKS:
         {
             int16_t newValue = editingValue + direction;
             if (newValue < 1) newValue = 1;
             if (newValue > MAX_TASKS) newValue = MAX_TASKS;
             editingValue = newValue;
         }
         break;
      case MenuItem::EDIT_COMPLETED_POMODOROS:
      case MenuItem::EDIT_INTERRUPTED_POMODOROS:
         {
             int16_t newValue = editingValue + direction;
             if (newValue < 0) newValue = 0;
             if (newValue > 99) newValue = 99;
             editingValue = newValue;
         }
         break;            
      // case MenuItem::IDLE_TIMEOUT_MINUTES:
      //    editingValue += direction;
      //    if (editingValue < 1) editingValue = 1;
      //    if (editingValue > 30) editingValue = 30;
      //    break;
        case MenuItem::IDLE_TIMEOUT_BATTERY:
            editingValue += direction;
            if (editingValue < 1) editingValue = 1;
            if (editingValue > 30) editingValue = 30;
            break;
        case MenuItem::IDLE_TIMEOUT_USB:
            editingValue += direction;
            if (editingValue < 1) editingValue = 1;
            if (editingValue > 60) editingValue = 60;  // Longer max for USB
            break;
        case MenuItem::IDLE_SLEEP_ON_USB:
            editingValue = editingValue ? 0 : 1;
            break;         
      case MenuItem::BRIGHTNESS:
         if (direction < 0 && editingValue == 0) {
             editingValue = 0;
         } else if (direction > 0 && editingValue == 7) {
             editingValue = 7;
         } else {
             editingValue += direction;
         }
         break;
      case MenuItem::THEME:
         editingValue += direction;
         if (editingValue < 1) editingValue = 1;
         if (editingValue > 2) editingValue = 2;
         break;
      case MenuItem::ALARM_DURATION:
         editingValue += direction;
         if (editingValue < 1) editingValue = 1;
         if (editingValue > 10) editingValue = 10;
         break;
      case MenuItem::ENABLE_WINDUP:
      case MenuItem::ALARM_VIBRATION:
      case MenuItem::ALARM_FLASH:
         // Toggle between 0 and 1
         editingValue = editingValue ? 0 : 1;
         break;
      default:
         break;
    }
    Serial.printf("Adjusted value: %d\n", editingValue);
}

void TimerCore::confirmValue() {
   if (menuState != MenuState::EDITING_VALUE) return;
    
   // Apply the value
   switch (currentMenuItem) {
      case MenuItem::POMODORO_LENGTH:
         setWorkDuration(editingValue);
         Serial.printf("Pomodoro length set to: %d min\n", editingValue);
         break;
      case MenuItem::SHORT_BREAK_LENGTH:
         setShortBreakDuration(editingValue);
         Serial.printf("Short Break length set to: %d min\n", editingValue);
         break; 
      case MenuItem::LONG_BREAK_LENGTH:
         setLongBreakDuration(editingValue);
         Serial.printf("Long Break length set to: %d min\n", editingValue);
         break; 
      case MenuItem::POMODOROS_BEFORE_LONG_BREAK:
         setPomodorosBeforeLongBreak(editingValue);
         Serial.printf("n Pomodoros Before long break set to: %d pomodoros\n", editingValue);
         break;
      case MenuItem::MANAGE_TASKS:
         setTotalTasks(editingValue);
         Serial.printf("Total tasks set to: %d\n", editingValue);
         break;
      case MenuItem::EDIT_COMPLETED_POMODOROS:
         setTaskCompletedPomodoros(currentTaskId, editingValue);
         Serial.printf("Task %d completed pomodoros set to: %d\n", currentTaskId + 1, editingValue);
         break;
      case MenuItem::EDIT_INTERRUPTED_POMODOROS:
         setTaskInterruptedPomodoros(currentTaskId, editingValue);
         Serial.printf("Task %d interrupted pomodoros set to: %d\n", currentTaskId + 1, editingValue);
         break;            
      // case MenuItem::IDLE_TIMEOUT_MINUTES:
      //    setIdleTimeoutDuration(editingValue);
      //    Serial.printf("Idle timeout time set to: %d min\n", editingValue);
      //    break;
        case MenuItem::IDLE_TIMEOUT_BATTERY:
            setIdleTimeoutBattery(editingValue);
            Serial.printf("Battery idle timeout set to: %d min\n", editingValue);
            break;
        case MenuItem::IDLE_TIMEOUT_USB:
            setIdleTimeoutUSB(editingValue);
            Serial.printf("USB idle timeout set to: %d min\n", editingValue);
            break;
        case MenuItem::IDLE_SLEEP_ON_USB:
            setSleepOnUSB(editingValue != 0);
            Serial.printf("Sleep on USB set to: %s\n", editingValue ? "ON" : "OFF");
            break;
      case MenuItem::BRIGHTNESS:
         setBrightnessLevel(editingValue);
         Serial.printf("Brightness level set to: %d\n", editingValue);
         break;
      case MenuItem::THEME:
         setTheme(editingValue);
         Serial.printf("Theme set to: %d\n", editingValue);
         break;
      case MenuItem::ENABLE_WINDUP:
            setWindupEnabled(editingValue != 0);
            Serial.printf("Wind-up mode set to: %s\n", editingValue ? "ON" : "OFF");
            break;
      case MenuItem::ALARM_DURATION:
         setAlarmDuration(editingValue);
         Serial.printf("Alarm duration set to: %d sec\n", editingValue);
         break;
      case MenuItem::ALARM_VIBRATION:
         setAlarmVibration(editingValue != 0);
         Serial.printf("Alarm vibration set to: %s\n", editingValue ? "ON" : "OFF");
         break;
      case MenuItem::ALARM_FLASH:
         setAlarmFlash(editingValue != 0);
         Serial.printf("Alarm flash set to: %s\n", editingValue ? "ON" : "OFF");
         break;
      default:
         break;
    }
    
    // Return to menu list
    menuState = MenuState::MENU_LIST;
}

// USB power detection method:
bool TimerCore::isOnUSBPower(float batteryVoltage) const {
    // When USB is connected, voltage reads very high (>5.0V) or we're externally powered
    return (batteryVoltage > 5.0);
}



bool TimerCore::checkIdleTimeout(float batteryVoltage) {
    if (state != TimerState::IDLE) {
        resetIdleTimer();
        return false;
    }
    
    // Check if we're on USB and sleep is disabled
    bool onUSB = isOnUSBPower(batteryVoltage);
    if (onUSB && !sleepOnUSB) {
        return false;  // Never sleep on USB if disabled
    }
    
    // Use appropriate timeout based on power source
    uint8_t timeout_minutes = onUSB ? idleTimeoutUSB : idleTimeoutBattery;
    uint32_t timeout_ms = timeout_minutes * 60 * 1000;
    uint32_t elapsed_ms = millis() - idleStartTime;
    
    static uint32_t lastDebug = 0;
    if (millis() - lastDebug > 10000) {
        Serial.printf("Idle timeout: %d/%d seconds (set to %d min, %s power, %.2fV)\n", 
                      elapsed_ms/1000, timeout_ms/1000, timeout_minutes,
                      onUSB ? "USB" : "Battery", batteryVoltage);
        lastDebug = millis();
    }
    
    return elapsed_ms >= timeout_ms;
}


void TimerCore::loadState() {
  Serial.println("Loading state from Preferences...");
  
  Preferences prefs;
  prefs.begin("pomodoro", true);
  
  totalTasks = prefs.getUChar("totalTasks", 1);
  currentTaskId = prefs.getUChar("currentTask", 0);
  workDuration = prefs.getUChar("workDuration", WORK_DURATION);
  shortBreakDuration = prefs.getUChar("shortBreak", SHORT_BREAK_DURATION);
  longBreakDuration = prefs.getUChar("longBreak", LONG_BREAK_DURATION);
  pomodorosBeforeLongBreak = prefs.getUChar("pomosB4Long", POMODOROS_BEFORE_LONG_BREAK);
  // idleTimeoutDuration = prefs.getUChar("idleTimeout", IDLE_TIMEOUT_MINUTES);
    idleTimeoutBattery = prefs.getUChar("idleTimeout", IDLE_TIMEOUT_BATTERY_MINUTES);  // Keep old key for compatibility
    idleTimeoutUSB = prefs.getUChar("idleTimeUSB", IDLE_TIMEOUT_USB_MINUTES);
    sleepOnUSB = prefs.getBool("sleepOnUSB", true);

  brightnessLevel = prefs.getUChar("brightness", 4);
  themeId = prefs.getUChar("theme", 1);
  windupEnabled = prefs.getBool("windupEn", false);
  // Load alarm settings
  alarmDuration = prefs.getUChar("alarmDur", DEFAULT_ALARM_DURATION);
  alarmVibrationEnabled = prefs.getBool("alarmVib", true);
  alarmFlashEnabled = prefs.getBool("alarmFlash", true);




  Serial.printf("Loaded: tasks=%d, task=%d, work=%d, short=%d, long=%d, bright=%d, windup=%d\n", 
                totalTasks, currentTaskId, workDuration, shortBreakDuration, 
                longBreakDuration, brightnessLevel, windupEnabled);

  Serial.printf("Alarm: dur=%d, vib=%d, flash=%d\n", 
                alarmDuration, alarmVibrationEnabled, alarmFlashEnabled);



  // Load pomodoro counts
  for (int i = 0; i < MAX_TASKS; i++) {
    String compKey = "comp" + String(i);
    String intKey = "int" + String(i);
    
    completedPomodoros[i] = prefs.getUChar(compKey.c_str(), 0);
    interruptedPomodoros[i] = prefs.getUChar(intKey.c_str(), 0);
    
    if (completedPomodoros[i] > 0 || interruptedPomodoros[i] > 0) {
      Serial.printf("Task %d: completed=%d, interrupted=%d\n",
                    i, completedPomodoros[i], interruptedPomodoros[i]);
    }
  }
  
  prefs.end();
  Serial.println("Preferences loading complete");
}

void TimerCore::saveState() {
   Serial.println("Saving state to Preferences...");
   
   Preferences prefs;
   prefs.begin("pomodoro", false);
   
   prefs.putUChar("totalTasks", totalTasks);
   prefs.putUChar("currentTask", currentTaskId);
   prefs.putUChar("workDuration", workDuration);
   prefs.putUChar("shortBreak", shortBreakDuration);
   prefs.putUChar("longBreak", longBreakDuration);
   prefs.putUChar("pomosB4Long", pomodorosBeforeLongBreak);
   // prefs.putUChar("idleTimeout", idleTimeoutDuration);
    prefs.putUChar("idleTimeout", idleTimeoutBattery);  // Keep old key name
    prefs.putUChar("idleTimeUSB", idleTimeoutUSB);
    prefs.putBool("sleepOnUSB", sleepOnUSB);   
   prefs.putUChar("brightness", brightnessLevel);
   prefs.putUChar("theme", themeId);
   prefs.putBool("windupEn", windupEnabled);  
   // NEW: Save alarm settings
   prefs.putUChar("alarmDur", alarmDuration);
   prefs.putBool("alarmVib", alarmVibrationEnabled);
   prefs.putBool("alarmFlash", alarmFlashEnabled);
   

    
    Serial.printf("Saving: idle battery=%d, idle USB=%d, sleep on USB=%d\n",
                  idleTimeoutBattery, idleTimeoutUSB, sleepOnUSB);

   Serial.printf("Saving: work=%d, short=%d, long=%d, pomodoros=%d, brightness=%d\n",
                workDuration, shortBreakDuration, longBreakDuration, 
                pomodorosBeforeLongBreak, brightnessLevel);
   Serial.printf("Alarm: dur=%d, vib=%d, flash=%d\n", 
                alarmDuration, alarmVibrationEnabled, alarmFlashEnabled);
   
   // Save pomodoro counts
   for (int i = 0; i < MAX_TASKS; i++) {
      String compKey = "comp" + String(i);
      String intKey = "int" + String(i);
      
      prefs.putUChar(compKey.c_str(), completedPomodoros[i]);
      prefs.putUChar(intKey.c_str(), interruptedPomodoros[i]);
  }
  
  prefs.end();
  Serial.println("Preferences save complete");
}

void TimerCore::resetSaveState() {
  totalTasks = 1;
  currentTaskId = 0;
  completedSessions = 0;
  pomodorosSinceLastLongBreak = 0;
  
  for (int i = 0; i < MAX_TASKS; i++) {
    completedPomodoros[i] = 0;
    interruptedPomodoros[i] = 0;
  }

  saveState();
}

void TimerCore::startBreak() {
  pomodorosSinceLastLongBreak++;
  if (pomodorosSinceLastLongBreak >= pomodorosBeforeLongBreak) {
    duration = longBreakDuration * 60;
    state = TimerState::LONG_BREAK;
    pomodorosSinceLastLongBreak = 0;
    Serial.println("Starting LONG break");
  } else {
    duration = shortBreakDuration * 60;
    state = TimerState::SHORT_BREAK;
    Serial.println("Starting short break");
  }
  remainingTime = duration;
  startTime = millis();
}

void TimerCore::startWork() {
  duration = workDuration * 60;
  remainingTime = duration;
  startTime = millis();
  state = TimerState::WORK;
  Serial.printf("Starting work session - Task %d\n", currentTaskId);
}

void TimerCore::pause() {
  if (state == TimerState::WORK) {
    pausedTime = millis();
    state = TimerState::PAUSED_WORK;
  } else if (state == TimerState::SHORT_BREAK) {
    pausedTime = millis();
    state = TimerState::PAUSED_SHORT_BREAK;
  } else if (state == TimerState::LONG_BREAK) {
    pausedTime = millis();
    state = TimerState::PAUSED_LONG_BREAK;
  }
}

void TimerCore::resume() {
  if (state == TimerState::PAUSED_WORK) {
    startTime += (millis() - pausedTime);
    state = TimerState::WORK;
  } else if (state == TimerState::PAUSED_SHORT_BREAK) {
    startTime += (millis() - pausedTime);
    state = TimerState::SHORT_BREAK;
  } else if (state == TimerState::PAUSED_LONG_BREAK) {
    startTime += (millis() - pausedTime);
    state = TimerState::LONG_BREAK;
  }
}

void TimerCore::interrupt() {
  if (state == TimerState::WORK || state == TimerState::PAUSED_WORK) {
    interruptedPomodoros[currentTaskId]++;
    saveState();
    state = TimerState::IDLE;
    remainingTime = 0;
  }
}

void TimerCore::reset() {
  state = TimerState::IDLE;
  remainingTime = 0;
  startTime = 0;
  pausedTime = 0;
}

void TimerCore::resetTaskStats() {
  completedSessions = 0;
  for (int i = 0; i < MAX_TASKS; i++) {
    completedPomodoros[i] = 0;
    interruptedPomodoros[i] = 0;
  }
}

void TimerCore::addTask() {
  if (totalTasks < MAX_TASKS) {
    totalTasks++;
    currentTaskId = totalTasks - 1;
    saveState();
  }
}

void TimerCore::selectTask(uint8_t taskId) {
  if (taskId < totalTasks) {
    currentTaskId = taskId;
  }
}

void TimerCore::startAlert() {
    Serial.println("ALERT STARTED");
    
    previousState = state;
    state = TimerState::ALERT;
    alertStartTime = millis();
    alertActive = true;
    blinkCount = 0;
    
    Serial.println("Alert state initialized");
}

void TimerCore::updateAlert() {
    if (!alertActive) return;

    uint32_t elapsed = millis() - alertStartTime;
    
    // Use the configurable alarm duration (convert seconds to milliseconds)
    uint32_t alert_duration = alarmDuration * 1000;
    
    // Update blink state
    uint32_t blink_interval = 400;
    blinkCount = (elapsed / blink_interval) % 2;
    
    if (elapsed >= alert_duration) {
        alertActive = false;
        Serial.println("Alert finished - processing completion");

        if (previousState == TimerState::WORK) {
            // Update stats
            if (currentTaskId < MAX_TASKS) {
                completedPomodoros[currentTaskId]++;
                completedSessions++;
                saveState();
            }

            // Start appropriate break
            startBreak();
        } else {
            state = TimerState::IDLE;
            remainingTime = 0;
        }
        
        // if (previousState == TimerState::WORK) {
        //     if (currentTaskId < MAX_TASKS) {
        //         completedPomodoros[currentTaskId]++;
        //         completedSessions++;
        //         Serial.printf("Work completed - Task %d: %d pomodoros, Total sessions: %d\n",
        //                     currentTaskId, completedPomodoros[currentTaskId], completedSessions);
        //     }
            
        //     Serial.println("Saving state...");
        //     saveState();
        //     Serial.println("State saved");
            
        //     pomodorosSinceLastLongBreak++;
        //     if (pomodorosSinceLastLongBreak >= pomodorosBeforeLongBreak) {
        //         duration = longBreakDuration * 60;
        //         state = TimerState::LONG_BREAK;
        //         pomodorosSinceLastLongBreak = 0;
        //         Serial.println("Starting LONG break");
        //     } else {
        //         duration = shortBreakDuration * 60;
        //         state = TimerState::SHORT_BREAK;
        //         Serial.println("Starting short break");
        //     }
        //     remainingTime = duration;
        //     startTime = millis();
            
        // } else {
        //     state = TimerState::IDLE;
        //     remainingTime = 0;
        //     Serial.println("Break finished - returning to IDLE");
        // }
        return;
    }
}

void TimerCore::update() {
    if (alertActive) {
        updateAlert();
        return;
    }

    if (state == TimerState::WORK || 
        state == TimerState::SHORT_BREAK || 
        state == TimerState::LONG_BREAK) {
        
        uint32_t currentTime = millis();
        uint32_t elapsed_ms;
        
        if (currentTime >= startTime) {
            elapsed_ms = currentTime - startTime;
        } else {
            elapsed_ms = (0xFFFFFFFF - startTime) + currentTime + 1;
        }
        
        uint32_t elapsed_seconds = elapsed_ms / 1000;
        uint32_t duration_ms = duration * 1000UL;
        
        if (elapsed_ms >= duration_ms || elapsed_seconds >= duration) {
            remainingTime = 0;
            startAlert();
        } else {
            remainingTime = (elapsed_seconds < duration) ? (duration - elapsed_seconds) : 0;
        }
    }
}
