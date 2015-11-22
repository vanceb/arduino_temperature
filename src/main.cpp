//For the OLED Display
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// For the one wire temp sensor DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>


// Set up the pins to communicate with the display
// Using software SPI
#define OLED_MOSI  10
#define OLED_CLK   11
#define OLED_DC    12
#define OLED_CS    14
#define OLED_RESET 13
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// Setup the pin for the one wire temp sensor
#define ONE_WIRE_BUS 4


// Define some key constants
// 15 minute intervals for 24 hours -> 24*4 = 96
#define HISTORY_SIZE 96
// How many milliseconds in each history bin -> 15*60*1000 = 900,000
#define BIN_MILLIS 900000
// Define the display modes
#define MODE_LIVE 0
#define MODE_HL_IN 1
#define MODE_HL_OUT 2
#define MODE_HIST_IN 3
#define MODE_HIST_OUT 4
// MAX_MODES used to roll aroind the modes using: mode++ % MAX_MODES
#define MAX_MODES 5

// Button
#define BUTTON_PIN 2
#define DEBOUNCE 100
#define LONG_PRESS 3000

// Define variables used
// These are used to record highs and lows since reset
float inHigh, inLow;
float outHigh, outLow;
// These arrays store highs and lows at 15 minute intervals for 24 hours
float inHigh24[HISTORY_SIZE];
float inLow24[HISTORY_SIZE];
float outHigh24[HISTORY_SIZE];
float outLow24[HISTORY_SIZE];
// Define variables to hold the current temperature values
float in;
float out;
// Store the current display mode
// Declared volatile as we will update this in an interrupt
volatile static unsigned long last_interrupt_time = 0;
volatile static unsigned long btn_pressed;
volatile static unsigned long btn_released;
volatile int mode = MODE_LIVE;


// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);




// Functions
// Display the live data
void displayLive(){
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("In:  ");
  display.print(in);
  display.setCursor(0,16);
  display.print("Out: ");
  display.print(out);
  display.display();
}

// Display the High and Low inside temperatures
void displayHLIn(){
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Mode HL In");
  display.display();
}

// Display the High and Low outside temperatures
void displayHLOut(){
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Mode HL Out");
  display.display();
}

// Display a graph of the history data for inside
void displayHistIn(){
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Mode Hist In");
  display.display();
}

// Display a graph of the history data for outside
void displayHistOut(){
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Mode Hist Out");
  display.display();
}

// Create the Graph axes
void displayAxes(){

}

// Update the display
void updateDisplay() {
  switch (mode) {
    case MODE_LIVE:
      displayLive();
      break;
    case MODE_HL_IN:
      displayHLIn();
      break;
    case MODE_HL_OUT:
      displayHLOut();
      break;
    case MODE_HIST_IN:
      displayHistIn();
      break;
    case MODE_HIST_OUT:
      displayHistOut();
      break;
    default:
      mode = MODE_LIVE;
      displayLive();
      break;
  }
}
// Do Button actions
void buttonPress() {
  if (digitalRead(BUTTON_PIN) == LOW){
    // The button is still pressed and this will be considered a Long Press
    btn_pressed = millis();
  } else {
    // The Button has been released
    unsigned long pressed_for = millis() - btn_pressed;
    if (pressed_for > LONG_PRESS){
      mode = MODE_LIVE;
    } else {
      mode = (mode + 1) % MAX_MODES;
    }
    updateDisplay();
  }
}

// Interrupt handler to take user button input
void buttonInterrupt() {
  unsigned long now = millis();
  if (now - last_interrupt_time > DEBOUNCE) {
    buttonPress();
    last_interrupt_time = now;
  }
}

// Setup
void setup(){
  // Initialise the Display
  display.begin(SSD1306_SWITCHCAPVCC);
  // Start up the DallasTemperature sensor
  sensors.begin();  // Start up the library
  mode = MODE_HL_IN;
  // Attach Button Interrupt
  pinMode(BUTTON_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonInterrupt, CHANGE);
}

// Loop
void loop() {
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures(); // Send the command to get temperatures
  in = sensors.getTempCByIndex(0);
  out = sensors.getTempCByIndex(1);
  updateDisplay();
  delay(1000);
}
