

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Button2.h>
#include <Rotary.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

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

// Config & timing states
int ch_millistep[MAX_CHANNELS] = {0, 0, 0, 0};
int ch_nextmillis[MAX_CHANNELS] = {0, 0, 0, 0};

int bpm = 120;

bool isPlaying = false;

Rotary r;
Button2 b;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int menu_state = MSTATE_CHANNEL;
int selected_channel = 0;
int selected_config = 0;

String CONFIGS[MAX_CONFIG] = {"modifier", "delay", "level"};
float CONFIG_VALUES[MAX_CHANNELS][MAX_CONFIG] = {{1.0, 0, 1.0}, {1.0, 0, 1.0}, {1.0, 0, 1.0}, {1.0, 0, 1.0}};
String CONFIG_PREPENDS[MAX_CONFIG] = {"x", "MS", ""};

// Update the menu UI upon a menu state change
void handleMenuStateChange()
{
  Serial.println("handleMenuStateChange");

  switch (menu_state)
  {
  case MSTATE_IDLE:
    Serial.println("Idle mode");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("idle");
    display.display();
    break;
  case MSTATE_CHANNEL:
    display.clearDisplay();
    display.setCursor(0, 0);
    display.write("Channel: ");
    updateMenu(0);
    break;
  case MSTATE_CONFIG:
    display.fillRect(70, 10, 5, 1, SSD1306_BLACK);
    updateMenu(0);
    break;
  case MSTATE_EDIT:
    display.fillRect(0, 20, 20, 1, SSD1306_BLACK);
    updateMenu(0);
    break;
  }
}

// Update only a small portion of the menu UI based on the rotary position
void updateMenu(int p)
{
  switch (menu_state)
  {
  case MSTATE_IDLE:
    Serial.println("Idle mode");
    break;
  case MSTATE_CHANNEL:
    // Display the currently selected channel, create a cursor and hide any previous values using black rectangles
    Serial.println("channel mode");
    selected_channel = max(0, min(MAX_CHANNELS, p));
    display.fillRect(70, 0, 30, 30, SSD1306_BLACK);
    display.setCursor(70, 0);
    display.print(selected_channel);
    display.fillRect(70, 10, 5, 1, SSD1306_WHITE);
    display.display();
    break;
  case MSTATE_CONFIG:
    // Display the currently selected config, create a cursor and hide any previous values using black rectangles
    Serial.println("config mode");
    Serial.print("Selected config: ");
    selected_config = max(0, min(MAX_CONFIG - 1, p));
    display.fillRect(0, 10, 200, 30, SSD1306_BLACK);
    display.setCursor(0, 10);
    display.setCursor(70, 10);
    display.print(floatToStringWithPart(CONFIG_VALUES[selected_channel][selected_config]));
    display.fillRect(0, 20, 20, 1, SSD1306_WHITE);
    display.display();
    break;
  case MSTATE_EDIT:
    // Display the currently selected config edit value, create a cursor and hide any previous values using black rectangles
    setNewConfigValue(p);
    Serial.println("edit config mode");
    display.setCursor(70, 10);
    display.fillRect(70, 10, 200, 10, SSD1306_BLACK);
    display.print(floatToStringWithPart(CONFIG_VALUES[selected_channel][selected_config]));
    display.fillRect(70, 20, 20, 1, SSD1306_WHITE);
    display.display();
    break;
  }
}

// Combine the value of the config together with its unit, and return it as a stirng
String floatToStringWithPart(float value)
{
  char buffer[10];
  dtostrf(value, 0, 2, buffer); // Convert float to string with 2 decimal places
  String result = String(buffer) + CONFIG_PREPENDS[selected_config];
  return result;
}

// apply the new value to the channel
void setNewMillistep()
{
  int base = (bpm / 60) * (CONFIG_VALUES[selected_channel][CONFIG_MODIFIER] * 100);
  ch_millistep[selected_channel] = base + CONFIG_VALUES[CONFIG_DELAY];
}

// set the new config value based on the rotary position. Each config has a different step size
void setNewConfigValue(int p)
{
  bool goesUp = r.getDirection() == RE_RIGHT;
  if (CONFIGS[selected_config] == "delay")
  {
    int step = 1;
    CONFIG_VALUES[selected_channel][selected_config] = goesUp ? CONFIG_VALUES[selected_channel][selected_config] + step : CONFIG_VALUES[selected_channel][selected_config] - step;

    setNewMillistep();
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
    setNewMillistep();
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
  if (menu_state == MSTATE_EDIT)
  {
    menu_state = MSTATE_CONFIG;
  }
  else
  {
    menu_state = min(4, menu_state + 1);
  }

  // reset the rotary position and display the new UI
  r.resetPosition(0);
  handleMenuStateChange();
}

// if the menu is idle, set it to channel, otherwise to idle
void onButtonLongPress()
{
  Serial.println("onbutton long press");
  menu_state = menu_state == MSTATE_IDLE ? MSTATE_CHANNEL : MSTATE_IDLE;
  handleMenuStateChange();
}

void setup()
{
  Serial.begin(9600);
  // Setup the display, wait for it to finish
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  display.clearDisplay();

  // Attach the buttons
  r.begin(ROTARY_PIN1, ROTARY_PIN2, BUTTON_PIN);
  r.setChangedHandler(onRotate);
  b.begin(BUTTON_PIN);
  b.setClickHandler(onButtonClick);
  b.setLongClickDetectedHandler(onButtonLongPress);

  // Draw the menu
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);

  handleMenuStateChange();

  // set all the millis to start time
  int m = millis();

  for (size_t i = 0; i < MAX_CHANNELS; i++)
  {
    ch_millistep[i] = (bpm / 60) * 100;
    ch_nextmillis[i] = m + ch_millistep[i];
  }
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
      {
        if (m >= ch_nextmillis[i])
        {
          Serial.print("Channel step: ");
          Serial.println(i);
          ch_nextmillis[i] = m + ch_millistep[i];
        }
      }
    }
  }
}