#include <Adafruit_MCP4728.h>
#include <Button2.h>
#include <Rotary.h>
#include <SPI.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiAvrI2c.h>
#include <Wire.h>

#define SCREEN_ADDRESS 0x3C
#define DAC_ADDRESS 0x60

#define ROTARY_PIN1 2
#define ROTARY_PIN2 3
#define BUTTON_PIN 4

#define CLICKS_PER_STEP 4

#define MSTATE_IDLE 0
#define MSTATE_CHAN 1
#define MSTATE_CONFIG 2
#define MSTATE_EDIT 3

#define MAX_CHAN 4
#define MAX_CONFIG 6

#define CONFIG_MODIFIER 0
#define CONFIG_DELAY 1
#define CONFIG_LEVEL 2
#define CONFIG_WIDTH 3
#define CONFIG_PROBABILITY 4
#define CONFIG_WAVE 5

#define BTN_LONGCLICK_MS 800

int STEP_LEN = 20;

// Config & timing states
int ch_millistep[MAX_CHAN] = {0, 0, 0, 0};
unsigned long ch_nextmillis[MAX_CHAN] = {0, 0, 0, 0};
int ch_values[MAX_CHAN] = {0, 0, 0, 0};

#define MAX_MOD_VALUES 29
float mod_values[MAX_MOD_VALUES] = {-128, -64,  -32,  -16,  -8, -4,  -3,  -2,  -1.8, -1.5,
                        -1.4, -1.3, -1.2, -1.1, 1,  1.1, 1.2, 1.3, 1.4,  1.5,
                        1.8,  2,    3,    4,    8,  16,  32,  64,  128};

int bpm = 120;

bool isPlaying = true;

Rotary r;
Button2 b;

SSD1306AsciiAvrI2c oled;

Adafruit_MCP4728 mcp;

int menu_state = MSTATE_CHAN;
int s_chan = 0;
int s_conf = 0;

unsigned long last_click = 0;

String CONFIGS[MAX_CONFIG] = {"Modifier", "Delay",       "Level",
                              "Width",    "Probability", "Wave"};
// modifier moet veel lager kunnen
// width - width of the gate
// Delay - make it a percentage
// how could one program a shuffle?
// probability- percentage
// wave 0 check this out:
// https://github.com/robertgallup/arduino-DualLFO/blob/main/DUAL_LFO/DUAL_LFO.ino

float C_VALUES[MAX_CHAN][MAX_CONFIG] = {{1, 0, 100, 50, 100, 0},
                                            {1, 0, 100, 50, 100, 0},
                                            {1, 0, 100, 50, 100, 0},
                                            {1, 0, 100, 50, 100, 0}};

// Update the menu UI upon a menu state change
void handleMenuStateChange() {
  switch (menu_state) {
  case MSTATE_IDLE:
    oled.clear();
    oled.print(F("idle"));
    r.resetPosition(0);
    break;
  case MSTATE_CHAN:
    oled.clear();
    oled.println(F("Channel: "));
    r.resetPosition(0);
    break;
  case MSTATE_CONFIG:
    oled.clear(80, 90, 1, 2);
    r.resetPosition(0);
    break;
  case MSTATE_EDIT:
    oled.clear(2, 80, 1, 2);
    oled.setCursor(0, 2);
    oled.print(CONFIGS[s_conf]);

    if (CONFIGS[s_conf] == "Modifier") {
      r.resetPosition(getPositionFromModifier(C_VALUES[s_chan][s_conf]));
    } else {
      int prevPos = C_VALUES[s_chan][s_conf];
      r.resetPosition(prevPos);
    }
    break;
  }

  updateMenu();
}

void updateMenu() {
  int p = r.getPosition();

  switch (menu_state) {
  case MSTATE_CHAN:
    s_chan = p % MAX_CHAN;
    oled.clear(80, 128, 1, 2);
    oled.setCursor(80, 0);
    oled.print("> ");
    oled.print(s_chan);
    break;

  case MSTATE_CONFIG:
    s_conf = p % MAX_CONFIG;
    oled.clear(0, 128, 2, 3);
    oled.setCursor(0, 2);
    oled.print("> ");
    oled.print(CONFIGS[s_conf]);
    oled.setCursor(80, 2);
    displayConfigValue();
    break;
  case MSTATE_EDIT:
    setNewConfigValue(p);

    oled.setCursor(80, 2);
    oled.clear(80, 128, 2, 3);
    oled.print("> ");
    displayConfigValue();

    break;
  }
}

void printAllValues(int channel) {
  Serial.print("printAllValues., Channel: ");
  Serial.println(channel);
  for (size_t i = 0; i < MAX_CONFIG; i++) {
    Serial.print(CONFIGS[i]);
    Serial.print(" : ");
    Serial.print(C_VALUES[channel][i]);
    Serial.print(" - ");
  }
  Serial.println();
}

void displayConfigValue() {
  if (CONFIGS[s_conf] == "Modifier") {
    oled.print(C_VALUES[s_chan][s_conf] < 0 ? "/ " : "X ");
    oled.print(abs(C_VALUES[s_chan][s_conf]));

  } else if (CONFIGS[s_conf] == "Wave") {
    oled.print(C_VALUES[s_chan][s_conf]);
  } else {
    int v = C_VALUES[s_chan][s_conf];
    oled.print(v);
    oled.print("%");
  }
}

// apply the new value to the channel
void setNewMillistep(int channel) {
  float mod = C_VALUES[channel][CONFIG_MODIFIER];
  float part = 60 / (float)bpm;
  int base = (float)part * 1000;
  ch_millistep[channel] = (base / mod) + C_VALUES[channel][CONFIG_DELAY];
}

int getPositionFromModifier(float modifier) {
  for (int i = 0; i < 29; i++) {
    if (mod_values[i] == modifier) {
      return i;
    }
  }
}

int between(int value, int min, int max) {
  return max(min, min(max, value));
}

// set the new config value based on the rotary position. Each config has a
// different step size
void setNewConfigValue(int p) {
  bool goesUp = r.getDirection() == RE_RIGHT;

  if (CONFIGS[s_conf] == "Modifier") {
    C_VALUES[s_chan][s_conf] = mod_values[between(p, 0, MAX_MOD_VALUES)];
  } else {
    C_VALUES[s_chan][s_conf] = between(p, 0, 100);
  }
  setNewMillistep(s_chan);
}

void onRotate(Rotary &r) {
  // if the last click time as less then 100 ms age, ignore the rotation
  if ((millis() - last_click) < 100) {
    return false;
  } else {
    // Update the UI and set new config values based on the rotary positiojn
    updateMenu();
  }
}

void onButtonClick() {
  // you've made a choice, now stick to it. If you're in edit mode, go back to
  // config mode, otherwise dive deeper into the menu
  menu_state =
      menu_state == MSTATE_EDIT ? MSTATE_CONFIG : min(4, menu_state + 1);
  last_click = millis();
  handleMenuStateChange();
}

// if the menu is idle, set it to channel, otherwise to idle
void onButtonLongPress() {
  menu_state = menu_state == MSTATE_IDLE ? MSTATE_CHAN : MSTATE_IDLE;
  handleMenuStateChange();
}

void setup() {
  Serial.begin(9600);
  Serial.println("Starting up: ");
  Serial.println("display...");
  // Setup the display, DAC, wait for it to finish
  oled.begin(&Adafruit128x32, SCREEN_ADDRESS);
  oled.setFont(System5x7);
  oled.clear();

  Serial.println(C_VALUES[s_chan][s_conf]);
  // Try to initialize!
  if (!mcp.begin(DAC_ADDRESS)) {
    Serial.println("Failed to find MCP4728 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MCP...");
  oled.println("LOADING...");

  // Attach the buttons
  Serial.println("buttons...");
  r.begin(ROTARY_PIN1, ROTARY_PIN2, BUTTON_PIN);
  r.setChangedHandler(onRotate);
  b.begin(BUTTON_PIN);
  b.setClickHandler(onButtonClick);
  b.setLongClickDetectedHandler(onButtonLongPress);

  mcp.fastWrite(0, 0, 0, 0);
  delay(1000);
  oled.clear();
  oled.println("READY");

  handleMenuStateChange();

  // set all the millis to start time
  int m = millis();

  for (size_t i = 0; i < MAX_CHAN; i++) {
    setNewMillistep(i);
    ch_nextmillis[i] = m + ch_millistep[i];
  }
}

void doStep(unsigned long m, int channel) {
  if (m >= ch_nextmillis[channel]) {
    if (ch_values[channel] == 0) {
      // the level modifies the amount of voltage coming out of the DAC
      float level = C_VALUES[channel][CONFIG_LEVEL];
      float m_width = C_VALUES[channel][CONFIG_WIDTH];
      float m_delay = C_VALUES[channel][CONFIG_DELAY];
      float probability = C_VALUES[channel][CONFIG_PROBABILITY];

      // set the level
      ch_values[channel] = (4095 * float(level));

      // calculate the time untill it should go to low. This is
      // ch_millistep[channel] times the width, minus the delay, which is a
      unsigned long time = ch_millistep[channel] * (1 - (m_width / 100.0));
      unsigned long delay = ch_millistep[channel] * (m_delay / 100.0);

      ch_nextmillis[channel] += time += delay;

      if (random(0, 100) < probability) {
        mcp.setChannelValue(channel, ch_values[channel]);
      }
    } else {
      ch_values[channel] = 0;
      mcp.setChannelValue(channel, 0);
      ch_nextmillis[channel] += (ch_millistep[channel] - STEP_LEN);
    }
  }
}

void loop() {
  r.loop();
  b.loop();

  if (isPlaying) {

    unsigned long m = millis();
    // loop tru all channels and check if they should be triggered
    // for (size_t i = 0; i < MAX_CHAN; i++) {
    //   doStep(m, i);
    // }

    doStep(m, 0);
  }
}