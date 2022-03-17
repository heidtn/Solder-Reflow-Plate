/* Solder Reflow Plate Sketch
 *  H/W - Ver 2.4
 *  S/W - Ver 1.0
 *  by Chris Halsall and Nathan Heidt     */

/* To prepare
 * 1) Install MiniCore in additional boards; (copy into File->Preferences->Additional Boards Manager
 * URLs) https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json 2) Then add MiniCore
 * by searching and installing (Tools->Board->Board Manager) 3) Install Adafruit_GFX and
 * Adafruit_SSD1306 libraries (Tools->Manage Libraries)
 */

/* To program
 *  1) Select the following settings under (Tools)
 *      Board->Minicore->Atmega328
 *      Clock->Internal 8MHz
 *      BOD->BOD 2.7V
 *      EEPROM->EEPROM retained
 *      Compiler LTO->LTO Disabled
 *      Variant->328P / 328PA
 *      Bootloader->No bootloader
 *  2) Set programmer of choice, e.g.'Arduino as ISP (MiniCore)', 'USB ASP', etc, and set correct
 * port. 3) Burn bootloader (to set fuses correctly) 4) Compile and upload
 *
 *
 * TODOS:
 * - add digital sensor setup routine if they are detected, but not setup
 * - figure out a method for how to use all the temperature sensors
 * - implement an observer/predictor for the temperature sensors.  Kalman filter time?!?
 */

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <SPI.h>

// Version Definitions
static const PROGMEM float hw = 2.4;
static const PROGMEM float sw = 2.0;

// Screen Definitions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define SCREEN_ADDRESS 0x3C                                       // I2C Address
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // Create Display

// Pin Definitions
#define MOSFET_PIN 3
#define UPSW_PIN 6
#define DNSW_PIN 5
#define TEMP_PIN 16 // A2
#define VCC_PIN 14  // A0

enum menu_state_t { MENU_IDLE, MENU_SELECT_PROFILE, MENU_HEAT, MENU_INC_TEMP, MENU_DEC_TEMP };
enum buttons_state_t { BUTTONS_NO_PRESS, BUTTONS_BOTH_PRESS, BUTTONS_UP_PRESS, BUTTONS_DN_PRESS };

// Temperature Info
byte max_temp_array[] = {140, 150, 160, 170, 180};
byte max_temp_index = 0;
byte tempIndexAddr = 1;

// Voltage Measurement Info
float vConvert = 52.00;
float vMin = 10.50;

// Solder Reflow Plate Logo
static const uint8_t PROGMEM logo[] = {
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x31, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x1f, 0xe0, 0x03, 0x01, 0x80, 0x00, 0x00, 0x30, 0x70, 0x00, 0x21, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x20, 0x03, 0x00, 0xc7, 0x80, 0x00, 0x20, 0x18, 0xf0, 0x61, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x00, 0x03, 0x3e, 0xcc, 0xc0, 0xc0, 0x04, 0x19, 0x98, 0x61, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x1c, 0x01, 0xf3, 0x77, 0xd8, 0xc7, 0xe0, 0x06, 0x33, 0x18, 0x61, 0x8f, 0x88, 0x00, 0x00, 0x00,
    0x06, 0x03, 0x3b, 0x61, 0xd0, 0xc6, 0x00, 0x07, 0xe2, 0x18, 0x61, 0x98, 0xd8, 0x04, 0x00, 0x00,
    0x01, 0xc6, 0x0b, 0x60, 0xd9, 0x86, 0x00, 0x06, 0x03, 0x30, 0xff, 0xb0, 0x78, 0x66, 0x00, 0x00,
    0x40, 0xe4, 0x0f, 0x60, 0xdf, 0x06, 0x00, 0x07, 0x03, 0xe0, 0x31, 0xe0, 0x78, 0x62, 0x00, 0x00,
    0x40, 0x3c, 0x0f, 0x61, 0xd8, 0x06, 0x00, 0x07, 0x83, 0x00, 0x31, 0xe0, 0x78, 0x63, 0x00, 0x00,
    0x60, 0x36, 0x1b, 0x63, 0xc8, 0x02, 0x00, 0x02, 0xc1, 0x00, 0x18, 0xb0, 0xcc, 0xe2, 0x00, 0x00,
    0x30, 0x33, 0x3b, 0x36, 0x4e, 0x03, 0x00, 0x02, 0x61, 0xc0, 0x0c, 0x99, 0xcd, 0xfe, 0x00, 0x00,
    0x0f, 0xe1, 0xe1, 0x3c, 0x03, 0xf3, 0x00, 0x02, 0x38, 0x7e, 0x0c, 0x8f, 0x07, 0x9c, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7f, 0x84, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xc0, 0xe4, 0x00, 0x18, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x3c, 0x3c, 0x18, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x1e, 0x06, 0x7f, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x3e, 0x03, 0x18, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x36, 0x7f, 0x19, 0x8c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0xe6, 0xc7, 0x19, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x06, 0x07, 0x83, 0x18, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x06, 0x07, 0x81, 0x18, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x06, 0x06, 0xc3, 0x98, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x04, 0x7e, 0x08, 0x3f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t logo_width = 128;
static const uint8_t logo_height = 27;

// Heating Animation
static const uint8_t PROGMEM heat_animate[] = {
    0b00000001, 0b00000000, 0b00000001, 0b10000000, 0b00000001, 0b10000000, 0b00000001, 0b01000000,
    0b00000010, 0b01000000, 0b00100010, 0b01000100, 0b00100100, 0b00100100, 0b01010101, 0b00100110,
    0b01001001, 0b10010110, 0b10000010, 0b10001001, 0b10100100, 0b01000001, 0b10011000, 0b01010010,
    0b01000100, 0b01100010, 0b00100011, 0b10000100, 0b00011000, 0b00011000, 0b00000111, 0b11100000};
static const uint8_t heat_animate_width = 16;
static const uint8_t heat_animate_height = 16;

// Tick
static const uint8_t PROGMEM tick[] = {
    0b00000000, 0b00000100, 0b00000000, 0b00001010, 0b00000000, 0b00010101, 0b00000000, 0b00101010,
    0b00000000, 0b01010100, 0b00000000, 0b10101000, 0b00000001, 0b01010000, 0b00100010, 0b10100000,
    0b01010101, 0b01000000, 0b10101010, 0b10000000, 0b01010101, 0b00000000, 0b00101010, 0b00000000,
    0b00010100, 0b00000000, 0b00001000, 0b00000000, 0b01111111, 0b11100000};
static const uint8_t tick_width = 16;
static const uint8_t tick_height = 15;

// This needs to be specified or the compiler will fail as you can't initialize a
// flexible array member in a nested context
#define MAX_PROFILE_LENGTH 8

// TODO(HEIDT) may need to switch away from floats for speed/sizeA
struct solder_profile_t {
    uint8_t points;
    float seconds[MAX_PROFILE_LENGTH];
    float fraction[MAX_PROFILE_LENGTH];
};

// TODO(HEIDT) how to adjust for environments where the board starts hot or cold?
// profiles pulled from here: https://www.7pcb.com/blog/lead-free-reflow-profile.php
const static solder_profile_t PROGMEM profiles[] = {
    {4, {25, 105, 115, 155}, {65.0, 78.0, 81.0, 100.0}}, {2, {162.0, 202.0}, {95.0, 100.0}}};

// temperature must be within this range to move on to next step
#define TARGET_TEMP_THRESHOLD 5.0

// PID values
float kI = 0.1;
float kD = 1.0;
float kP = 1.0;
float I_clip = 50;
float error_I = 0;

// Optional temperature sensor
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
int sensor_count = 0;
DeviceAddress addresses[3];
/
    // -------------------- Function prototypes -----------------------------------
    void inline heatAnimate(int &x, int &y, float v, float t);

// -------------------- Function definitions ----------------------------------

void setup() {

    // Pin Direction control
    pinMode(MOSFET_PIN, OUTPUT);
    digitalWrite(MOSFET_PIN, LOW);
    pinMode(UPSW_PIN, INPUT);
    pinMode(DNSW_PIN, INPUT);
    pinMode(TEMP_PIN, INPUT);
    pinMode(VCC_PIN, INPUT);

    // Pull saved values from EEPROM
    max_temp_index = EEPROM.read(tempIndexAddr) % sizeof(max_temp_array);

    // Enable Fast PWM with no prescaler
    setFastPwm();

    // Start-up Diplay
    showLogo();

    // Go to main menu
    mainMenu();

    // check onewire TEMP_PIN sensors
    setupSensors();
}

inline void setupSensors() {
    sensor_count = sensors.getDeviceCount();
    for (int i = 0; i < min(sensor_count, sizeof(addresses)); i++) {
        addresses[i] = sensors.search(addresses[i]);
    }
}

inline void setFastPwm() {
    TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
    TCCR2B = _BV(CS20);
}

void showLogo() {
    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.drawBitmap(0, 0, logo, logo_width, logo_height, SSD1306_WHITE);
    display.setCursor(80, 16);
    display.print(F("S/W V"));
    display.print(sw, 1);
    display.setCursor(80, 24);
    display.print(F("H/W V"));
    display.print(hw, 1);
    display.display();
    delay(3000);
}

void mainMenu() {
    // Debounce
    while (!digitalRead(UPSW_PIN) || !digitalRead(DNSW_PIN))
        ;
    menu_state_t cur_state = MENU_IDLE;

    int x = 0;   // Display change counter
    int y = 200; // Display change max (modulused below)
    uint8_t profile_index = 0;

    while (1) {
        switch (cur_state) {
        case MENU_IDLE: {
            analogWrite(MOSFET_PIN, 0); // Ensure MOSFET off
            clearMainMenu();
            buttons_state_t cur_button = getButtonsState();

            if (cur_button == BUTTONS_BOTH_PRESS) {
                cur_state = MENU_SELECT_PROFILE;
            } else if (cur_button == BUTTONS_UP_PRESS) {
                cur_state = MENU_INC_TEMP;
            } else if (cur_button == BUTTONS_DN_PRESS) {
                cur_state = MENU_DEC_TEMP;
            }
        } break;
        case MENU_SELECT_PROFILE: {
            profile_index = getProfile();
            cur_state = MENU_HEAT;
        } break;
        case MENU_HEAT: {
            if (!heat(max_temp_array[max_temp_index], profile_index)) {
                cancelledPB();
            } else {
                coolDown();
                completed();
            }
            cur_state = MENU_IDLE;
        } break;
        case MENU_INC_TEMP: {
            if (max_temp_index < sizeof(max_temp_array) - 1) {
                max_temp_index++;
                EEPROM.update(tempIndexAddr, max_temp_index);
            }
            cur_state = MENU_IDLE;
        } break;
        case MENU_DEC_TEMP: {
            if (max_temp_index > 0) {
                max_temp_index--;
                EEPROM.update(tempIndexAddr, max_temp_index);
            }
            cur_state = MENU_IDLE;
        } break;
        }

        // Change Display (left-side)
        showMainMenuLeft(x, y);

        // Update Display (right-side)
        showMainMenuRight();
    }
}

// TODO(HEIDT) change to a down-up model or we'll loop forever in these cases
buttons_state_t getButtonsState() {
    if (digitalRead(UPSW_PIN) && digitalRead(DNSW_PIN)) {
        return BUTTONS_NO_PRESS;
    } else {
        delay(100);
        if (!digitalRead(UPSW_PIN) && !digitalRead(DNSW_PIN)) {
            return BUTTONS_BOTH_PRESS;
        } else if (digitalRead(UPSW_PIN)) {
            return BUTTONS_UP_PRESS;
        } else {
            return BUTTONS_DN_PRESS;
        }
    }

    // wait for up on both switches
    // TODO(HEIDT) holding down switches will block execution flow, address
    while (!digitalRead(UPSW_PIN) || !digitalRead(DNSW_PIN))
        ;
}

inline int8_t getProfile() {
    int8_t cur_profile = 0;
    while (1) {
        clearMainMenu();
        display.setCursor(3, 4);
        display.print(F("Pick profile"));
        display.drawLine(3, 12, 79, 12, SSD1306_WHITE);
        display.setCursor(3, 14);
        display.print(F(" UP/DN: cycle"));
        display.setCursor(3, 22);
        display.print(F(" DN: choose"));
        buttons_state_t cur_button = getButtonsState();
        if (cur_button == BUTTONS_BOTH_PRESS) {
            clearMainMenu();
            return cur_profile;
        } else if (cur_button == BUTTONS_DN_PRESS) {
            cur_profile--;
        } else if (cur_button == BUTTONS_UP_PRESS) {
            cur_profile++;
        }
        cur_profile %= sizeof(profiles);
        displayProfileRight(cur_profile);
    }
}

inline void displayProfileRight(int8_t cur_profile) {
    int cur_x = 90;
    int cur_y = 30;
    // start at x=90, go to SCREEN_WIDTH-8, save 6 pixels for cooldown
    int x_dist = SCREEN_WIDTH - 90 - 8;
    display.setCursor(cur_x, cur_y);
    int total_seconds = (int)profiles[cur_profile].seconds[profiles[cur_profile].points - 1];

    for (int i = 0; i < profiles[cur_profile].points; i++) {
        int x_next = (int)((profiles[cur_profile].seconds[i] / total_seconds) * x_dist) + 90;
        int y_next = 30 - (int)(profiles[cur_profile].fraction[i] * 28);
        display.drawLine(cur_x, cur_y, x_next, y_next, SSD1306_WHITE);
        cur_x = x_next;
        cur_y = y_next;
    }
    // draw down to finish TEMP_PIN
    display.drawLine(cur_x, cur_y, SCREEN_WIDTH - 2, 30, SSD1306_WHITE);
}

inline void clearMainMenu() {
    display.clearDisplay();
    display.setTextSize(1);
    display.drawRoundRect(0, 0, 83, 32, 2, SSD1306_WHITE);
}

inline void showMainMenuLeft(int &x, int &y) {
    if (x < (y * 0.5)) {
        display.setCursor(3, 4);
        display.print(F("PRESS BUTTONS"));
        display.drawLine(3, 12, 79, 12, SSD1306_WHITE);
        display.setCursor(3, 14);
        display.print(F(" Change  MAX"));
        display.setCursor(3, 22);
        display.print(F(" Temperature"));
    } else {
        display.setCursor(3, 4);
        display.print(F("HOLD  BUTTONS"));
        display.drawLine(3, 12, 79, 12, SSD1306_WHITE);
        display.setCursor(3, 18);
        display.print(F("Begin Heating"));
    }
    x = (x + 1) % y; // Display change increment and modulus
}

inline void showMainMenuRight() {
    display.setCursor(95, 6);
    display.print(F("TEMP"));
    display.setCursor(95, 18);
    display.print(max_temp_array[max_temp_index]);
    display.print(F("C"));
    display.display();
}

inline void showHeatMenu(byte max_temp) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(22, 4);
    display.print(F("HEATING"));
    display.setTextSize(1);
    display.setCursor(52, 24);
    display.print(max_temp);
    display.print(F("C"));
    display.display();
}

bool heat(byte max_temp, int profile_index) {
    // Debounce
    while (!digitalRead(UPSW_PIN) || !digitalRead(DNSW_PIN)) {
    }

    // Heating Display
    showHeatMenu(max_temp);
    delay(3000);

    float t; // Used to store current temperature
    float v; // Used to store current voltage

    unsigned long profile_max_time = millis() / 1000 + (8 * 60);
    unsigned long step_start_time = (millis() / 1000);
    int current_step = 0;

    // Other control variables
    int x = 0;  // Heat Animate Counter
    int y = 80; // Heat Animate max (modulused below)

    float start_temp = getTemp();
    float goal_temp = profiles[profile_index].fraction[0] * max_temp;
    float step_runtime = profiles[profile_index].seconds[0];
    float last_time = step_start_time;
    float last_temp = getTemp();
    error_I = 0;

    while (1) {
        // Cancel heat
        if (getButtonsState() != BUTTONS_NO_PRESS) {
            analogWrite(MOSFET_PIN, 0);
            return 0;
        }

        // Check Heating not taken more than 8 minutes
        if (millis() / 1000 > profile_max_time) {
            analogWrite(MOSFET_PIN, 0);
            cancelledTimer();
            return 0;
        }

        // Measure Values
        t = getTemp();
        v = getVolts();

        // Determine what target temp is and PID to it
        float time_into_step = ((float)millis() / 1000.0) - (float)step_start_time;
        float target_temp = min(
            ((goal_temp - start_temp) * (time_into_step / step_runtime)) + start_temp, goal_temp);
            
        // TODO(HEIDT) PID for a ramp will always lag, other options may be better
        stepPID(target_temp, t, last_temp, time_into_step - last_time);
        last_time = time_into_step;

        // if we finish the step timewise
        if (time_into_step >= step_runtime) {
            // and if we're within the goal temperature of the step
            if (abs(t - goal_temp) < TARGET_TEMP_THRESHOLD) {
                // move onto the next step in the profile
                current_step++;
                // if that was the last step, we're done!
                if (current_step == profiles[profile_index].points) {
                    analogWrite(MOSFET_PIN, 0);
                    return 1;
                }
                // otherwise, get the next goal temperature and runtime, and do the process again
                start_temp = t;
                goal_temp = profiles[profile_index].fraction[current_step];
                step_runtime = profiles[profile_index].fraction[current_step] -
                               profiles[profile_index].fraction[current_step - 1];
                step_start_time = millis() / 1000.0;
            }
        }

        heatAnimate(x, y, v, t);
    }
}

void stepPID(float target_temp, float current_temp, float last_temp, float dt) {
    float error = target_temp - current_temp;
    float D = (current_temp - last_temp) / dt;

    error_I += error * dt;
    if (error_I > I_clip) {
        error_I = I_clip;
    } else if (error_I < -1 * I_clip) {
        error_I = -1 * I_clip;
    }

    float PWM = error * kP + D * kD + error_I * kI;
    if (PWM > 200) {
        PWM = 200;
    }
    if (PWM < 0) {
        PWM = 0;
    }
    analogWrite(MOSFET_PIN, (int)PWM);
}

void inline heatAnimate(int &x, int &y, float v, float t) {
    // Heat Animate Control
    display.clearDisplay();
    display.drawBitmap(0, 3, heat_animate, heat_animate_width, heat_animate_height, SSD1306_WHITE);
    display.drawBitmap(112, 3, heat_animate, heat_animate_width, heat_animate_height,
                       SSD1306_WHITE);
    display.fillRect(0, 3, heat_animate_width, heat_animate_height * (y - x) / y, SSD1306_BLACK);
    display.fillRect(112, 3, heat_animate_width, heat_animate_height * (y - x) / y, SSD1306_BLACK);
    x = (x + 1) % y; // Heat animate increment and modulus

    // Update display
    display.setTextSize(2);
    display.setCursor(22, 4);
    display.print(F("HEATING"));
    display.setTextSize(1);
    display.setCursor(20, 24);
    display.print(F("~"));
    display.print(v, 1);
    display.print(F("V"));
    if (t >= 100) {
        display.setCursor(78, 24);
    } else if (t >= 10) {
        display.setCursor(81, 24);
    } else {
        display.setCursor(84, 24);
    }
    display.print(F("~"));
    display.print(t, 0);
    display.print(F("C"));
    display.display();
}

void cancelledPB() { // Cancelled via push button
    // Debounce
    while (!digitalRead(UPSW_PIN) || !digitalRead(DNSW_PIN)) {
    }

    // Update Display
    display.clearDisplay();
    display.drawRoundRect(22, 0, 84, 32, 2, SSD1306_WHITE);
    display.setCursor(25, 4);
    display.print(F("  CANCELLED"));
    display.drawLine(25, 12, 103, 12, SSD1306_WHITE);
    display.setCursor(25, 14);
    display.println(" Push button");
    display.setCursor(25, 22);
    display.println("  to return");
    display.setTextSize(3);
    display.setCursor(5, 4);
    display.print(F("!"));
    display.setTextSize(3);
    display.setCursor(108, 4);
    display.print(F("!"));
    display.setTextSize(1);
    display.display();
    delay(50);

    // Wait to return on any button press
    while (getButtonsState() == BUTTONS_NO_PRESS)
        ;
}

void cancelledTimer() { // Cancelled via 5 minute Time Limit
    // Debounce
    while (!digitalRead(UPSW_PIN) || !digitalRead(DNSW_PIN)) {
    }

    // Initiate Swap Display
    int x = 0;   // Display change counter
    int y = 150; // Display change max (modulused below)

    // Wait to return on any button press
    while (getButtonsState() == BUTTONS_NO_PRESS) {
        // Update Display
        display.clearDisplay();
        display.drawRoundRect(22, 0, 84, 32, 2, SSD1306_WHITE);
        display.setCursor(25, 4);
        display.print(F("  TIMED OUT"));
        display.drawLine(25, 12, 103, 12, SSD1306_WHITE);

        // Swap Main Text
        if (x < (y * 0.3)) {
            display.setCursor(25, 14);
            display.println(" Took longer");
            display.setCursor(25, 22);
            display.println(" than 5 mins");
        } else if (x < (y * 0.6)) {
            display.setCursor(28, 14);
            display.println("Try a higher");
            display.setCursor(25, 22);
            display.println(" current PSU");
        } else {
            display.setCursor(25, 14);
            display.println(" Push button");
            display.setCursor(25, 22);
            display.println("  to return");
        }
        x = (x + 1) % y; // Display change increment and modulus

        display.setTextSize(3);
        display.setCursor(5, 4);
        display.print(F("!"));
        display.setTextSize(3);
        display.setCursor(108, 4);
        display.print(F("!"));
        display.setTextSize(1);
        display.display();
        delay(50);
    }
}

void coolDown() {
    // Debounce
    while (!digitalRead(UPSW_PIN) || !digitalRead(DNSW_PIN)) {
    }

    float t = getTemp(); // Used to store current temperature

    // Wait to return on any button press, or TEMP_PIN below threshold
    while (digitalRead(UPSW_PIN) && digitalRead(DNSW_PIN) && t > 45.00) {
        display.clearDisplay();
        display.drawRoundRect(22, 0, 84, 32, 2, SSD1306_WHITE);
        display.setCursor(25, 4);
        display.print(F("  COOL DOWN"));
        display.drawLine(25, 12, 103, 12, SSD1306_WHITE);
        display.setCursor(25, 14);
        display.println("  Still Hot");
        t = getTemp();
        if (t >= 100) {
            display.setCursor(49, 22);
        } else {
            display.setCursor(52, 22);
        }
        display.print(F("~"));
        display.print(t, 0);
        display.print(F("C"));
        display.setTextSize(3);
        display.setCursor(5, 4);
        display.print(F("!"));
        display.setTextSize(3);
        display.setCursor(108, 4);
        display.print(F("!"));
        display.setTextSize(1);
        display.display();
    }
}

void completed() {
    // Debounce
    while (!digitalRead(UPSW_PIN) || !digitalRead(DNSW_PIN)) {
    }

    // Update Display
    display.clearDisplay();
    display.drawRoundRect(22, 0, 84, 32, 2, SSD1306_WHITE);
    display.setCursor(25, 4);
    display.print(F("  COMPLETED  "));
    display.drawLine(25, 12, 103, 12, SSD1306_WHITE);
    display.setCursor(25, 14);
    display.println(" Push button");
    display.setCursor(25, 22);
    display.println("  to return");
    display.drawBitmap(0, 9, tick, tick_width, tick_height, SSD1306_WHITE);
    display.drawBitmap(112, 9, tick, tick_width, tick_height, SSD1306_WHITE);
    display.display();

    // Wait to return on any button press
    while (digitalRead(UPSW_PIN) && digitalRead(DNSW_PIN)) {
    }
}

float getTemp() {
    float t = 0;
    for (byte i = 0; i < 100; i++) { // Poll TEMP_PIN reading 100 times
        t = t + analogRead(TEMP_PIN);
    }
    t = ((t / 100) * -1.46) + 434;

    // return the maximum of all the temperature sensors for safety
    for (int i = 0; i < sensor_count; i++) {
        t = max(t, sensors.getTempC(addresses[i]));
    }
    return t;
}

float getVolts() {
    float v = 0;
    for (byte i = 0; i < 20; i++) { // Poll Voltage reading 20 times
        v = v + analogRead(VCC_PIN);
    }
    return v / 20 / vConvert; // Average, convert to V, and return
}

void loop() {
    // Not used
}
