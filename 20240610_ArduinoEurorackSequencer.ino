#include <SPI.h>
#include <Wire.h>
#include <Button2.h>
#include <Rotary.h>
#include <Adafruit_MCP4728.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiAvrI2c.h>

#define SCREEN_ADDRESS 0x3C
#define DAC_ADDRESS 0x60

#define ROTARY_PIN1 2
#define ROTARY_PIN2 3
#define BUTTON_PIN 4

#define CLICKS_PER_STEP 4

#define MSTATE_IDLE 0
#define MSTATE_CHANNEL 1
#define MSTATE_CONFIG 2
#define MSTATE_EDIT 3

#define MAX_CHANNELS 4
#define MAX_CONFIG 3

#define CONFIG_MODIFIER 0
#define CONFIG_DELAY 1
#define CONFIG_LEVEL 2

#define BTN_LONGCLICK_MS 800

int STEP_LEN = 20;

// Config & timing states
int ch_millistep[MAX_CHANNELS] = {0, 0, 0, 0};
int ch_nextmillis[MAX_CHANNELS] = {0, 0, 0, 0};
int ch_values[MAX_CHANNELS] = {0, 0, 0, 0};

int bpm = 50;

bool isPlaying = true;

Rotary r;
Button2 b;

SSD1306AsciiAvrI2c oled;

Adafruit_MCP4728 mcp;

int menu_state = MSTATE_CHANNEL;
int selected_channel = 0;
int selected_config = 0;

String CONFIGS[MAX_CONFIG] = {"modifier", "delay", "level"};

float CONFIG_VALUES[MAX_CHANNELS][MAX_CONFIG] = {{1.0, 0, 1.0}, {1.0, 0, 1.0}, {1.0, 0, 0.8}, {1.0, 0, 1.0}};
String CONFIG_PREPENDS[MAX_CONFIG] = {"x", "MS", ""};

// Update the menu UI upon a menu state change
void handleMenuStateChange()
{
  // Serial.println(F("handleMenuStateChange"));

  switch (menu_state)
  {
  case MSTATE_IDLE:
    // Serial.println(F("Idle mode"));
    oled.clear();
    // display.setCursor(0, 0);
    oled.print(F("idle"));
    break;
  case MSTATE_CHANNEL:
    oled.clear();
    // display.setCursor(0, 0);
    oled.println(F("Channel: "));
    break;
  case MSTATE_CONFIG:
    oled.clear(80, 90, 1, 2);
    // oled.println(F("..."));
    break;
  case MSTATE_EDIT:
    oled.clear(2, 80, 1, 2);
    oled.setCursor(0, 2);
    oled.print(CONFIGS[selected_config]);
    // display.fillRect(0, 20, 20, 1, SSD1306_BLACK);
    break;
  }

  updateMenu(0);
}

// Update only a small portion of the menu UI based on the rotary position
void updateMenu(int p)
{
  switch (menu_state)
  {
  case MSTATE_IDLE:
    // Serial.println(F("Idle mode"));
    break;
  case MSTATE_CHANNEL:

    // Serial.println(F("channel mode"));
    selected_channel = max(0, min(MAX_CHANNELS, p));
    oled.clear(80, 128, 1, 2);
    oled.setCursor(80, 0);
    oled.print("> ");
    oled.print(selected_channel);
    break;
  case MSTATE_CONFIG:
    // display the current config of this channel (e.g. delay, level, modifier)
    selected_config = max(0, min(MAX_CONFIG - 1, p));
    oled.clear(0, 128, 2, 3);
    oled.setCursor(0, 2);
    oled.print("> ");
    oled.print(CONFIGS[selected_config]);
    oled.setCursor(80, 2);
    oled.print(CONFIG_VALUES[selected_channel][selected_config]);
    break;
  case MSTATE_EDIT:
    // Display the currently selected config edit value, create a cursor and hide any previous values using black rectangles
    setNewConfigValue(p);
    oled.setCursor(80, 2);
    oled.clear(80, 128, 2, 3);
    oled.print("> ");

    oled.print(CONFIG_VALUES[selected_channel][selected_config]);
    break;
  }
}
// apply the new value to the channel
void setNewMillistep(int channel)
{
  float mod = CONFIG_VALUES[channel][CONFIG_MODIFIER];
  float part = 60 / (float)bpm;
  int base = (float)part * 1000;
  STEP_LEN = 2;
  Serial.println("setNewMillistep");
  ch_millistep[channel] = (base / mod) + CONFIG_VALUES[channel][CONFIG_DELAY];
}

// set the new config value based on the rotary position. Each config has a different step size
void setNewConfigValue(int p)
{
  bool goesUp = r.getDirection() == RE_RIGHT;
  if (CONFIGS[selected_config] == "delay")
  {
    int step = 1;
    CONFIG_VALUES[selected_channel][selected_config] = goesUp ? CONFIG_VALUES[selected_channel][selected_config] + step : CONFIG_VALUES[selected_channel][selected_config] - step;

    setNewMillistep(selected_channel);
  }
  else if (CONFIGS[selected_config] == "level")
  {
    float step = 0.1;
    float newValue = goesUp ? CONFIG_VALUES[selected_channel][selected_config] + step : CONFIG_VALUES[selected_channel][selected_config] - step;
    CONFIG_VALUES[selected_channel][selected_config] = max(0.0, min(1.0, newValue));
  }
  else if (CONFIGS[selected_config] == "modifier")
  {
    float step = CONFIG_VALUES[selected_channel][selected_config] > 1.0 ? 0.25 : 0.1;
    float newValue = goesUp ? CONFIG_VALUES[selected_channel][selected_config] + step : CONFIG_VALUES[selected_channel][selected_config] - step;
    CONFIG_VALUES[selected_channel][selected_config] = max(0.25, min(5.0, newValue));
    setNewMillistep(selected_channel);
  }
}

void onRotate(Rotary &r)
{
  // Update the UI and set new config values based on the rotary positiojn
  int position = r.getPosition();
  updateMenu(position);
}

void onButtonClick()
{
  // you've made a choice, now stick to it. If you're in edit mode, go back to config mode, otherwise dive deeper into the menu
  menu_state = menu_state == MSTATE_EDIT ? MSTATE_CONFIG : min(4, menu_state + 1);
  ;

  // reset the rotary position and display the new UI
  r.resetPosition(0);
  handleMenuStateChange();
}

// if the menu is idle, set it to channel, otherwise to idle
void onButtonLongPress()
{
  // Serial.println(F("onbutton long press"));
  menu_state = menu_state == MSTATE_IDLE ? MSTATE_CHANNEL : MSTATE_IDLE;
  handleMenuStateChange();
}

void setup()
{
  Serial.begin(9600);
  // Setup the display, DAC, wait for it to finish
  oled.begin(&Adafruit128x32, SCREEN_ADDRESS);
  oled.setFont(System5x7);
  oled.clear();

  // Try to initialize!
  if (!mcp.begin(DAC_ADDRESS))
  {
    // Serial.println("Failed to find MCP4728 chip");
    while (1)
    {
      delay(10);
    }
  }

  oled.println("HELLO how r u");

  // Attach the buttons
  r.begin(ROTARY_PIN1, ROTARY_PIN2, BUTTON_PIN);
  r.setChangedHandler(onRotate);
  b.begin(BUTTON_PIN);
  b.setClickHandler(onButtonClick);
  b.setLongClickDetectedHandler(onButtonLongPress);

  mcp.fastWrite(0, 0, 0, 0);

  delay(1000);

  handleMenuStateChange();

  // set all the millis to start time
  int m = millis();

  for (size_t i = 0; i < MAX_CHANNELS; i++)
  {
    setNewMillistep(i);
    ch_nextmillis[i] = m + ch_millistep[i];

    // Serial.print("Step: ");
    // Serial.print(i);
    // Serial.print(" - ");
    // Serial.print(ch_millistep[i]);
    // Serial.print(" - ");
    // Serial.println(m);
  }
}

void doStep(int m, int step)
{
  // Serial.print("Doing step");
  // Serial.print(step);
  // Serial.print(" time: ");
  // Serial.print(m);
  // Serial.print(" nextmillis: ");
  // Serial.print(ch_nextmillis[step]);
  // Serial.print(" do: ");

  if (m >= ch_nextmillis[step])
  {
    if (ch_values[step] == 0)
    {
      // Serial.print("HIGH");
      float level = CONFIG_VALUES[step][CONFIG_LEVEL];
      ch_values[step] = (4095 * float(level));
      
      Serial.println(ch_values[step]);
      mcp.setChannelValue(step, ch_values[step]);
      
      ch_nextmillis[step] += STEP_LEN;
    }
    else
    {
      // Serial.print("LOW");
      ch_values[step] = 0;
      mcp.setChannelValue(step, 0);
      ch_nextmillis[step] += (ch_millistep[step] - STEP_LEN);
    }
  }

  // Serial.println();
}

void loop()
{
  r.loop();
  b.loop();

  if (isPlaying)
  {

    int m = millis();
    // loop tru all channels and check if they should be triggered
    for (size_t i = 0; i < MAX_CHANNELS; i++)
    {
      doStep(m, i);
    }
    // doStep(m, 0);
  }
}