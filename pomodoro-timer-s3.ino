/*
 * LILYGO T-DISPLAY S3 POMODORO TIMER
 * Optimized for EC11 Quadrature Encoder
 * 
 * WIRING:
 * EC11 Pin A (CLK) → GPIO 21
 * EC11 Pin B (DT)  → GPIO 18
 * EC11 Common      → GND
 * EC11 Switch 1    → GPIO 16
 * EC11 Switch 2    → GND
 * 
 * Button 1         → GPIO 0
 * Button 2         → GPIO 14
 */

#include <TFT_eSPI.h>
#include <Button2.h>
#include <RotaryEncoder.h>
#include "lvgl.h"
#include "timer_core.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"

LV_FONT_DECLARE(pomodoro_symbols);

#include "background1_theme.h"
#include "pomodoro1_theme.h"
#include "flower1_theme.h"
#include "bud1_theme.h"
#include "background2_theme.h"
#include "pomodoro2_theme.h"
#include "flower2_theme.h"
#include "bud2_theme.h"

// Pin Definitions
#define PIN_BUTTON_1 0
#define PIN_BUTTON_2 14
#define PIN_LCD_BL 38
#define PIN_POWER_ON 15
#define PIN_VIBRATION 13
#define PIN_BAT_VOLT 4
#define LCD_BL_PWM_CHANNEL 1

// EC11 Encoder pins
#define PIN_ENC_A 18
#define PIN_ENC_B 21
#define PIN_ENC_BTN 16

// ============================================================================
// LAYOUT CONSTANTS
// ============================================================================
// Display dimensions
const int DISPLAY_WIDTH = 320;
const int DISPLAY_HEIGHT = 170;

// Container sizes
const int SIDEBAR_WIDTH = 85;
const int SIDEBAR_HEIGHT = 170;
const int MAIN_CONTAINER_WIDTH_NORMAL = 235;
const int MAIN_CONTAINER_WIDTH_FULL = 320;
const int MAIN_CONTAINER_HEIGHT = 170;

// Task list
const int TASK_LIST_WIDTH = 80;
const int TASK_LIST_HEIGHT = 170;
const int TASK_LIST_ITEM_HEIGHT = 20;
const int TASK_LIST_PAD = 5;
const int TASK_NUMBER_OFFSET_SINGLE = 15;  // For tasks 1-9
const int TASK_NUMBER_OFFSET_DOUBLE = 18;  // For tasks 10+


// Tick configuration - CHANGE THESE TO CUSTOMIZE
const uint8_t TICK_INTERVAL_MINUTES = 1;     // 5 = one tick every 5 minutes
const int TICK_INNER_RADIUS = 62;            // Distance from center to tick start
const int TICK_OUTER_RADIUS = 66;            // Distance from center to tick end  
const int TICK_MAJOR_WIDTH = 2;              // Width of major ticks (every 5)
const int TICK_MINOR_WIDTH = 1;              // Width of minor ticks
const uint32_t TICK_MAJOR_COLOR = 0x808080;  // Bright gray for major ticks
const uint32_t TICK_MINOR_COLOR = 0x505050;  // Dim gray for minor ticks


// Work display
const int WORK_ARC_SIZE = 160;
const int WORK_ARC_WIDTH = 12;
const int PERCENT_CONTAINER_WIDTH = 70;
const int PERCENT_CONTAINER_HEIGHT = 160;
const int TIME_CONTAINER_WIDTH = 70;
const int TIME_CONTAINER_HEIGHT = 160;
const int POMODORO_SYMBOL_CONTAINER_WIDTH = 60;
const int POMODORO_SYMBOL_CONTAINER_HEIGHT = 80;

// Progress indicators
const int PROGRESS_POMO_SPACING = 22;  // 17px image + 5px gap
const int PROGRESS_POMO_Y_POS = 25;

// Menu
const int MENU_CONTAINER_WIDTH = 310;
const int MENU_CONTAINER_HEIGHT = 160;

// Timing constants
const uint32_t BATTERY_CHECK_INTERVAL_MS = 2000;
const uint32_t ENCODER_DEBOUNCE_MS = 50;
const uint32_t LVGL_TICK_MS = 20;
const uint32_t DISPLAY_UPDATE_MS = 20;

// Alert timing
const uint32_t ALERT_BLINK_INTERVAL_MS = 400;
const uint16_t WINDUP_START_VIBRATION_MS = 80;

// Colors
const uint32_t COLOR_BLACK = 0x000000;
const uint32_t COLOR_WHITE = 0xFFFFFF;
const uint32_t COLOR_SIDEBAR_BG = 0x1a1a1a;
const uint32_t COLOR_SIDEBAR_BG_ALERT = 0xB3B3B3;
const uint32_t COLOR_GRAY = 0x808080;
const uint32_t COLOR_DARK_GRAY = 0x303030;
const uint32_t COLOR_MENU_TITLE = 0x8D00FF;
const uint32_t COLOR_GREEN = 0x00FF00;
const uint32_t COLOR_GREEN_WORK = 0x00E676;
const uint32_t COLOR_ORANGE = 0xFFAA00;
const uint32_t COLOR_BLUE_SHORT = 0x00AAFF;
const uint32_t COLOR_BLUE_LONG = 0x0055FF;
const uint32_t COLOR_RED = 0xFF0000;
const uint32_t COLOR_YELLOW = 0xFFFF00;


// Symbols
#define SYMBOL_COMPLETED_POMODORO "\xEF\x84\x91"
#define SYMBOL_INTERRUPTED_POMODORO "\xef\x84\x8c"

// Brightness settings
const int BRIGHTNESS_VALUES[8] = {20, 40, 80, 120, 160, 200, 230, 255};

// ============================================================================
// ENCODER SETUP - OPTIMIZED FOR EC11
// ============================================================================
RotaryEncoder *encoder = nullptr;
static volatile bool encoder_changed = false;

// Interrupt handler - ONLY call tick(), nothing else
void IRAM_ATTR checkPosition() {
  encoder->tick();
  encoder_changed = true;
}

// ============================================================================
// Display Variables
// ============================================================================
#define LVGL_LCD_BUF_SIZE (320 * 40)
static lv_disp_draw_buf_t disp_buf;
static lv_color_t *lv_disp_buf;
static lv_disp_drv_t disp_drv;

// ============================================================================
// UI Objects
// ============================================================================
static lv_obj_t *main_container = nullptr;
static lv_obj_t *sidebar_container = nullptr;
static lv_obj_t *bg_img = nullptr;
static lv_obj_t *time_label = nullptr;
static lv_obj_t *state_label = nullptr;
static lv_obj_t *session_label = nullptr;
static lv_obj_t *battery_label = nullptr;

// Task list
static lv_obj_t *task_list = nullptr;
static lv_obj_t *task_numbers[MAX_TASKS] = {nullptr};
static lv_obj_t *task_symbols[MAX_TASKS] = {nullptr};
static lv_obj_t *task_counts[MAX_TASKS] = {nullptr};
static lv_obj_t *count_labels[MAX_TASKS] = {nullptr};
static lv_obj_t *interrupt_symbols[MAX_TASKS] = {nullptr};
static lv_obj_t *interrupt_counts[MAX_TASKS] = {nullptr};
static uint8_t visible_tasks = 0;

// Work timer
static lv_obj_t *work_arc = nullptr;
static lv_obj_t *work_percentage_label = nullptr;
static lv_obj_t *percent_symbol = nullptr;
static lv_obj_t *work_minutes_label = nullptr;
static lv_obj_t *work_seconds_label = nullptr;
static lv_obj_t *percent_container = nullptr;
static lv_obj_t *time_container = nullptr;
static lv_obj_t *status_label = nullptr;
static lv_obj_t *task_title_label = nullptr;
static lv_obj_t *task_num_label = nullptr;
static lv_obj_t *pomo_container = nullptr;
static lv_obj_t *starting_container = nullptr;
static lv_obj_t *starting_label = nullptr;
static lv_obj_t *starting_sub_label = nullptr;

// Progress indicators
static lv_obj_t *progress_pomodoros[10] = {nullptr};
static lv_obj_t *progress_container = nullptr;
static const uint8_t PROGRESS_POMO_SIZE = 20;

// Pomodoro images
static lv_obj_t *pomodoro_images[MAX_TASKS][8] = {{nullptr}};

static const lv_img_dsc_t *theme_background = &background_theme1;
static const lv_img_dsc_t *theme_pomodoro = &pomodoro_19_theme1;
static const lv_img_dsc_t *theme_flower = &flower_theme1;
static const lv_img_dsc_t *theme_bud = &bud_theme1;

// Arc tick marks
static lv_obj_t *arc_ticks[60] = {nullptr};  // Support up to 60 ticks
static lv_point_t tick_points[60][2];        // Each tick needs its own point array
static uint8_t current_tick_count = 0;

// idle time
static lv_obj_t *idle_info_label = nullptr;       // For idle time warning
static lv_obj_t *summary_container = nullptr;     // Container for summary display
static lv_obj_t *summary_today_label = nullptr;   // "Today:"
static lv_obj_t *summary_completed_num = nullptr; // "3" (number)
static lv_obj_t *summary_completed_sym = nullptr; // "⚫︎" (symbol)
static lv_obj_t *summary_interrupted_num = nullptr; // "0" (number)
static lv_obj_t *summary_interrupted_sym = nullptr; // "⚪︎" (symbol)
static lv_obj_t *summary_total_label = nullptr;   // "Total: 1h 15m"
static lv_obj_t *summary_pomo_label = nullptr;   
static lv_obj_t *summary_numbers_label = nullptr;
static lv_obj_t *summary_separator = nullptr;  // Add this

// battery power
static float current_battery_voltage = 0.0;


// ============================================================================
// Encoder State - Clean tracking
// ============================================================================
static long encoder_position = 0;
static unsigned long encoderButtonPressTime = 0;
static bool encoderButtonLongPressHandled = false;


// Initialize objects
TFT_eSPI tft = TFT_eSPI();
Button2 btn1(PIN_BUTTON_1);
Button2 btn2(PIN_BUTTON_2);
TimerCore timer;

// Forward declarations
void enter_deep_sleep();
void display_sleep_message();
void update_display();
void update_task_display();
void handle_button1_click();
void handle_button1_longpress();
void handle_button2_click();
void handle_button2_longpress();
void update_menu_display();
void update_battery_display();
void update_brightness();
void apply_theme_assets();
void apply_display_orientation();
void disable_scrolling(lv_obj_t *obj);
void set_alert_colors(bool inverted);
void update_cpu_frequency();

// Display flush callback
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

void display_sleep_message() {
  lv_obj_clean(lv_scr_act());
  lv_obj_t *sleep_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_font(sleep_label, &lv_font_montserrat_14, 0);
  lv_obj_align(sleep_label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(sleep_label, "Entering deep sleep...\nRelease button now");
  lv_refr_now(NULL);
}

void dim_backlight_for_sleep() {
  int8_t brightness_level = timer.getBrightnessLevel();
  if (timer.isOnUSBPower(current_battery_voltage) && brightness_level < 7) {
    brightness_level += 1;
  }

  for (int8_t level = brightness_level; level >= 0; level--) {
    ledcWrite(LCD_BL_PWM_CHANNEL, BRIGHTNESS_VALUES[level]);
    delay(30);
  }
  ledcWrite(LCD_BL_PWM_CHANNEL, 0);
}


void enter_deep_sleep() {
  dim_backlight_for_sleep();

  // Disable watchdog before sleep
  esp_task_wdt_delete(NULL);  // Remove current task from WDT
  esp_task_wdt_deinit();       // Deinitialize WDT completely

  tft.writecommand(0x28);
  delay(20);
  tft.writecommand(0x10);
  delay(120);
  
  digitalWrite(PIN_LCD_BL, LOW);
  
  gpio_config_t config = {
    .pin_bit_mask = (1ULL << GPIO_NUM_14),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_LOW_LEVEL
  };
  gpio_config(&config);

  uint64_t mask = (1ULL << GPIO_NUM_14);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ALL_LOW);
  
  digitalWrite(PIN_POWER_ON, LOW);
  Serial.flush();
  delay(100);
  
  esp_deep_sleep_start();
}

// Button handlers
void handle_button1_click() {
  Serial.println("\n--- Button 1 clicked ---");
  timer.resetIdleTimer();

  switch (timer.getState()) {
    case TimerState::IDLE:
      if (timer.getWindupEnabled()) {
        timer.startWindup();
      } else {
        timer.startWork();
      }
      break;
    case TimerState::WIND_UP:
      timer.startWorkFromWindup();
      break;
    case TimerState::STARTING:
      break;
    case TimerState::WORK:
      timer.interrupt();
      break;
    case TimerState::SHORT_BREAK:
    case TimerState::LONG_BREAK:
      timer.pause();
      break;
    case TimerState::PAUSED_WORK:
    case TimerState::PAUSED_SHORT_BREAK:
    case TimerState::PAUSED_LONG_BREAK:
      timer.resume();
      break;
  }
}

void handle_button1_longpress() {
  Serial.println("Long press detected - entering sleep");
  timer.resetIdleTimer();
  display_sleep_message();
  
  while (digitalRead(PIN_BUTTON_1) == LOW) {
    delay(50);
  }
  
  delay(500);
  enter_deep_sleep();
}

void handle_button2_click() {
  Serial.println("\n--- Button 2 clicked (Reset) ---");
  timer.resetIdleTimer();
  
  if (timer.getState() == TimerState::WIND_UP) {
    timer.cancelWindup();
    Serial.println("Reset complete");
    return;
  }
  
  timer.reset();
  Serial.println("Reset complete");
}

void handle_button2_longpress() {
  Serial.println("Long press Button 2 - Reset save state");
  timer.resetIdleTimer();
  timer.resetSaveState();

  if (task_list != nullptr) {
    lv_obj_clean(task_list);
    
    for (int i = 0; i < MAX_TASKS; i++) {
      task_numbers[i] = nullptr;
      task_symbols[i] = nullptr;
      task_counts[i] = nullptr;
      count_labels[i] = nullptr;
      interrupt_symbols[i] = nullptr;
      interrupt_counts[i] = nullptr;
    }
  }

  update_task_display();
}


void update_battery_display() {
  static uint32_t last_voltage_check = 0;
  
  if (millis() - last_voltage_check > BATTERY_CHECK_INTERVAL_MS) {
    current_battery_voltage = analogRead(PIN_BAT_VOLT) * 3.3 / 4095.0 * 2.0;
    last_voltage_check = millis();
    
    if (battery_label != nullptr) {
      char voltage_str[16];
      
      if (current_battery_voltage > 5.0) {
        snprintf(voltage_str, sizeof(voltage_str), "USB");
        lv_label_set_text(battery_label, voltage_str);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00AAFF), 0);
      } else {
        snprintf(voltage_str, sizeof(voltage_str), "%.2fV", current_battery_voltage);
        lv_label_set_text(battery_label, voltage_str);
        
        if (current_battery_voltage > 3.8) {
          lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00FF00), 0);
        } else if (current_battery_voltage > 3.4) {
          lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFFF00), 0);
        } else {
          lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFF0000), 0);
        }
      }
    }
  }
}

void update_brightness() {
  static int8_t last_brightness = -1;
  int8_t current_brightness = timer.getBrightnessLevel();
  bool on_usb = timer.isOnUSBPower(current_battery_voltage);

  if (timer.getMenuState() == MenuState::EDITING_VALUE &&
      timer.getCurrentMenuItem() == MenuItem::BRIGHTNESS) {
    return;
  }

  if (on_usb && current_brightness < 7) {
    current_brightness += 1;
  } else if (!on_usb && timer.getState() == TimerState::IDLE && current_brightness > 0) {
    current_brightness -= 1;
  }
  
  if (current_brightness != last_brightness) {
    ledcWrite(LCD_BL_PWM_CHANNEL, BRIGHTNESS_VALUES[current_brightness]);
    last_brightness = current_brightness;
  }
}

void apply_theme_assets() {
  uint8_t theme = timer.getTheme();
  if (theme == 2) {
    theme_background = &background_theme2;
    theme_pomodoro = &pomodoro_19_theme2;
    theme_flower = &flower_theme2;
    theme_bud = &bud_theme2;
  } else {
    theme_background = &background_theme1;
    theme_pomodoro = &pomodoro_19_theme1;
    theme_flower = &flower_theme1;
    theme_bud = &bud_theme1;
  }

  if (bg_img != nullptr) {
    lv_img_set_src(bg_img, theme_background);
  }
}

void apply_display_orientation() {
  uint8_t rotation = timer.getScreenFlipped() ? 1 : 3;
  tft.setRotation(rotation);
}



void update_task_display() {

  // First time setup of task list
  if (task_list == nullptr) {
    task_list = lv_obj_create(sidebar_container);
    lv_obj_set_size(task_list, TASK_LIST_WIDTH, TASK_LIST_HEIGHT);
    lv_obj_set_style_pad_all(task_list, TASK_LIST_PAD, 0);
    lv_obj_set_style_pad_bottom(task_list, TASK_LIST_ITEM_HEIGHT, 0);
    lv_obj_set_style_bg_color(task_list, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(task_list, 0, 0);
    lv_obj_clear_flag(task_list, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    // Enable scrolling
    lv_obj_add_flag(task_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(task_list, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < MAX_TASKS; i++) {
      task_numbers[i] = nullptr;
      task_symbols[i] = nullptr;
      task_counts[i] = nullptr;
      count_labels[i] = nullptr;           
      interrupt_symbols[i] = nullptr;      
      interrupt_counts[i] = nullptr;       

    }
  }


  // ADD THIS: Ensure task_list is visible and valid parent
  if (sidebar_container == nullptr || lv_obj_has_flag(task_list, LV_OBJ_FLAG_HIDDEN)) {
    Serial.println("WARNING: task_list or sidebar_container in invalid state");
    return;
  }

  // This ensures the scroll state is clean
  lv_obj_scroll_to_y(task_list, 0, LV_ANIM_OFF);


  uint8_t total_tasks = timer.getTotalTasks();
  uint8_t current_task = timer.getCurrentTaskId();

  // First, hide all existing task labels
  for (int i = 0; i < MAX_TASKS; i++) {
    if (task_numbers[i] != nullptr) {
      if (i < total_tasks) {
        lv_obj_clear_flag(task_numbers[i], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(task_numbers[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (task_symbols[i] != nullptr) {
      if (i < total_tasks) {
        lv_obj_clear_flag(task_symbols[i], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(task_symbols[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (task_counts[i] != nullptr) {
      if (i < total_tasks) {
        lv_obj_clear_flag(task_counts[i], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(task_counts[i], LV_OBJ_FLAG_HIDDEN);
      }
    }

    // HIDE ALL ADDITIONAL LABELS BY DEFAULT
    if (count_labels[i] != nullptr) {
      lv_obj_add_flag(count_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (interrupt_symbols[i] != nullptr) {
      lv_obj_add_flag(interrupt_symbols[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (interrupt_counts[i] != nullptr) {
      lv_obj_add_flag(interrupt_counts[i], LV_OBJ_FLAG_HIDDEN);
    }


  }

  for (int i = 0; i < total_tasks; i++) {
    // Create task number label
    if (task_numbers[i] == nullptr) {
      task_numbers[i] = lv_label_create(task_list);
      lv_obj_align(task_numbers[i], LV_ALIGN_TOP_LEFT, 0, i * TASK_LIST_ITEM_HEIGHT);
      lv_obj_set_style_text_font(task_numbers[i], &lv_font_montserrat_14, 0);
    }

    // Create symbols/counts labels
    if (task_symbols[i] == nullptr) {
      task_symbols[i] = lv_label_create(task_list);
      lv_obj_set_style_text_font(task_symbols[i], &pomodoro_symbols, 0);
    }
    if (task_counts[i] == nullptr) {
      task_counts[i] = lv_label_create(task_list);
      lv_obj_set_style_text_font(task_counts[i], &lv_font_montserrat_14, 0);
      lv_label_set_text(task_counts[i], "");
    }

    // Task number
    char number_str[8];
    snprintf(number_str, sizeof(number_str), "%d:", i + 1);
    lv_label_set_text(task_numbers[i], number_str);

    // Position based on task number
    int x_offset = (i + 1) < 10 ? TASK_NUMBER_OFFSET_SINGLE : TASK_NUMBER_OFFSET_DOUBLE;

    uint8_t completed = timer.getTaskCompletedPomodoros(i);
    uint8_t interrupted = timer.getTaskInterruptedPomodoros(i);


    if (completed + interrupted > 6) {
      // Show counts instead of individual circles
      lv_obj_align(task_symbols[i], LV_ALIGN_TOP_LEFT, x_offset, i * 20 + 3);

      // First label: Filled circle with its count
      char symbol_str[16] = "";
      snprintf(symbol_str, sizeof(symbol_str), "%s", SYMBOL_COMPLETED_POMODORO);
      lv_obj_set_style_text_font(task_symbols[i], &pomodoro_symbols, 0);
      lv_label_set_text(task_symbols[i], symbol_str);
      lv_obj_clear_flag(task_symbols[i], LV_OBJ_FLAG_HIDDEN);

      // Second label: "xN" with regular font
      if (count_labels[i] == nullptr) {
        count_labels[i] = lv_label_create(task_list);
        lv_obj_set_style_text_font(count_labels[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(count_labels[i], "");  // Initialize with empty string
        
      }

      // Set the actual text content
      char count_str[32];
      snprintf(count_str, sizeof(count_str), "x%d", completed);
      lv_label_set_text(count_labels[i], count_str);
      lv_obj_align_to(count_labels[i], task_symbols[i], LV_ALIGN_OUT_RIGHT_MID, 0, 0);
      lv_obj_clear_flag(count_labels[i], LV_OBJ_FLAG_HIDDEN);  // Make sure it's visible

      // Third label: Empty circle with pomodoro_symbols font
      if (interrupt_symbols[i] == nullptr) {
        interrupt_symbols[i] = lv_label_create(task_list);
        lv_obj_set_style_text_font(interrupt_symbols[i], &pomodoro_symbols, 0);
        lv_label_set_text(interrupt_symbols[i], "");  
      }

      char interrupt_symbol[16] = "";
      snprintf(interrupt_symbol, sizeof(interrupt_symbol), "%s", SYMBOL_INTERRUPTED_POMODORO);  // Removed space
      lv_label_set_text(interrupt_symbols[i], interrupt_symbol);
      lv_obj_align_to(interrupt_symbols[i], count_labels[i], LV_ALIGN_OUT_RIGHT_MID, 4, 0);  // Added small gap
      lv_obj_clear_flag(interrupt_symbols[i], LV_OBJ_FLAG_HIDDEN);

      // Fourth label: interrupt count with regular font
      if (interrupt_counts[i] == nullptr) {
        interrupt_counts[i] = lv_label_create(task_list);
        lv_obj_set_style_text_font(interrupt_counts[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(interrupt_counts[i], "");
      }

      char interrupt_count[32];
      snprintf(interrupt_count, sizeof(interrupt_count), "x%d", interrupted);
      lv_label_set_text(interrupt_counts[i], interrupt_count);
      lv_obj_align_to(interrupt_counts[i], interrupt_symbols[i], LV_ALIGN_OUT_RIGHT_MID, 0, 0);
      lv_obj_clear_flag(interrupt_counts[i], LV_OBJ_FLAG_HIDDEN);

      lv_obj_add_flag(task_counts[i], LV_OBJ_FLAG_HIDDEN);

      // Set colors for all labels
      lv_color_t color = (i == current_task) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x808080);
      lv_obj_set_style_text_color(task_symbols[i], color, 0);
      lv_obj_set_style_text_color(count_labels[i], color, 0);
      lv_obj_set_style_text_color(interrupt_symbols[i], color, 0);
      lv_obj_set_style_text_color(interrupt_counts[i], color, 0);
    } else {
      // Show individual circles
      char symbols_str[32] = "";
      for (int j = 0; j < completed; j++) {
        strcat(symbols_str, SYMBOL_COMPLETED_POMODORO);
      }
      for (int j = 0; j < interrupted; j++) {
        strcat(symbols_str, SYMBOL_INTERRUPTED_POMODORO);
      }

      lv_obj_align(task_symbols[i], LV_ALIGN_TOP_LEFT, x_offset, i * 20 + 3);
      lv_label_set_text(task_symbols[i], symbols_str);
      lv_obj_clear_flag(task_symbols[i], LV_OBJ_FLAG_HIDDEN);


      // Hide counts label when showing individual circles
      if (count_labels[i] != nullptr) lv_obj_add_flag(count_labels[i], LV_OBJ_FLAG_HIDDEN);
      if (interrupt_symbols[i] != nullptr) lv_obj_add_flag(interrupt_symbols[i], LV_OBJ_FLAG_HIDDEN);
      if (interrupt_counts[i] != nullptr) lv_obj_add_flag(interrupt_counts[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(task_counts[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Set colors based on current task
    lv_color_t color = (i == current_task) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x808080);
    lv_obj_set_style_text_color(task_numbers[i], color, 0);
    lv_obj_set_style_text_color(task_symbols[i], color, 0);
    lv_obj_set_style_text_color(task_counts[i], color, 0);
  }

  visible_tasks = total_tasks;

  // Make current task visible when scrolling needed
  if (timer.getCurrentTaskId() * 20 > lv_obj_get_height(task_list)) {
    lv_obj_scroll_to_y(task_list, timer.getCurrentTaskId() * 20, LV_ANIM_OFF);  // ← NO ANIMATION
  }
}

lv_color_t color_from_gradient(int percentage) {
  // Map percentage (0-100) to hue (120-0)
  int hue = 120 - (percentage * 120 / 100);
  
  // Convert HSV to RGB
  return lv_color_hsv_to_rgb(hue, 100, 100);
}

// Helper function to disable scrolling on LVGL objects
void disable_scrolling(lv_obj_t *obj) {
  if (obj != nullptr) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
  }
}

// Helper function to set alert colors (inverted or normal)
void set_alert_colors(bool inverted) {
  // Define colors based on inversion state
  lv_color_t bg_main = inverted ? lv_color_hex(COLOR_WHITE) : lv_color_hex(COLOR_BLACK);
  lv_color_t bg_sidebar = inverted ? lv_color_hex(COLOR_SIDEBAR_BG_ALERT) : lv_color_hex(COLOR_SIDEBAR_BG);
  lv_color_t text_main = inverted ? lv_color_hex(COLOR_BLACK) : lv_color_hex(COLOR_WHITE);
  
  // Set background colors
  if (main_container != nullptr) {
    lv_obj_set_style_bg_color(main_container, bg_main, 0);
  }
  
  if (sidebar_container != nullptr) {
    lv_obj_set_style_bg_color(sidebar_container, bg_sidebar, 0);
    if (task_list != nullptr) {
      lv_obj_set_style_bg_color(task_list, bg_sidebar, 0);
    }
  }
  
  lv_obj_set_style_bg_color(lv_scr_act(), bg_main, 0);
  
  // Set main label text colors
  if (time_label != nullptr) {
    lv_obj_set_style_text_color(time_label, text_main, 0);
  }
  if (state_label != nullptr) {
    lv_obj_set_style_text_color(state_label, text_main, 0);
  }
  if (session_label != nullptr) {
    lv_obj_set_style_text_color(session_label, text_main, 0);
  }
  
  // Set sidebar task label colors
  for (int i = 0; i < visible_tasks && i < MAX_TASKS; i++) {
    if (task_numbers[i] != nullptr && !lv_obj_has_flag(task_numbers[i], LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_set_style_text_color(task_numbers[i], text_main, 0);
    }
    if (task_symbols[i] != nullptr && !lv_obj_has_flag(task_symbols[i], LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_set_style_text_color(task_symbols[i], text_main, 0);
    }
    if (count_labels[i] != nullptr && !lv_obj_has_flag(count_labels[i], LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_set_style_text_color(count_labels[i], text_main, 0);
    }
    if (interrupt_symbols[i] != nullptr && !lv_obj_has_flag(interrupt_symbols[i], LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_set_style_text_color(interrupt_symbols[i], text_main, 0);
    }
    if (interrupt_counts[i] != nullptr && !lv_obj_has_flag(interrupt_counts[i], LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_set_style_text_color(interrupt_counts[i], text_main, 0);
    }
  }
  
  // Set battery label color
  if (battery_label != nullptr) {
    lv_obj_set_style_text_color(battery_label, text_main, 0);
  }
}

// Helper function to manage CPU frequency based on timer state
void update_cpu_frequency() {
  static TimerState last_state = TimerState::IDLE;
  TimerState current_state = timer.getState();
  
  // Only change frequency when state actually changes
  if (current_state != last_state) {
    if (current_state == TimerState::IDLE) {
      // Low power mode when idle
      setCpuFrequencyMhz(40);
      Serial.println("CPU: 40MHz (idle power saving)");
    } else {
      // Normal power mode when active
      setCpuFrequencyMhz(80);
      Serial.println("CPU: 80MHz (active)");
    }
    last_state = current_state;
  }
}


/**
 * Calculate how many ticks to show based on duration and interval
 * Examples:
 *   25 minutes / 5 min interval = 5 ticks
 *   25 minutes / 1 min interval = 25 ticks
 */
uint8_t calculate_tick_count(uint8_t duration_minutes) {
  return duration_minutes / TICK_INTERVAL_MINUTES;
}


/**
 * Create tick marks around the arc
 * @param tick_count Number of evenly-spaced ticks to create
 * @param arc_center_x X coordinate of arc center (160 for centered 320px display)
 * @param arc_center_y Y coordinate of arc center (85 for centered 170px display)
 */
void create_arc_ticks(uint8_t tick_count, int arc_center_x = 157, int arc_center_y = 82) {
  if (main_container == nullptr || tick_count == 0 || tick_count > 60) return;
  
  Serial.printf("Creating %d arc ticks\n", tick_count);
  
  // Clean up existing ticks
  for (int i = 0; i < 60; i++) {
    if (arc_ticks[i] != nullptr) {
      lv_obj_del(arc_ticks[i]);
      arc_ticks[i] = nullptr;
    }
  }
  
  current_tick_count = tick_count;
  
  // Create new ticks
  for (int i = 0; i < tick_count; i++) {
    // Calculate angle (starting at 270° = top, going clockwise)
    float angle_deg = 270.0f + (i * 360.0f / tick_count);
    float angle_rad = angle_deg * PI / 180.0f;
    
    // Calculate start point (inner radius)
    int x1 = arc_center_x + (int)(TICK_INNER_RADIUS * cos(angle_rad));
    int y1 = arc_center_y + (int)(TICK_INNER_RADIUS * sin(angle_rad));
    
    // Calculate end point (outer radius)
    int x2 = arc_center_x + (int)(TICK_OUTER_RADIUS * cos(angle_rad));
    int y2 = arc_center_y + (int)(TICK_OUTER_RADIUS * sin(angle_rad));
    
    // Store points for this tick
    tick_points[i][0].x = x1;
    tick_points[i][0].y = y1;
    tick_points[i][1].x = x2;
    tick_points[i][1].y = y2;
    
    // Create line object
    arc_ticks[i] = lv_line_create(main_container);
    lv_line_set_points(arc_ticks[i], tick_points[i], 2);
    
    // Style: make every 5th tick more prominent
    bool is_major = (i % 5 == 0);
    lv_obj_set_style_line_width(arc_ticks[i], is_major ? TICK_MAJOR_WIDTH : TICK_MINOR_WIDTH, 0);
    lv_obj_set_style_line_color(arc_ticks[i], 
                                 lv_color_hex(is_major ? TICK_MAJOR_COLOR : TICK_MINOR_COLOR), 0);
    
    Serial.printf("Tick %d: angle=%.1f° pos=(%d,%d)->(%d,%d)\n", 
                  i, angle_deg, x1, y1, x2, y2);
  }
}


// show all ticks
void show_arc_ticks() {
  for (int i = 0; i < current_tick_count; i++) {
    if (arc_ticks[i] != nullptr) {
      lv_obj_clear_flag(arc_ticks[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}


// Hide all tick marks
void hide_arc_ticks() {
  for (int i = 0; i < current_tick_count; i++) {
    if (arc_ticks[i] != nullptr) {
      lv_obj_add_flag(arc_ticks[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

// create UI elements for work and windup
void create_work_ui_elements() {
  if (work_arc != nullptr) return;  // Already created
  
  // Create arc for progress in the center
  work_arc = lv_arc_create(main_container);
  lv_obj_set_size(work_arc, WORK_ARC_SIZE, WORK_ARC_SIZE);
  lv_arc_set_bg_angles(work_arc, 0, 360);
  lv_arc_set_rotation(work_arc, 270);
  lv_obj_set_style_arc_color(work_arc, lv_color_hex(0x303030), LV_PART_MAIN);
  lv_obj_set_style_arc_color(work_arc, lv_color_hex(0x00E676), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(work_arc, WORK_ARC_WIDTH, LV_PART_MAIN);
  lv_obj_set_style_arc_width(work_arc, WORK_ARC_WIDTH, LV_PART_INDICATOR);
  
  lv_obj_set_style_arc_rounded(work_arc, true, LV_PART_INDICATOR);
  lv_obj_remove_style(work_arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(work_arc, LV_OBJ_FLAG_CLICKABLE);

  // Create container for task display (LEFT side)
  percent_container = lv_obj_create(main_container);
  lv_obj_set_size(percent_container, PERCENT_CONTAINER_WIDTH, PERCENT_CONTAINER_HEIGHT);
  lv_obj_align(percent_container, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_set_style_bg_color(percent_container, lv_color_hex(COLOR_BLACK), 0);
  lv_obj_set_style_border_width(percent_container, 0, 0);
  lv_obj_set_style_pad_top(percent_container, 10, 0);
  lv_obj_set_style_pad_bottom(percent_container, 10, 0);
  disable_scrolling(percent_container);

  // Create labels (no text/formatting yet)
  task_title_label = lv_label_create(percent_container);
  task_num_label = lv_label_create(percent_container);
  
  // Create pomodoro container
  pomo_container = lv_obj_create(percent_container);
  lv_obj_set_size(pomo_container, 60, 60);  // Reduced from 80 to 60
  lv_obj_align(pomo_container, LV_ALIGN_BOTTOM_MID, 0, 10);
  lv_obj_set_style_bg_color(pomo_container, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(pomo_container, 0, 0);
  lv_obj_set_style_pad_all(pomo_container, 5, 0);
  disable_scrolling(pomo_container);
      
  // Create percentage label (in center of arc)
  work_percentage_label = lv_label_create(main_container);
  lv_obj_set_style_text_color(work_percentage_label, lv_color_hex(0x808080), 0);
  lv_obj_set_style_text_font(work_percentage_label, &lv_font_montserrat_40, 0);
  lv_obj_align(work_percentage_label, LV_ALIGN_CENTER, 0, -5);

  // "%" symbol
  percent_symbol = lv_label_create(main_container);
  lv_obj_set_style_text_font(percent_symbol, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(percent_symbol, lv_color_hex(0x808080), 0);
  lv_label_set_text(percent_symbol, "%");
  lv_obj_align(percent_symbol, LV_ALIGN_CENTER, 0, 20);

  // Create container for time display (RIGHT side)
  time_container = lv_obj_create(main_container);
  lv_obj_set_size(time_container, 70, 160);
  lv_obj_align(time_container, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_obj_set_style_bg_color(time_container, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(time_container, 0, 0);
  disable_scrolling(time_container);

  // Time labels
  work_minutes_label = lv_label_create(time_container);
  lv_obj_set_style_text_font(work_minutes_label, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(work_minutes_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(work_minutes_label, LV_ALIGN_CENTER, 0, -5);

  work_seconds_label = lv_label_create(time_container);
  lv_obj_set_style_text_font(work_seconds_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(work_seconds_label, lv_color_hex(0x808080), 0);
  lv_obj_align(work_seconds_label, LV_ALIGN_CENTER, 0, 20);

  lv_obj_move_foreground(work_arc);
  
  // Create tick marks based on work duration
  uint8_t tick_count = calculate_tick_count(timer.getWorkDuration());
  create_arc_ticks(tick_count);
  
  Serial.println("Work UI elements created with tick marks");
}


void update_work_display() {
  // Create UI elements if needed
  create_work_ui_elements();

  // Show work display elements
  if (work_arc != nullptr) lv_obj_clear_flag(work_arc, LV_OBJ_FLAG_HIDDEN);
  if (percent_container != nullptr) lv_obj_clear_flag(percent_container, LV_OBJ_FLAG_HIDDEN);
  if (time_container != nullptr) lv_obj_clear_flag(time_container, LV_OBJ_FLAG_HIDDEN);
  if (work_percentage_label != nullptr) lv_obj_clear_flag(work_percentage_label, LV_OBJ_FLAG_HIDDEN);
  if (work_minutes_label != nullptr) lv_obj_clear_flag(work_minutes_label, LV_OBJ_FLAG_HIDDEN);
  if (work_seconds_label != nullptr) lv_obj_clear_flag(work_seconds_label, LV_OBJ_FLAG_HIDDEN);
  if (percent_symbol != nullptr) lv_obj_clear_flag(percent_symbol, LV_OBJ_FLAG_HIDDEN);

  // Hide standard display elements
  if (time_label != nullptr) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
  if (session_label != nullptr) lv_obj_add_flag(session_label, LV_OBJ_FLAG_HIDDEN);
  if (state_label != nullptr) lv_obj_add_flag(state_label, LV_OBJ_FLAG_HIDDEN);
  // HIDE all summary labels
  if (summary_today_label != nullptr) lv_obj_add_flag(summary_today_label, LV_OBJ_FLAG_HIDDEN);
  if (summary_completed_num != nullptr) lv_obj_add_flag(summary_completed_num, LV_OBJ_FLAG_HIDDEN);
  if (summary_completed_sym != nullptr) lv_obj_add_flag(summary_completed_sym, LV_OBJ_FLAG_HIDDEN);
  if (summary_separator != nullptr) lv_obj_add_flag(summary_separator, LV_OBJ_FLAG_HIDDEN);
  if (summary_interrupted_num != nullptr) lv_obj_add_flag(summary_interrupted_num, LV_OBJ_FLAG_HIDDEN);
  if (summary_interrupted_sym != nullptr) lv_obj_add_flag(summary_interrupted_sym, LV_OBJ_FLAG_HIDDEN);
  if (summary_total_label != nullptr) lv_obj_add_flag(summary_total_label, LV_OBJ_FLAG_HIDDEN);
  if (idle_info_label != nullptr) lv_obj_add_flag(idle_info_label, LV_OBJ_FLAG_HIDDEN);

  show_arc_ticks();

  // CONFIGURE for WORK mode
  // Task title: "TASK" (small)
  if (task_title_label != nullptr) {
    lv_label_set_text(task_title_label, "TASK");
    lv_obj_set_style_text_font(task_title_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(task_title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(task_title_label, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_clear_flag(task_title_label, LV_OBJ_FLAG_HIDDEN);
  }
  
  // Task number: "3" (large)
  if (task_num_label != nullptr) {
    char task_str[8];
    snprintf(task_str, sizeof(task_str), "%d", timer.getCurrentTaskId() + 1);
    lv_label_set_text(task_num_label, task_str);
    lv_obj_set_style_text_font(task_num_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(task_num_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(task_num_label, LV_ALIGN_CENTER, 0, -5);
    lv_obj_clear_flag(task_num_label, LV_OBJ_FLAG_HIDDEN);
  }
  
  // Update pomodoro symbols
  if (pomo_container != nullptr) {
    lv_obj_clean(pomo_container);
    
    uint8_t completed = timer.getTaskCompletedPomodoros(timer.getCurrentTaskId());
    uint8_t interrupted = timer.getTaskInterruptedPomodoros(timer.getCurrentTaskId());
    
    lv_obj_t *pomodoro_label = lv_label_create(pomo_container);
    lv_obj_set_style_text_font(pomodoro_label, &pomodoro_symbols, 0);
    lv_obj_set_style_text_color(pomodoro_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(pomodoro_label, lv_pct(100));  // Make label full width
    lv_obj_set_style_text_align(pomodoro_label, LV_TEXT_ALIGN_CENTER, 0);  
    lv_obj_align(pomodoro_label, LV_ALIGN_TOP_MID, 0, 0);  // CHANGE FROM LV_ALIGN_TOP_LEFT
    
    char symbols_str[128] = "";
    for (int i = 0; i < completed; i++) {
      strcat(symbols_str, SYMBOL_COMPLETED_POMODORO);
      if ((i + 1) % 4 == 0) strcat(symbols_str, "\n");
    }
    for (int i = 0; i < interrupted; i++) {
      strcat(symbols_str, SYMBOL_INTERRUPTED_POMODORO);
      if ((completed + i + 1) % 4 == 0) strcat(symbols_str, "\n");
    }
    
    lv_label_set_text(pomodoro_label, symbols_str);
  }

  // Calculate and update progress
  uint32_t total = timer.getWorkDuration() * 60;
  if (total == 0) total = 1;
  uint32_t remaining = timer.getRemainingSeconds();
  uint32_t elapsed = (remaining <= total) ? (total - remaining) : 0;
  int percentage = (total > 0) ? ((elapsed * 100) / total) : 0;
  percentage = constrain(percentage, 0, 100);
  
  if (work_arc != nullptr) {
    lv_arc_set_value(work_arc, percentage);
  }
  
  char percent_str[8];
  snprintf(percent_str, sizeof(percent_str), "%d", percentage);
  if (work_percentage_label != nullptr) {
    lv_label_set_text(work_percentage_label, percent_str);
  }

  // Update time display
  if (timer.getRemainingMinutes() >= 1) {
    char min_str[4];
    snprintf(min_str, sizeof(min_str), "%lu", timer.getRemainingMinutes());
    if (work_minutes_label != nullptr) lv_label_set_text(work_minutes_label, min_str);
    if (work_seconds_label != nullptr) lv_label_set_text(work_seconds_label, "min");
  } else {
    char sec_str[4];
    snprintf(sec_str, sizeof(sec_str), "%lu", timer.getRemainingSecondsInMinute());
    if (work_minutes_label != nullptr) lv_label_set_text(work_minutes_label, sec_str);
    if (work_seconds_label != nullptr) lv_label_set_text(work_seconds_label, "sec");
  }
  
  lv_color_t progress_color = color_from_gradient(percentage);
  if (work_arc != nullptr) {
    lv_obj_set_style_arc_color(work_arc, progress_color, LV_PART_INDICATOR);
  }
}

// Modified update_windup_display - now just configures existing elements
void update_windup_display() {
  // Create UI elements if needed (don't call update_work_display!)
  create_work_ui_elements();

  // Show display elements
  if (work_arc != nullptr) {
    lv_obj_clear_flag(work_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(work_arc, LV_ALIGN_CENTER, 0, 0);
  }
  if (percent_container != nullptr) lv_obj_clear_flag(percent_container, LV_OBJ_FLAG_HIDDEN);
  if (time_container != nullptr) lv_obj_clear_flag(time_container, LV_OBJ_FLAG_HIDDEN);
  if (work_percentage_label != nullptr) lv_obj_clear_flag(work_percentage_label, LV_OBJ_FLAG_HIDDEN);
  if (work_minutes_label != nullptr) lv_obj_clear_flag(work_minutes_label, LV_OBJ_FLAG_HIDDEN);
  if (work_seconds_label != nullptr) lv_obj_clear_flag(work_seconds_label, LV_OBJ_FLAG_HIDDEN);
  if (percent_symbol != nullptr) lv_obj_clear_flag(percent_symbol, LV_OBJ_FLAG_HIDDEN);

  // Hide standard display elements
  if (time_label != nullptr) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
  if (session_label != nullptr) lv_obj_add_flag(session_label, LV_OBJ_FLAG_HIDDEN);
  if (state_label != nullptr) lv_obj_add_flag(state_label, LV_OBJ_FLAG_HIDDEN);
  if (bg_img != nullptr) lv_obj_add_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
  if (sidebar_container != nullptr) lv_obj_add_flag(sidebar_container, LV_OBJ_FLAG_HIDDEN);
  // HIDE all summary labels
  if (summary_today_label != nullptr) lv_obj_add_flag(summary_today_label, LV_OBJ_FLAG_HIDDEN);
  if (summary_completed_num != nullptr) lv_obj_add_flag(summary_completed_num, LV_OBJ_FLAG_HIDDEN);
  if (summary_completed_sym != nullptr) lv_obj_add_flag(summary_completed_sym, LV_OBJ_FLAG_HIDDEN);
  if (summary_separator != nullptr) lv_obj_add_flag(summary_separator, LV_OBJ_FLAG_HIDDEN);
  if (summary_interrupted_num != nullptr) lv_obj_add_flag(summary_interrupted_num, LV_OBJ_FLAG_HIDDEN);
  if (summary_interrupted_sym != nullptr) lv_obj_add_flag(summary_interrupted_sym, LV_OBJ_FLAG_HIDDEN);
  if (summary_total_label != nullptr) lv_obj_add_flag(summary_total_label, LV_OBJ_FLAG_HIDDEN);
  if (idle_info_label != nullptr) lv_obj_add_flag(idle_info_label, LV_OBJ_FLAG_HIDDEN);

  show_arc_ticks();


  // Get wind-up progress
  uint32_t percentage = timer.getWindupPercentage();
  uint32_t windupSeconds = timer.getWindupValue();
  
  // Update arc progress
  if (work_arc != nullptr) {
    lv_arc_set_value(work_arc, percentage);
    lv_color_t progress_color = color_from_gradient(percentage);
    lv_obj_set_style_arc_color(work_arc, progress_color, LV_PART_INDICATOR);
  }
  
  // Update percentage text
  char percent_str[8];
  snprintf(percent_str, sizeof(percent_str), "%lu", percentage);
  if (work_percentage_label != nullptr) {
    lv_label_set_text(work_percentage_label, percent_str);
  }

  // Update time display
  uint32_t minutes = windupSeconds / 60;
  uint32_t seconds = windupSeconds % 60;
  
  if (minutes >= 1) {
    char min_str[4];
    snprintf(min_str, sizeof(min_str), "%lu", minutes);
    if (work_minutes_label != nullptr) lv_label_set_text(work_minutes_label, min_str);
    if (work_seconds_label != nullptr) lv_label_set_text(work_seconds_label, "min");
  } else {
    char sec_str[4];
    snprintf(sec_str, sizeof(sec_str), "%lu", seconds);
    if (work_minutes_label != nullptr) lv_label_set_text(work_minutes_label, sec_str);
    if (work_seconds_label != nullptr) lv_label_set_text(work_seconds_label, "sec");
  }
  

  // HIDE the large task number label (only used in work mode)
  if (task_num_label != nullptr) {
    lv_obj_add_flag(task_num_label, LV_OBJ_FLAG_HIDDEN);
  }
  
  // Configure task_title_label for wind-up (combined text)
  if (task_title_label != nullptr) {
    char title_str[32];
    snprintf(title_str, sizeof(title_str), "WIND UP\nTASK %d", timer.getCurrentTaskId() + 1);
    lv_label_set_text(task_title_label, title_str);
    lv_obj_set_style_text_color(task_title_label, lv_color_hex(0x00AAFF), 0); // Blue
    lv_obj_set_style_text_font(task_title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(task_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(task_title_label, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_clear_flag(task_title_label, LV_OBJ_FLAG_HIDDEN);
  }
  
  // Update pomodoro display
  if (pomo_container != nullptr) {
    lv_obj_clean(pomo_container);
    
    uint8_t completed = timer.getTaskCompletedPomodoros(timer.getCurrentTaskId());
    uint8_t interrupted = timer.getTaskInterruptedPomodoros(timer.getCurrentTaskId());
    
    lv_obj_t *pomodoro_label = lv_label_create(pomo_container);
    lv_obj_set_style_text_font(pomodoro_label, &pomodoro_symbols, 0);
    lv_obj_set_style_text_color(pomodoro_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(pomodoro_label, lv_pct(100));  // Make label full width
    lv_obj_set_style_text_align(pomodoro_label, LV_TEXT_ALIGN_CENTER, 0);  
    lv_obj_align(pomodoro_label, LV_ALIGN_TOP_MID, 0, 0);  // CHANGE FROM LV_ALIGN_TOP_LEFT
    
    char symbols_str[128] = "";
    for (int i = 0; i < completed; i++) {
      strcat(symbols_str, SYMBOL_COMPLETED_POMODORO);
      if ((i + 1) % 4 == 0) strcat(symbols_str, "\n");
    }
    for (int i = 0; i < interrupted; i++) {
      strcat(symbols_str, SYMBOL_INTERRUPTED_POMODORO);
      if ((completed + i + 1) % 4 == 0) strcat(symbols_str, "\n");
    }
    
    lv_label_set_text(pomodoro_label, symbols_str);
  }
}



void update_pomodoro_display() {
    if (main_container == nullptr) {
        return;
    }

    // Define cluster positions for each task (up to 7 tasks)
    const int cluster_positions[7][2] = {
        {92, 77},   // Task 1
        {15, 72},   // Task 2
        {89, 37},   // Task 3
        {15, 115},  // Task 4
        {88, 119},  // Task 5
        {20, 35},   // Task 6
        {57, 0}    // Task 7
    };
    
    // Define pomodoro offsets within each cluster
    const int pomodoro_offsets[5][2] = {
        {0, 0},     // Pomodoro 1 (center, drawn last - on top)
        {-20, 0},   // Pomodoro 2
        {13, -12},    // Pomodoro 3
        {-10, -13},   // Pomodoro 4
        {2, -24}    // Pomodoro 5
    };

    // Hide all pomodoro images during work mode
    if (timer.getState() == TimerState::WORK || 
        timer.getState() == TimerState::WIND_UP || 
        timer.getState() == TimerState::STARTING) {
        for (int task = 0; task < MAX_TASKS; task++) {
            for (int p = 0; p < 8; p++) {
                if (pomodoro_images[task][p] != nullptr) {
                    lv_obj_add_flag(pomodoro_images[task][p], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
        return;
    }

    // First, hide all existing pomodoro images
    for (int task = 0; task < MAX_TASKS; task++) {
        for (int p = 0; p < 8; p++) {
            if (pomodoro_images[task][p] != nullptr) {
                lv_obj_add_flag(pomodoro_images[task][p], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    // Display pomodoros for each task (up to 7 tasks)
    uint8_t total_tasks = timer.getTotalTasks();
    uint8_t max_display_tasks = (total_tasks > 7) ? 7 : total_tasks;

    int current_task = timer.getCurrentTaskId();
    int next_task = current_task + 1;

    for (int task = 0; task < max_display_tasks; task++) {
        uint8_t completed = timer.getTaskCompletedPomodoros(task);
        
        bool is_current = (task == current_task);
        bool is_next = (task == next_task);
        bool show_random_flower = (!is_current && !is_next && completed == 0);

        int cluster_x = cluster_positions[task][0];
        int cluster_y = cluster_positions[task][1];
        
        // mirror offsets for odd tasks, pomodoros on left side 
        bool mirror = (task % 2 == 1);  // Tasks 1, 3, 5, ... are even (0-indexed)

        if (completed <= 5) {
            // Draw individual pomodoros (reverse order so #1 is on top)
            // for (int p = completed - 1; p >= 0; p--) { // reverse order
            for (int p = 0; p < completed; p++) {  // forward order

                // Create pomodoro image if it doesn't exist
                if (pomodoro_images[task][p] == nullptr) {
                    pomodoro_images[task][p] = lv_img_create(main_container);
                }
                lv_img_set_src(pomodoro_images[task][p], theme_pomodoro);
                lv_obj_set_style_img_opa(pomodoro_images[task][p], LV_OPA_100, 0);
                
                // Calculate position: cluster center + offset (mirrored for even tasks)
                int offset_x = pomodoro_offsets[p][0];
                int offset_y = pomodoro_offsets[p][1];
                
                // Mirror x-offset for even tasks
                if (mirror) {
                    offset_x = -offset_x;
                }

                // Calculate position: cluster center + offset
                int pomodoro_x = cluster_x + offset_x;
                int pomodoro_y = cluster_y + offset_y;
                
                lv_obj_set_pos(pomodoro_images[task][p], pomodoro_x, pomodoro_y);
                lv_obj_clear_flag(pomodoro_images[task][p], LV_OBJ_FLAG_HIDDEN);
            }

            // Remaining slots for current task: up to 3 random buds
            if (is_current) {
                int available_slots[5];
                int available_count = 0;
                for (int p = completed; p < 5; p++) {
                    available_slots[available_count++] = p;
                }
                if (available_count > 0) {
                    uint32_t seed = (uint32_t)(timer.getPomodorosSinceLastLongBreak() * 31u + task * 17u + current_task * 7u);
                    for (int i = available_count - 1; i > 0; i--) {
                        seed = seed * 1103515245u + 12345u;
                        int j = seed % (i + 1);
                        int tmp = available_slots[i];
                        available_slots[i] = available_slots[j];
                        available_slots[j] = tmp;
                    }
                    int show_count = (available_count > 3) ? 3 : available_count;
                    for (int i = 0; i < show_count; i++) {
                        int p = available_slots[i];
                        if (pomodoro_images[task][p] == nullptr) {
                            pomodoro_images[task][p] = lv_img_create(main_container);
                        }
                        lv_img_set_src(pomodoro_images[task][p], theme_bud);
                        lv_obj_set_style_img_opa(pomodoro_images[task][p], LV_OPA_70, 0);

                        int offset_x = pomodoro_offsets[p][0];
                        int offset_y = pomodoro_offsets[p][1];
                        if (mirror) {
                            offset_x = -offset_x;
                        }

                        int bud_x = cluster_x + offset_x;
                        int bud_y = cluster_y + offset_y;
                        lv_obj_set_pos(pomodoro_images[task][p], bud_x, bud_y);
                        lv_obj_clear_flag(pomodoro_images[task][p], LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }

            // Next task: up to 3 random flowers for remaining slots
            if (is_next) {
                int available_slots[5];
                int available_count = 0;
                for (int p = completed; p < 5; p++) {
                    available_slots[available_count++] = p;
                }
                if (available_count > 0) {
                    uint32_t seed = (uint32_t)(timer.getPomodorosSinceLastLongBreak() * 31u + task * 17u + (current_task + 1) * 7u);
                    for (int i = available_count - 1; i > 0; i--) {
                        seed = seed * 1103515245u + 12345u;
                        int j = seed % (i + 1);
                        int tmp = available_slots[i];
                        available_slots[i] = available_slots[j];
                        available_slots[j] = tmp;
                    }
                    int show_count = (available_count > 3) ? 3 : available_count;
                    for (int i = 0; i < show_count; i++) {
                        int p = available_slots[i];
                        if (pomodoro_images[task][p] == nullptr) {
                            pomodoro_images[task][p] = lv_img_create(main_container);
                        }
                        lv_img_set_src(pomodoro_images[task][p], theme_flower);
                        lv_obj_set_style_img_opa(pomodoro_images[task][p], LV_OPA_70, 0);

                        int offset_x = pomodoro_offsets[p][0];
                        int offset_y = pomodoro_offsets[p][1];
                        if (mirror) {
                            offset_x = -offset_x;
                        }

                        int flower_x = cluster_x + offset_x;
                        int flower_y = cluster_y + offset_y;
                        lv_obj_set_pos(pomodoro_images[task][p], flower_x, flower_y);
                        lv_obj_clear_flag(pomodoro_images[task][p], LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }

            // Other empty tasks: single flower at a pseudo-random slot
            if (show_random_flower) {
                int flower_slot = (timer.getPomodorosSinceLastLongBreak() + task) % 5;
                if (pomodoro_images[task][flower_slot] == nullptr) {
                    pomodoro_images[task][flower_slot] = lv_img_create(main_container);
                }
                lv_img_set_src(pomodoro_images[task][flower_slot], theme_flower);
                lv_obj_set_style_img_opa(pomodoro_images[task][flower_slot], LV_OPA_70, 0);

                int offset_x = pomodoro_offsets[flower_slot][0];
                int offset_y = pomodoro_offsets[flower_slot][1];
                if (mirror) {
                    offset_x = -offset_x;
                }

                int flower_x = cluster_x + offset_x;
                int flower_y = cluster_y + offset_y;
                lv_obj_set_pos(pomodoro_images[task][flower_slot], flower_x, flower_y);
                lv_obj_clear_flag(pomodoro_images[task][flower_slot], LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            // More than 5 pomodoros: draw 5 pomodoros + count overlay
            
            // Draw the 5 base pomodoros (reverse order)
            for (int p = 4; p >= 0; p--) {
                if (pomodoro_images[task][p] == nullptr) {
                    pomodoro_images[task][p] = lv_img_create(main_container);
                }
                lv_img_set_src(pomodoro_images[task][p], theme_pomodoro);
                lv_obj_set_style_img_opa(pomodoro_images[task][p], LV_OPA_100, 0);
                
                int pomodoro_x = cluster_x + pomodoro_offsets[p][0];
                int pomodoro_y = cluster_y + pomodoro_offsets[p][1];
                
                lv_obj_set_pos(pomodoro_images[task][p], pomodoro_x, pomodoro_y);
                lv_obj_clear_flag(pomodoro_images[task][p], LV_OBJ_FLAG_HIDDEN);
            }
            
            // Create/update count overlay label (use slot 5 for the count label)
            if (pomodoro_images[task][5] == nullptr) {
                // Create label instead of image for the count
                pomodoro_images[task][5] = lv_label_create(main_container);
                lv_obj_set_style_text_font(pomodoro_images[task][5], &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(pomodoro_images[task][5], lv_color_hex(0xFFFFFF), 0);
                lv_obj_set_style_bg_color(pomodoro_images[task][5], lv_color_hex(0x000000), 0);
                lv_obj_set_style_bg_opa(pomodoro_images[task][5], LV_OPA_70, 0);
                lv_obj_set_style_pad_all(pomodoro_images[task][5], 2, 0);
                lv_obj_set_style_radius(pomodoro_images[task][5], 8, 0);
            }
            
            // Set count text and position at cluster center
            char count_str[8];
            snprintf(count_str, sizeof(count_str), "%d", completed);
            lv_label_set_text((lv_obj_t*)pomodoro_images[task][5], count_str);
            lv_obj_set_pos(pomodoro_images[task][5], cluster_x - 5, cluster_y - 8); // Centered
            lv_obj_clear_flag(pomodoro_images[task][5], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void update_long_break_progress() {
    // Create container on first call
    if (progress_container == nullptr) {
        progress_container = lv_obj_create(lv_scr_act());
        lv_obj_set_size(progress_container, 130, 50);  // 135px wide
        lv_obj_align(progress_container, LV_ALIGN_TOP_LEFT, MAIN_CONTAINER_WIDTH_NORMAL - 125, 0);  // Align to right of main area
        lv_obj_set_style_bg_opa(progress_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(progress_container, 0, 0);
        lv_obj_set_style_pad_all(progress_container, 0, 0);
        disable_scrolling(progress_container);
    }
    
    // Don't show during work, wind-up, or if menu is open
    if (timer.getState() == TimerState::WORK || 
        timer.getState() == TimerState::WIND_UP || 
        timer.getState() == TimerState::STARTING || 
        timer.getMenuState() != MenuState::CLOSED) {
        lv_obj_add_flag(progress_container, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    // Show container
    lv_obj_clear_flag(progress_container, LV_OBJ_FLAG_HIDDEN);
    
    uint8_t completed = timer.getPomodorosSinceLastLongBreak();
    uint8_t total = timer.getPomodorosBeforeLongBreak();
    
    // Center within the 135px container
    int start_x = (130 / 2) - (total * PROGRESS_POMO_SPACING / 2);
    
    for (int i = 0; i < total; i++) {
        // Create pomodoro image if it doesn't exist
        if (progress_pomodoros[i] == nullptr) {
            progress_pomodoros[i] = lv_img_create(progress_container);
        }
        lv_img_set_src(progress_pomodoros[i], theme_pomodoro);
        
        // Position the pomodoro (relative to container)
        lv_obj_set_pos(progress_pomodoros[i], start_x + i * PROGRESS_POMO_SPACING, PROGRESS_POMO_Y_POS);
        
        // Show/hide and set opacity based on completion
        if (i < completed) {
            lv_obj_clear_flag(progress_pomodoros[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_img_opa(progress_pomodoros[i], LV_OPA_100, 0);
        } else {
            lv_obj_clear_flag(progress_pomodoros[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_img_opa(progress_pomodoros[i], LV_OPA_40, 0);
        }
    }
    
    // Hide any extra pomodoros if settings changed
    for (int i = total; i < 10; i++) {
        if (progress_pomodoros[i] != nullptr) {
            lv_obj_add_flag(progress_pomodoros[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Calculate total completed pomodoros across all tasks
uint8_t get_total_completed_pomodoros() {
  uint8_t total = 0;
  for (int i = 0; i < timer.getTotalTasks(); i++) {
    total += timer.getTaskCompletedPomodoros(i);
  }
  return total;
}


// Calculate total interrupted pomodoros across all tasks
uint8_t get_total_interrupted_pomodoros() {
  uint8_t total = 0;
  for (int i = 0; i < timer.getTotalTasks(); i++) {
    total += timer.getTaskInterruptedPomodoros(i);
  }
  return total;
}

// Calculate total work time in minutes (completed pomodoros only)
uint16_t get_total_work_minutes() {
  uint8_t completed = get_total_completed_pomodoros();
  return completed * timer.getWorkDuration();
}


// Format time duration into hours and minutes string
// Examples: "1h 25m", "45m", "3h 0m"
void format_duration(uint16_t total_minutes, char* buffer, size_t buffer_size) {
  uint16_t hours = total_minutes / 60;
  uint16_t minutes = total_minutes % 60;
  
  if (hours > 0) {
    snprintf(buffer, buffer_size, "%dh %dm", hours, minutes);
  } else {
    snprintf(buffer, buffer_size, "%dm", minutes);
  }
}


// Updated display function
void update_display() {


  static uint8_t last_theme = 0;
  static TimerState last_state = TimerState::IDLE;
  static bool last_flipped = false;


  // First time setup of containers
  if (main_container == nullptr) {
    // Create main container for timer
    main_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_container, MAIN_CONTAINER_WIDTH_NORMAL, MAIN_CONTAINER_HEIGHT);
    lv_obj_set_style_pad_all(main_container, 3, 0);
    lv_obj_align(main_container, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(main_container, lv_color_hex(COLOR_BLACK), 0);
    lv_obj_set_style_border_width(main_container, 0, 0);

    // DISABLE SCROLLING ON MAIN CONTAINER:
    disable_scrolling(main_container);

    // Add background image
    bg_img = lv_img_create(main_container);
    lv_img_set_src(bg_img, theme_background);
    lv_obj_align(bg_img, LV_ALIGN_BOTTOM_LEFT, 0, 5);  // Center in main container

    // CREATE BATTERY LABEL (add this after main_container creation)
    battery_label = lv_label_create(lv_scr_act());  // Create on screen, not container
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_12, 0);  // Small font
    lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(battery_label, LV_ALIGN_TOP_LEFT, 2, 2);  // Upper left corner
    lv_label_set_text(battery_label, "-.--V");  // Initial text




    // Create sidebar container
    sidebar_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sidebar_container, SIDEBAR_WIDTH, SIDEBAR_HEIGHT);
    lv_obj_set_style_pad_all(sidebar_container, 0, 0);
    lv_obj_align(sidebar_container, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sidebar_container, lv_color_hex(COLOR_SIDEBAR_BG), 0);
    lv_obj_set_style_border_width(sidebar_container, 0, 0);
    disable_scrolling(sidebar_container);

    // Create labels as children of main_container
    time_label = lv_label_create(main_container);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_30, 0);
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -30, 0);

    state_label = lv_label_create(main_container);
    lv_obj_align(state_label, LV_ALIGN_RIGHT_MID, -20, 30);

    // Create summary display labels (hidden by default)
    summary_today_label = lv_label_create(main_container);
    lv_obj_set_style_text_font(summary_today_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(summary_today_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(summary_today_label, LV_ALIGN_CENTER, 45, -10);
    lv_obj_add_flag(summary_today_label, LV_OBJ_FLAG_HIDDEN);

    // Completed count (number with montserrat)
    summary_completed_num = lv_label_create(main_container);
    lv_obj_set_style_text_font(summary_completed_num, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(summary_completed_num, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(summary_completed_num, LV_ALIGN_CENTER, 23, 10);
    lv_obj_add_flag(summary_completed_num, LV_OBJ_FLAG_HIDDEN);

    // Completed symbol (pomodoro_symbols font)
    summary_completed_sym = lv_label_create(main_container);
    lv_obj_set_style_text_font(summary_completed_sym, &pomodoro_symbols, 0);
    lv_obj_set_style_text_color(summary_completed_sym, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(summary_completed_sym, SYMBOL_COMPLETED_POMODORO);
    lv_obj_add_flag(summary_completed_sym, LV_OBJ_FLAG_HIDDEN);

    // Separator "|"
    summary_separator = lv_label_create(main_container);  // ← Use global variable
    lv_obj_set_style_text_font(summary_separator, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(summary_separator, lv_color_hex(0x808080), 0);
    lv_label_set_text(summary_separator, "|");
    lv_obj_add_flag(summary_separator, LV_OBJ_FLAG_HIDDEN);

    // Interrupted count (number with montserrat)
    summary_interrupted_num = lv_label_create(main_container);
    lv_obj_set_style_text_font(summary_interrupted_num, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(summary_interrupted_num, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_flag(summary_interrupted_num, LV_OBJ_FLAG_HIDDEN);

    // Interrupted symbol (pomodoro_symbols font)
    summary_interrupted_sym = lv_label_create(main_container);
    lv_obj_set_style_text_font(summary_interrupted_sym, &pomodoro_symbols, 0);
    lv_obj_set_style_text_color(summary_interrupted_sym, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(summary_interrupted_sym, SYMBOL_INTERRUPTED_POMODORO);
    lv_obj_add_flag(summary_interrupted_sym, LV_OBJ_FLAG_HIDDEN);

    // Total time label
    summary_total_label = lv_label_create(main_container);
    lv_obj_set_style_text_font(summary_total_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(summary_total_label, lv_color_hex(0x808080), 0);
    lv_obj_align(summary_total_label, LV_ALIGN_CENTER, 53, 40);
    lv_obj_add_flag(summary_total_label, LV_OBJ_FLAG_HIDDEN);

    // Create idle info label
    idle_info_label = lv_label_create(main_container);
    lv_obj_set_style_text_font(idle_info_label, &lv_font_montserrat_14, 0);
    lv_obj_align(idle_info_label, LV_ALIGN_BOTTOM_RIGHT, -30, -5);
    lv_obj_set_style_text_color(idle_info_label, lv_color_hex(0xFF5F1F), 0);
    lv_obj_add_flag(idle_info_label, LV_OBJ_FLAG_HIDDEN);

  }

  update_menu_display();
  if (timer.getScreenFlipped() != last_flipped) {
    apply_display_orientation();
    lv_obj_invalidate(lv_scr_act());
    last_flipped = timer.getScreenFlipped();
  }
  if (timer.getTheme() != last_theme) {
    apply_theme_assets();
    update_pomodoro_display();
    update_long_break_progress();
    last_theme = timer.getTheme();
  }

  if (timer.getMenuState() != MenuState::CLOSED) {
      update_menu_display();
      return;
  }

  if (timer.getState() == TimerState::STARTING) {
    if (starting_container == nullptr) {
      starting_container = lv_obj_create(lv_scr_act());
      lv_obj_set_size(starting_container, DISPLAY_WIDTH, DISPLAY_HEIGHT);
      lv_obj_center(starting_container);
      lv_obj_set_style_bg_color(starting_container, lv_color_hex(COLOR_BLACK), 0);
      lv_obj_set_style_border_width(starting_container, 0, 0);
      disable_scrolling(starting_container);

      starting_label = lv_label_create(starting_container);
      lv_obj_set_style_text_font(starting_label, &lv_font_montserrat_24, 0);
      lv_obj_set_style_text_color(starting_label, lv_color_hex(0x00E676), 0);
      lv_label_set_text(starting_label, "Brewing focus...");
      lv_obj_align(starting_label, LV_ALIGN_CENTER, 0, -10);

      starting_sub_label = lv_label_create(starting_container);
      lv_obj_set_style_text_font(starting_sub_label, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(starting_sub_label, lv_color_hex(0x808080), 0);
      lv_label_set_text(starting_sub_label, "Pomodoro starting");
      lv_obj_align(starting_sub_label, LV_ALIGN_CENTER, 0, 18);
    }

    lv_obj_clear_flag(starting_container, LV_OBJ_FLAG_HIDDEN);
    if (main_container != nullptr) lv_obj_add_flag(main_container, LV_OBJ_FLAG_HIDDEN);
    if (sidebar_container != nullptr) lv_obj_add_flag(sidebar_container, LV_OBJ_FLAG_HIDDEN);
    if (progress_container != nullptr) lv_obj_add_flag(progress_container, LV_OBJ_FLAG_HIDDEN);

    if (last_state != TimerState::STARTING && timer.getAlarmVibration()) {
      digitalWrite(PIN_VIBRATION, HIGH);
      delay(WINDUP_START_VIBRATION_MS);
      digitalWrite(PIN_VIBRATION, LOW);
    }

    last_state = TimerState::STARTING;
    return;
  }

  if (starting_container != nullptr) {
    lv_obj_add_flag(starting_container, LV_OBJ_FLAG_HIDDEN);
  }
  if (main_container != nullptr) lv_obj_clear_flag(main_container, LV_OBJ_FLAG_HIDDEN);


  // Show/hide based on timer state
  if (timer.getState() == TimerState::WORK) {
    //hide during work
    lv_obj_add_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sidebar_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(main_container, 320, 170);
    update_work_display();  // Call work-specific display update

    // HIDE timer labels during work mode
    if (time_label != nullptr) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (state_label != nullptr) lv_obj_add_flag(state_label, LV_OBJ_FLAG_HIDDEN);

  } else if (timer.getState() == TimerState::WIND_UP) {
    // Hide during wind-up (similar to work)
    lv_obj_add_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sidebar_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(main_container, 320, 170);
    update_windup_display();  // Call wind-up display

    // HIDE timer labels during wind-up mode
    if (time_label != nullptr) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (state_label != nullptr) lv_obj_add_flag(state_label, LV_OBJ_FLAG_HIDDEN);
    

  } else {
    // === IDLE STATE DISPLAY ===
    

    // Show standard display elements when not WORK or WIND_UP
    lv_obj_clear_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sidebar_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(main_container, 240, 170);

   // Show standard display elements
    if (time_label != nullptr) lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (state_label != nullptr) lv_obj_clear_flag(state_label, LV_OBJ_FLAG_HIDDEN);



    // Hide work display elements if they exist
    if (work_arc != nullptr) lv_obj_add_flag(work_arc, LV_OBJ_FLAG_HIDDEN);
    if (work_percentage_label != nullptr) lv_obj_add_flag(work_percentage_label, LV_OBJ_FLAG_HIDDEN);
    if (work_minutes_label != nullptr) lv_obj_add_flag(work_minutes_label, LV_OBJ_FLAG_HIDDEN);
    if (work_seconds_label != nullptr) lv_obj_add_flag(work_seconds_label, LV_OBJ_FLAG_HIDDEN);
    if (percent_container != nullptr) lv_obj_add_flag(percent_container, LV_OBJ_FLAG_HIDDEN);
    if (time_container != nullptr) lv_obj_add_flag(time_container, LV_OBJ_FLAG_HIDDEN);
    if (status_label != nullptr) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    if (percent_symbol != nullptr)lv_obj_add_flag(percent_symbol, LV_OBJ_FLAG_HIDDEN);  // Add this line
  

    hide_arc_ticks();
    update_task_display();

  }

  update_pomodoro_display();
  update_long_break_progress();


  // === UPDATE TIME DISPLAY ===
  char time_str[32];

  if (timer.getState() != TimerState::WORK && 
      timer.getState() != TimerState::WIND_UP && 
      timer.getState() != TimerState::STARTING) {
    if (timer.getState() == TimerState::IDLE) {
        // Show session summary instead of 00:00
        uint8_t completed = get_total_completed_pomodoros();
        uint8_t interrupted = get_total_interrupted_pomodoros();
        uint16_t total_minutes = get_total_work_minutes();

        // Calculate idle time
        uint32_t idle_seconds = (millis() - timer.getIdleStartTime()) / 1000;
        uint32_t idle_minutes = idle_seconds / 60;
        
    if (completed > 0 || interrupted > 0) {
      // === SHOW SUMMARY STATS ===
      
      // HIDE timer labels
      if (time_label != nullptr) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
      if (state_label != nullptr) lv_obj_add_flag(state_label, LV_OBJ_FLAG_HIDDEN);
      
      // "Today:" label
      if (summary_today_label != nullptr) {
        lv_label_set_text(summary_today_label, "Today:");
        lv_obj_clear_flag(summary_today_label, LV_OBJ_FLAG_HIDDEN);
      }
      
      // Completed number
      if (summary_completed_num != nullptr) {
        char num_str[8];
        snprintf(num_str, sizeof(num_str), "%d:", completed);
        lv_label_set_text(summary_completed_num, num_str);
        lv_obj_clear_flag(summary_completed_num, LV_OBJ_FLAG_HIDDEN);
      }
      
      // Completed symbol (positioned relative to number)
      if (summary_completed_sym != nullptr) {
        lv_obj_align_to(summary_completed_sym, summary_completed_num, LV_ALIGN_OUT_RIGHT_MID, 3, 0);
        lv_obj_clear_flag(summary_completed_sym, LV_OBJ_FLAG_HIDDEN);
      }
      
      // Separator
      if (summary_separator != nullptr) {
        lv_obj_align_to(summary_separator, summary_completed_sym, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
        lv_obj_clear_flag(summary_separator, LV_OBJ_FLAG_HIDDEN);
      }
      
      // Interrupted number
      if (summary_interrupted_num != nullptr) {
        char num_str[8];
        snprintf(num_str, sizeof(num_str), "%d:", interrupted);
        lv_label_set_text(summary_interrupted_num, num_str);
        lv_obj_align_to(summary_interrupted_num, summary_separator, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
        lv_obj_clear_flag(summary_interrupted_num, LV_OBJ_FLAG_HIDDEN);
      }
      
      // Interrupted symbol
      if (summary_interrupted_sym != nullptr) {
        lv_obj_align_to(summary_interrupted_sym, summary_interrupted_num, LV_ALIGN_OUT_RIGHT_MID, 3, 0);
        lv_obj_clear_flag(summary_interrupted_sym, LV_OBJ_FLAG_HIDDEN);
      }
      
      // Total time
      if (summary_total_label != nullptr) {
        char duration_str[16];
        format_duration(total_minutes, duration_str, sizeof(duration_str));
        char total_str[32];
        snprintf(total_str, sizeof(total_str), "Total: %s", duration_str);
        lv_label_set_text(summary_total_label, total_str);
        lv_obj_clear_flag(summary_total_label, LV_OBJ_FLAG_HIDDEN);
      }
      
    } else {
      // === SHOW READY MESSAGE ===
      
      // HIDE all summary labels
      if (summary_today_label != nullptr) lv_obj_add_flag(summary_today_label, LV_OBJ_FLAG_HIDDEN);
      if (summary_completed_num != nullptr) lv_obj_add_flag(summary_completed_num, LV_OBJ_FLAG_HIDDEN);
      if (summary_completed_sym != nullptr) lv_obj_add_flag(summary_completed_sym, LV_OBJ_FLAG_HIDDEN);
      if (summary_separator != nullptr) lv_obj_add_flag(summary_separator, LV_OBJ_FLAG_HIDDEN);
      if (summary_interrupted_num != nullptr) lv_obj_add_flag(summary_interrupted_num, LV_OBJ_FLAG_HIDDEN);
      if (summary_interrupted_sym != nullptr) lv_obj_add_flag(summary_interrupted_sym, LV_OBJ_FLAG_HIDDEN);
      if (summary_total_label != nullptr) lv_obj_add_flag(summary_total_label, LV_OBJ_FLAG_HIDDEN);
      
      // SHOW timer labels
      if (time_label != nullptr) {
        lv_label_set_text(time_label, "00:00");
        lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
      }
      
      if (state_label != nullptr) {
        lv_label_set_text(state_label, "Ready to start");
        lv_obj_set_style_text_color(state_label, lv_color_hex(0x808080), 0);
        lv_obj_clear_flag(state_label, LV_OBJ_FLAG_HIDDEN);
      }
    }
      
      // === IDLE TIME WARNING ===
      if (idle_info_label != nullptr) {
        if (idle_minutes > 1) {
          char idle_str[32];
          snprintf(idle_str, sizeof(idle_str), "Idle: %lumin", idle_minutes);
          lv_label_set_text(idle_info_label, idle_str);
          lv_obj_clear_flag(idle_info_label, LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_add_flag(idle_info_label, LV_OBJ_FLAG_HIDDEN);
        }
      }
      
    } else {
      // === ACTIVE TIMER (Work/Break) ===
      
      // HIDE summary labels
      if (summary_today_label != nullptr) lv_obj_add_flag(summary_today_label, LV_OBJ_FLAG_HIDDEN);
      if (summary_completed_num != nullptr) lv_obj_add_flag(summary_completed_num, LV_OBJ_FLAG_HIDDEN);
      if (summary_completed_sym != nullptr) lv_obj_add_flag(summary_completed_sym, LV_OBJ_FLAG_HIDDEN);
      if (summary_separator != nullptr) lv_obj_add_flag(summary_separator, LV_OBJ_FLAG_HIDDEN);
      if (summary_interrupted_num != nullptr) lv_obj_add_flag(summary_interrupted_num, LV_OBJ_FLAG_HIDDEN);
      if (summary_interrupted_sym != nullptr) lv_obj_add_flag(summary_interrupted_sym, LV_OBJ_FLAG_HIDDEN);
      if (summary_total_label != nullptr) lv_obj_add_flag(summary_total_label, LV_OBJ_FLAG_HIDDEN);
      if (idle_info_label != nullptr) lv_obj_add_flag(idle_info_label, LV_OBJ_FLAG_HIDDEN);

      
      // SHOW and update timer labels
      if (time_label != nullptr) {
        char time_str[10];
        snprintf(time_str, sizeof(time_str), "%02lu:%02lu",
                 timer.getRemainingMinutes(),
                 timer.getRemainingSecondsInMinute());
        lv_label_set_text(time_label, time_str);
        lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
      }
      
      if (state_label != nullptr) {
        lv_obj_clear_flag(state_label, LV_OBJ_FLAG_HIDDEN);
              
        // Update state display
        switch (timer.getState()) {
          case TimerState::SHORT_BREAK:
            lv_label_set_text(state_label, "Short Break");
            lv_obj_set_style_text_color(state_label, lv_color_hex(0x00AAFF), 0);
            break;
          case TimerState::LONG_BREAK:
            lv_label_set_text(state_label, "Long Break!");
            lv_obj_set_style_text_color(state_label, lv_color_hex(0x0055FF), 0);
            break;
          case TimerState::PAUSED_SHORT_BREAK:
          case TimerState::PAUSED_LONG_BREAK:
            lv_label_set_text(state_label, "Break Paused");
            lv_obj_set_style_text_color(state_label, lv_color_hex(0xFFAA00), 0);
            break;
          case TimerState::PAUSED_WORK:
            lv_label_set_text(state_label, "Focus Paused");
            lv_obj_set_style_text_color(state_label, lv_color_hex(0xFFAA00), 0);
            break;
          default:
            break;
        }
      }
    }
  }
    update_battery_display();


    // Handle alert state
    static bool was_alert_active = false;  // Track previous alert state
    
    if (timer.isAlertActive()) {
        // Vibration: only if enabled
        if (timer.getAlarmVibration() && timer.getBlinkCount() == 1) {
            digitalWrite(PIN_VIBRATION, HIGH);
            delay(50);
            digitalWrite(PIN_VIBRATION, LOW);
        }

        // Visual flash: only if enabled
        if (timer.getAlarmFlash()) {
            set_alert_colors(timer.getBlinkCount() == 1);  // Inverted when blinkCount is 1
        }
        was_alert_active = true;
        return;
    }

    // Turn off vibration when not in alert
    digitalWrite(PIN_VIBRATION, LOW);
    
    // Reset colors only when transitioning OUT of alert (not every frame)
    if (was_alert_active) {
        set_alert_colors(false);
        was_alert_active = false;
        // Re-update task display to restore proper task-specific colors
        if (timer.getState() != TimerState::WORK && 
            timer.getState() != TimerState::WIND_UP && 
            timer.getState() != TimerState::STARTING) {
            update_task_display();
        }
    }

    last_state = timer.getState();
}


void update_menu_display() {
    static lv_obj_t *menu_container = nullptr;
    static lv_obj_t *menu_title = nullptr;
    static lv_obj_t *menu_item_label = nullptr;
    static lv_obj_t *menu_value_label = nullptr;
    
    // ADD THIS CHECK AT THE VERY START:
    if (timer.getMenuState() == MenuState::CLOSED) {
        // Hide menu when closed
        if (menu_container != nullptr) {
            lv_obj_add_flag(menu_container, LV_OBJ_FLAG_HIDDEN);
        }
        // Show normal display elements
        if (time_label != nullptr) lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
        if (state_label != nullptr) lv_obj_clear_flag(state_label, LV_OBJ_FLAG_HIDDEN);
        if (session_label != nullptr) lv_obj_clear_flag(session_label, LV_OBJ_FLAG_HIDDEN);
        if (bg_img != nullptr) lv_obj_clear_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
        if (sidebar_container != nullptr) lv_obj_clear_flag(sidebar_container, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    // Hide normal display elements when menu is open
    if (time_label != nullptr) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (state_label != nullptr) lv_obj_add_flag(state_label, LV_OBJ_FLAG_HIDDEN);
    if (session_label != nullptr) lv_obj_add_flag(session_label, LV_OBJ_FLAG_HIDDEN);
    if (bg_img != nullptr) lv_obj_add_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
    if (sidebar_container != nullptr) lv_obj_add_flag(sidebar_container, LV_OBJ_FLAG_HIDDEN);  // ADD THIS

    // Create menu display (first time only)
    if (menu_container == nullptr) {
        menu_container = lv_obj_create(lv_scr_act());  
        lv_obj_set_size(menu_container, 310, 160);
        lv_obj_center(menu_container);
        lv_obj_set_style_bg_color(menu_container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(menu_container, 0, 0);  
        
        menu_title = lv_label_create(menu_container);
        lv_obj_set_style_text_font(menu_title, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(menu_title, lv_color_hex(0x8D00FF), 0); 
        lv_label_set_text(menu_title, "SETTINGS");
        lv_obj_align(menu_title, LV_ALIGN_TOP_MID, 0, 5);
        
        menu_item_label = lv_label_create(menu_container);
        lv_obj_set_style_text_font(menu_item_label, &lv_font_montserrat_16, 0);
        lv_obj_align(menu_item_label, LV_ALIGN_CENTER, 0, -15);
        
        menu_value_label = lv_label_create(menu_container);
        lv_obj_set_style_text_font(menu_value_label, &lv_font_montserrat_30, 0);
        lv_obj_align(menu_value_label, LV_ALIGN_CENTER, 0, 20);
    }
    
    lv_obj_clear_flag(menu_container, LV_OBJ_FLAG_HIDDEN);
    
    // Update menu content
if (timer.getMenuState() == MenuState::MENU_LIST) {
    // Set label text based on current menu item
    switch (timer.getCurrentMenuItem()) {
        case MenuItem::POMODORO_LENGTH:
            lv_label_set_text(menu_item_label, "Pomodoro Length");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d min", timer.getWorkDuration());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::SHORT_BREAK_LENGTH:
            lv_label_set_text(menu_item_label, "Short Break Length");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d min", timer.getShortBreakDuration());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::LONG_BREAK_LENGTH:
            lv_label_set_text(menu_item_label, "Long Break Length");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d min", timer.getLongBreakDuration());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::POMODOROS_BEFORE_LONG_BREAK:
            lv_label_set_text(menu_item_label, "n Pomodoros before long break");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d pomodoros", timer.getPomodorosBeforeLongBreak());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::LONG_BREAK_PROGRESS:
            lv_label_set_text(menu_item_label, "Long Break Progress");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d / %d",
                         timer.getPomodorosSinceLastLongBreak(),
                         timer.getPomodorosBeforeLongBreak());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::MANAGE_TASKS:
            lv_label_set_text(menu_item_label, "Total Tasks");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d tasks", timer.getTotalTasks());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::EDIT_COMPLETED_POMODOROS:
            {
                char label_str[32];
                snprintf(label_str, sizeof(label_str), "Task %d Completed", timer.getCurrentTaskId() + 1);
                lv_label_set_text(menu_item_label, label_str);
                
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d", timer.getTaskCompletedPomodoros(timer.getCurrentTaskId()));
                lv_label_set_text(menu_value_label, val_str);
            }
            break;

        case MenuItem::EDIT_INTERRUPTED_POMODOROS:
            {
                char label_str[32];
                snprintf(label_str, sizeof(label_str), "Task %d Interrupted", timer.getCurrentTaskId() + 1);
                lv_label_set_text(menu_item_label, label_str);
                
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d", timer.getTaskInterruptedPomodoros(timer.getCurrentTaskId()));
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::ALARM_DURATION:
            lv_label_set_text(menu_item_label, "Alarm Duration");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d sec", timer.getAlarmDuration());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::ALARM_VIBRATION:
            lv_label_set_text(menu_item_label, "Haptic Feedback");
            lv_label_set_text(menu_value_label, timer.getAlarmVibration() ? "ON" : "OFF");
            break;
        case MenuItem::ALARM_FLASH:
            lv_label_set_text(menu_item_label, "Alarm Flash");
            lv_label_set_text(menu_value_label, timer.getAlarmFlash() ? "ON" : "OFF");
            break;                        
        // case MenuItem::IDLE_TIMEOUT_MINUTES:
        //     lv_label_set_text(menu_item_label, "Idle timeout time (min)");
        //     {
        //         char val_str[16];
        //         snprintf(val_str, sizeof(val_str), "%d min", timer.getIdleTImeoutDuration());
        //         lv_label_set_text(menu_value_label, val_str);
        //     }
        //     break;
        case MenuItem::IDLE_TIMEOUT_BATTERY:
            lv_label_set_text(menu_item_label, "Idle Timeout (Battery)");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d min", timer.getIdleTimeoutBattery());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::IDLE_TIMEOUT_USB:
            lv_label_set_text(menu_item_label, "Idle Timeout (USB)");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d min", timer.getIdleTimeoutUSB());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::IDLE_SLEEP_ON_USB:
            lv_label_set_text(menu_item_label, "Sleep When on USB");
            lv_label_set_text(menu_value_label, timer.getSleepOnUSB() ? "ON" : "OFF");
            break;            
        case MenuItem::BRIGHTNESS:
            lv_label_set_text(menu_item_label, "Brightness");
            {
                char val_str[16];
                // Show as percentage or level number
                int percent = (timer.getBrightnessLevel() + 1) * 12.5;  // Convert 0-7 to ~12-100%
                snprintf(val_str, sizeof(val_str), "%d%%", percent);
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::THEME:
            lv_label_set_text(menu_item_label, "Theme");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "Theme %d", timer.getTheme());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::SCREEN_ORIENTATION:
            lv_label_set_text(menu_item_label, "Orientation");
            lv_label_set_text(menu_value_label, timer.getScreenFlipped() ? "Flipped" : "Normal");
            break;
        case MenuItem::ENABLE_WINDUP:
            lv_label_set_text(menu_item_label, "Wind-up Mode");
            lv_label_set_text(menu_value_label, timer.getWindupEnabled() ? "ON" : "OFF");
            break;
        default:
            break;
    }
    lv_obj_set_style_text_color(menu_item_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(menu_value_label, lv_color_hex(0x808080), 0);
    
} else if (timer.getMenuState() == MenuState::EDITING_VALUE) {
    // Set label text based on current menu item
    switch (timer.getCurrentMenuItem()) {
        case MenuItem::POMODORO_LENGTH:
            lv_label_set_text(menu_item_label, "Pomodoro Length");
            break;
        case MenuItem::SHORT_BREAK_LENGTH:
            lv_label_set_text(menu_item_label, "Short Break Length");
            break;
        case MenuItem::LONG_BREAK_LENGTH:
            lv_label_set_text(menu_item_label, "Long Break Length");
            break;
        case MenuItem::POMODOROS_BEFORE_LONG_BREAK:
            lv_label_set_text(menu_item_label, "Pomodoros Before Break");
            break;
        case MenuItem::LONG_BREAK_PROGRESS:
            lv_label_set_text(menu_item_label, "Long Break Progress");
            break;
        case MenuItem::MANAGE_TASKS:
            lv_label_set_text(menu_item_label, "Total Tasks");
            break;            
        case MenuItem::EDIT_COMPLETED_POMODOROS:
            {
                char label_str[32];
                snprintf(label_str, sizeof(label_str), "Task %d Completed", timer.getCurrentTaskId() + 1);
                lv_label_set_text(menu_item_label, label_str);
            }
            break;
        case MenuItem::EDIT_INTERRUPTED_POMODOROS:
            {
                char label_str[32];
                snprintf(label_str, sizeof(label_str), "Task %d Interrupted", timer.getCurrentTaskId() + 1);
                lv_label_set_text(menu_item_label, label_str);
            }
            break;
        case MenuItem::ALARM_DURATION:
            lv_label_set_text(menu_item_label, "Alarm Duration");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d sec", timer.getEditingValue());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::ALARM_VIBRATION:
            lv_label_set_text(menu_item_label, "Haptic Feedback");
            lv_label_set_text(menu_value_label, timer.getEditingValue() ? "ON" : "OFF");
            break;
        case MenuItem::ALARM_FLASH:
            lv_label_set_text(menu_item_label, "Alarm Flash");
            lv_label_set_text(menu_value_label, timer.getEditingValue() ? "ON" : "OFF");
            break;      
        // case MenuItem::IDLE_TIMEOUT_MINUTES:
        //     lv_label_set_text(menu_item_label, "Idle Timeout");
        //     break;
        case MenuItem::IDLE_TIMEOUT_BATTERY:
            lv_label_set_text(menu_item_label, "Battery Idle Timeout");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d min", timer.getEditingValue());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::IDLE_TIMEOUT_USB:
            lv_label_set_text(menu_item_label, "USB Idle Timeout");
            {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d min", timer.getEditingValue());
                lv_label_set_text(menu_value_label, val_str);
            }
            break;
        case MenuItem::IDLE_SLEEP_ON_USB:
            lv_label_set_text(menu_item_label, "Sleep on USB");
            lv_label_set_text(menu_value_label, timer.getEditingValue() ? "ON" : "OFF");
            break;            
        case MenuItem::BRIGHTNESS:
            lv_label_set_text(menu_item_label, "Brightness");
            break;
        case MenuItem::THEME:
            lv_label_set_text(menu_item_label, "Theme");
            break;
        case MenuItem::SCREEN_ORIENTATION:
            lv_label_set_text(menu_item_label, "Orientation");
            break;
        case MenuItem::ENABLE_WINDUP:
            lv_label_set_text(menu_item_label, "Wind-up Mode");
            lv_label_set_text(menu_value_label, timer.getEditingValue() ? "ON" : "OFF");
            break;
        default:
            break;
    }
    
    char val_str[16];
    // Special formatting for brightness
    if (timer.getCurrentMenuItem() == MenuItem::BRIGHTNESS) {
        int percent = (timer.getEditingValue() + 1) * 12.5;
        snprintf(val_str, sizeof(val_str), "%d%%", percent);
    } else if (timer.getCurrentMenuItem() == MenuItem::THEME) {
        snprintf(val_str, sizeof(val_str), "Theme %d", timer.getEditingValue());
    } else if (timer.getCurrentMenuItem() == MenuItem::MANAGE_TASKS) {
        snprintf(val_str, sizeof(val_str), "%d tasks", timer.getEditingValue());
    } else if (timer.getCurrentMenuItem() == MenuItem::POMODOROS_BEFORE_LONG_BREAK) {
        snprintf(val_str, sizeof(val_str), "%d", timer.getEditingValue());
    } else if (timer.getCurrentMenuItem() == MenuItem::LONG_BREAK_PROGRESS) {
        snprintf(val_str, sizeof(val_str), "%d / %d", timer.getEditingValue(),
                 timer.getPomodorosBeforeLongBreak());
    } else if (timer.getCurrentMenuItem() == MenuItem::EDIT_COMPLETED_POMODOROS || 
               timer.getCurrentMenuItem() == MenuItem::EDIT_INTERRUPTED_POMODOROS) {
        snprintf(val_str, sizeof(val_str), "%d", timer.getEditingValue());
    } else if (timer.getCurrentMenuItem() == MenuItem::ALARM_VIBRATION || 
               timer.getCurrentMenuItem() == MenuItem::ALARM_FLASH) {
        snprintf(val_str, sizeof(val_str), "%s", timer.getEditingValue() ? "ON" : "OFF");
    } else if (timer.getCurrentMenuItem() == MenuItem::SCREEN_ORIENTATION) {
        snprintf(val_str, sizeof(val_str), "%s", timer.getEditingValue() ? "Flipped" : "Normal");
    } else {
        snprintf(val_str, sizeof(val_str), "%d min", timer.getEditingValue());
    }    
    lv_label_set_text(menu_value_label, val_str);
    lv_obj_set_style_text_color(menu_item_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_color(menu_value_label, lv_color_hex(0x00FF00), 0);
}
}


void setup() {
  Serial.begin(115200);
  delay(100);

  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("Reset reason: %d\n", reason);


  // Initialize watchdog timer (30 second timeout)
  esp_task_wdt_init(30, true);  // 30 seconds, panic on timeout
  esp_task_wdt_add(NULL);       // Add current task to WDT
  Serial.println("Watchdog timer initialized (30s timeout)");

  // Check wake reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    Serial.println("Woke from deep sleep");
    gpio_reset_pin(GPIO_NUM_14);
    gpio_reset_pin((gpio_num_t)PIN_ENC_A);
    gpio_reset_pin((gpio_num_t)PIN_ENC_B);
    gpio_reset_pin((gpio_num_t)PIN_ENC_BTN);
    delay(100);
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    pinMode(PIN_ENC_BTN, INPUT_PULLUP);
    while (digitalRead(PIN_BUTTON_2) == LOW) delay(10);
    delay(100);
  }

  Serial.println("Starting setup...");
  setCpuFrequencyMhz(80);
  timer = TimerCore();

  // Initialize power
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  // Clear button states
  btn1.reset();
  btn2.reset();

  // Initialize TFT
  tft.init();
  apply_display_orientation();
  tft.fillScreen(TFT_BLACK);

  // Initialize backlight
  ledcSetup(LCD_BL_PWM_CHANNEL, 10000, 8);
  ledcAttachPin(PIN_LCD_BL, LCD_BL_PWM_CHANNEL);
  ledcWrite(LCD_BL_PWM_CHANNEL, BRIGHTNESS_VALUES[timer.getBrightnessLevel()]);

  // Initialize LVGL
  lv_init();
  lv_disp_buf = (lv_color_t *)heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t),
                                               MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  
  lv_disp_draw_buf_init(&disp_buf, lv_disp_buf, NULL, LVGL_LCD_BUF_SIZE);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 320;
  disp_drv.ver_res = 170;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &disp_buf;
  lv_disp_drv_register(&disp_drv);

  // ============================================================================
  // EC11 ENCODER SETUP - OPTIMIZED
  // ============================================================================
  Serial.println("Initializing EC11 encoder...");
  
  // Create encoder with FOUR3 mode (best for EC11)
  encoder = new RotaryEncoder(PIN_ENC_A, PIN_ENC_B, RotaryEncoder::LatchMode::FOUR3);
  
  // Set initial position
  encoder->setPosition(0);
  encoder_position = 0;
  delay(10);
  
  // Attach interrupts to both encoder pins
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), checkPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), checkPosition, CHANGE);
  
  // Setup encoder button
  pinMode(PIN_ENC_BTN, INPUT_PULLUP);
  
  Serial.println("EC11 encoder ready!");
  // ============================================================================

  // Setup buttons
  btn1.begin(PIN_BUTTON_1);
  btn1.setLongClickTime(2000);
  btn1.setDoubleClickTime(400);

  btn2.begin(PIN_BUTTON_2);
  btn2.setLongClickTime(2000);

  btn1.setClickHandler([](Button2 &b) { handle_button1_click(); });
  btn1.setLongClickDetectedHandler([](Button2 &b) { handle_button1_longpress(); });
  btn2.setClickHandler([](Button2 &b) { handle_button2_click(); });
  btn2.setLongClickDetectedHandler([](Button2 &b) { handle_button2_longpress(); });

  // Setup vibration
  pinMode(PIN_VIBRATION, OUTPUT);
  digitalWrite(PIN_VIBRATION, LOW);

  // Initialize ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Setup screen
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
  
  Serial.println("Setup complete!");
}

void loop() {
  static uint32_t last_lvgl_tick = 0;
  static uint32_t last_ui_update = 0;
  uint32_t now = millis();
  
  // Read battery voltage
  static uint32_t last_voltage_check = 0;
  if (millis() - last_voltage_check > BATTERY_CHECK_INTERVAL_MS) {
    current_battery_voltage = analogRead(PIN_BAT_VOLT) * 3.3 / 4095.0 * 2.0;
    last_voltage_check = millis();
  }


  // Check idle timeout with voltage reading
  if (timer.checkIdleTimeout(current_battery_voltage)) {
    Serial.println("Idle timeout - entering sleep");
    display_sleep_message();
    delay(1000);
    enter_deep_sleep();
    return;
  }


  // --- LVGL tick at 20 ms ---
  if (now - last_lvgl_tick >= LVGL_TICK_MS) {
    lv_tick_inc(LVGL_TICK_MS);
    lv_timer_handler();
    last_lvgl_tick = now;
  }

  // --- UI / timer update at lower rate ---
  const uint32_t UI_INTERVAL_ACTIVE = 100;    // 20 Hz when running
  const uint32_t UI_INTERVAL_IDLE   = 250;   // 4 Hz when idle

  uint32_t interval = (timer.getState() == TimerState::IDLE) 
                        ? UI_INTERVAL_IDLE 
                        : UI_INTERVAL_ACTIVE;

  if (now - last_ui_update >= interval) {
    timer.update();
    update_cpu_frequency();
    update_display();
    update_brightness();
    last_ui_update = now;
  }


  // Button handling
  btn1.loop();
  btn2.loop();

  
  // ============================================================================
  // ENCODER PROCESSING - Clean and efficient
  // ============================================================================
  if (encoder_changed) {
    encoder_changed = false;
    
    long new_position = encoder->getPosition();
    
    if (new_position != encoder_position) {
      if (timer.getState() == TimerState::STARTING) {
        encoder_position = new_position;
        return;
      }
      int8_t direction = (new_position > encoder_position) ? -1 : 1;
      if (timer.getScreenFlipped()) {
        direction = -direction;
      }
      encoder_position = new_position;
      
      timer.resetIdleTimer();
      
      // Handle encoder rotation based on state
      if (timer.getState() == TimerState::WIND_UP) {
        timer.incrementWindup(direction);
        
      } else if (timer.getMenuState() == MenuState::CLOSED) {
        // Task selection
        uint8_t current = timer.getCurrentTaskId();
        uint8_t total = timer.getTotalTasks();

        // ADD THESE DEBUG PRINTS:
        Serial.printf("ENCODER: State=%d, Current=%d, Total=%d, Direction=%d\n", 
                      timer.getState(), current, total, direction);
        
        if (current >= total) {
            Serial.println("ERROR: Current task >= total tasks!");
            timer.selectTask(0);  // Force to valid task
            return;
        }
        
        
        if (direction < 0 && current < total - 1) {
          timer.selectTask(current + 1);
          if (task_list != nullptr && !lv_obj_has_flag(task_list, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_scroll_to_y(task_list, (current + 1) * 20, LV_ANIM_OFF);  // ← NO ANIMATION
          }
        } else if (direction > 0 && current > 0) {
          timer.selectTask(current - 1);
          if (task_list != nullptr && !lv_obj_has_flag(task_list, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_scroll_to_y(task_list, (current - 1) * 20, LV_ANIM_OFF);  // ← NO ANIMATION
          }
        }

        
      } else if (timer.getMenuState() == MenuState::MENU_LIST) {
        timer.navigateMenu(direction);
        
      } else if (timer.getMenuState() == MenuState::EDITING_VALUE) {
        timer.adjustValue(direction);
        if (timer.getCurrentMenuItem() == MenuItem::BRIGHTNESS) {
          ledcWrite(LCD_BL_PWM_CHANNEL, BRIGHTNESS_VALUES[timer.getEditingValue()]);
        }
      }

    }
  }
  
  // ============================================================================
  // ENCODER BUTTON HANDLING - NON-BLOCKING VERSION
  // ============================================================================
  static bool last_enc_button = HIGH;
  static uint32_t last_button_change = 0;
  const uint32_t DEBOUNCE_MS = 50;

  bool current_enc_button = digitalRead(PIN_ENC_BTN);

  if (timer.getState() == TimerState::STARTING) {
    last_enc_button = current_enc_button;
    last_button_change = millis();
  } else {
    // Only process changes after debounce period has elapsed
    if (current_enc_button != last_enc_button && 
        millis() - last_button_change >= DEBOUNCE_MS) {
      
      last_button_change = millis();  // Record time of this change
      
      if (current_enc_button == LOW) {
        // Button pressed
        timer.resetIdleTimer();
        encoderButtonPressTime = millis();
        encoderButtonLongPressHandled = false;
      } else {
        // Button released
        unsigned long duration = millis() - encoderButtonPressTime;
        
        if (!encoderButtonLongPressHandled && duration < 500) {
          // Short press
          if (timer.getMenuState() == MenuState::CLOSED) {
            timer.addTask();
          } else if (timer.getMenuState() == MenuState::MENU_LIST) {
            timer.selectMenuItem();
          } else if (timer.getMenuState() == MenuState::EDITING_VALUE) {
            timer.confirmValue();
          }
        }
      }
      
      last_enc_button = current_enc_button;  // Update last state
    }
  }


  
  // Check for long press
  if (timer.getState() != TimerState::STARTING && current_enc_button == LOW && !encoderButtonLongPressHandled) {
    if (millis() - encoderButtonPressTime >= 500) {
      encoderButtonLongPressHandled = true;
      
      if (timer.getMenuState() == MenuState::CLOSED) {
        timer.openMenu();
      } else {
        timer.closeMenu();
      }
    }
  }
  

  
  // Feed the watchdog (reset timer)
  esp_task_wdt_reset();  
}
