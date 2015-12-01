//For the OLED Display
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// For the one wire temp sensor DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>


/*****************************************************************************
Pins
*****************************************************************************/

// Set up the pins to communicate with the display
// Using software SPI - Arranged to use consecutive pins for easy circuit
#define OLED_MOSI  10
#define OLED_CLK   11
#define OLED_DC    12
#define OLED_CS    14
#define OLED_RESET 13
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// Setup the pin for the one wire temp sensor
#define ONE_WIRE_BUS 4

// Button
#define BUTTON_PIN 2


/*****************************************************************************
Contstants
*****************************************************************************/

// Define some key constants
// 15 minute intervals for 24 hours -> 24*4 = 96
#define HISTORY_SIZE 96
// How many milliseconds in each 15 min history bin -> 15*60*1000 = 900,000
#define BIN_MILLIS 900000

// Define the display modes
#define MODE_LIVE 0
#define MODE_HL_IN 1
#define MODE_HL_OUT 2
#define MODE_HIST_IN 3
#define MODE_HIST_OUT 4
// MAX_MODES used to roll around the modes using: mode++ % MAX_MODES
#define MAX_MODES 5

// Button timing
#define DEBOUNCE 50
#define LONG_PRESS 2000

// Display constants
// x position to start drawing the graph
#define START_GRAPH 32
// Comment out below if you want to plot dots without interconnecting lines
#define PLOT_LINES 1


/*****************************************************************************
Global Variables
*****************************************************************************/

// The buffer bin containing the next temperature reading
int nextBin;
int timeBin;
// Define variables to hold the current temperature values
float in;
float out;
// These are used to record highs and lows since reset
float inHigh, inLow;
float outHigh, outLow;
// These arrays store temperature readings at 15 minute intervals for 24 hours
float inBuffer[HISTORY_SIZE];
float outBuffer[HISTORY_SIZE];

// Following varibles declared volatile as we will update them in an interrupt
// Hold time of last interrupt so we can debounce and judge short or long press
volatile static unsigned long last_interrupt_time = 0;
// Interrupt fires on change so we need to keep track of whether the button
// has been pressed or released (We act on release...)
volatile static unsigned long btn_pressed;
volatile static unsigned long btn_released;
// Store the current display mode
volatile static int mode = MODE_LIVE;


/*****************************************************************************
Hardware (Library) Global Variables
*****************************************************************************/

// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);


/*****************************************************************************
Functions - Helper
*****************************************************************************/

// Helper Functions
// Get the highest value from a float array
float highest(float* data){
  float highest = -274.0;
  for (int i=0; i<HISTORY_SIZE; i++){
    if(data[i] > highest){
      highest = data[i];
    }
  }
  return highest;
}
// Get the lowest value from a float array
float lowest(float* data){
  float lowest = 1000;
  for (int i=0; i<HISTORY_SIZE; i++){
    if(data[i] < lowest){
      lowest = data[i];
    }
  }
  return lowest;
}


/*****************************************************************************
Functions - Display
*****************************************************************************/

// Create the Graph axes
void drawAxes(){
  // Draw Tickmarks
  for(int i=0; i<HISTORY_SIZE; i++){
    if (i % (HISTORY_SIZE/4) == 0){
      // Major tick at 6 hour intervals
      int xTick = START_GRAPH + i;
      display.drawLine(xTick, display.height(),xTick, display.height()-4, WHITE);
    } else if (i % (HISTORY_SIZE/24) == 0) {
      // Minor tick at hour intervals
      int xTick = START_GRAPH + i;
      display.drawLine(xTick, display.height(),xTick, display.height()-2, WHITE);
    }
  }
}


// Print a label on the graph at a specific position to fit with the axes
void label(char* text){
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,12);
  display.print(text);
}


// Draw the graph within the axes
void plot(float* data){
  // Get the max and min so we can scale the y axis
  float max = highest(data);
  float min = lowest(data);
  float scale = display.height()/(max-min);
  // The current bin is going to be placed at the RHS of the graph
  // historical values will be drawn towards the left
  int nowBin = (millis()/BIN_MILLIS) % HISTORY_SIZE;
  int newY;
#ifdef PLOT_LINES
  // We want to join the dots
  // We therefore need the previous plotted position
  int oldY = display.height() - (int) floor((data[(nowBin)] - min)*scale);
  for (int i=1; i<HISTORY_SIZE; i++){
    newY = display.height() - (int) floor((data[(nowBin - i) % HISTORY_SIZE] - min)*scale);
    // Draw a line between the last and current points
    display.drawLine(display.width() - (i), oldY, display.width() - (i + 1), newY, WHITE);
    // Update before we go around again
    oldY = newY;
  }
#else
  // We just want the dots
  for (int i=0; i<HISTORY_SIZE; i++){
    newY = display.height() - (int) floor((data[(nowBin - i) % HISTORY_SIZE] - min)*scale);
    // Plot the point
    display.drawPixel(display.width() - (i+1), newY, WHITE);
  }
#endif
  // Print the max and min values on the y axis
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print(max);
  display.setCursor(0,display.height() - 7);
  display.print(min);
}


// Display the live data
void displayLive(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
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
  display.setCursor(0,8);
  display.print("In:");
  display.setCursor(64,0);
  display.print(inHigh);
  display.setCursor(64,16);
  display.print(inLow);
  display.display();
}


// Display the High and Low outside temperatures
void displayHLOut(){
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setCursor(0,8);
  display.print("Out:");
  display.setCursor(64,0);
  display.print(outHigh);
  display.setCursor(64,16);
  display.print(outLow);
  display.display();
}


// Display a graph of the history data for inside
void displayHistIn(){
  display.clearDisplay();
  label("In");
  drawAxes();
  plot(inBuffer);
  display.display();
}


// Display a graph of the history data for outside
void displayHistOut(){
  display.clearDisplay();
  label("Out");
  drawAxes();
  plot(outBuffer);
  display.display();
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


/*****************************************************************************
Functions - Button
*****************************************************************************/

// Do Button actions
void buttonPress() {
  if (digitalRead(BUTTON_PIN) == LOW){
    // The button is still pressed and this will be considered a Long Press
    btn_pressed = millis();
  } else {
    // The Button has been released
    unsigned long pressed_for = millis() - btn_pressed;
    if (pressed_for > LONG_PRESS){
      switch (mode) {
        case MODE_HL_IN:
          inHigh = in;
          inLow = inHigh;
          break;
        case MODE_HL_OUT:
          outHigh = out;
          outLow = outHigh;
          break;
        default:
          mode = MODE_LIVE;
      }
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


/*****************************************************************************
Setup and Loop
*****************************************************************************/

// Setup
void setup(){
  // Initialise the Display
  display.begin(SSD1306_SWITCHCAPVCC);
  // Start up the DallasTemperature sensor
  sensors.begin();  // Start up the library
  mode = MODE_LIVE;

  // Attach Button Interrupt - internal pullup to reduce component count
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonInterrupt, CHANGE);

  // Initialise the temperature variables to sensible values
  sensors.requestTemperatures(); // Send the command to get temperatures
  in = sensors.getTempCByIndex(0);
  out = sensors.getTempCByIndex(1);
  inHigh = in;
  inLow = in;
  outHigh = out;
  outLow = out;
  nextBin = 0;
  for (int i=0; i<HISTORY_SIZE; i++){
    inBuffer[i] = in;
    outBuffer[i] = out;
  }
}


// Loop
void loop() {
  // We use "Time Bins" to record and store the data
  timeBin = (millis() / BIN_MILLIS) % HISTORY_SIZE;
  // See if it it time for us to take another reading
  if (timeBin == nextBin) {
    // Send the command to get temperatures and read them
    sensors.requestTemperatures();
    in = sensors.getTempCByIndex(0);
    out = sensors.getTempCByIndex(1);
    // Update the history
    inBuffer[nextBin] = in;
    outBuffer[nextBin] = out;
    // Check and update the Max and Min temperatures
    if (in > inHigh) inHigh = in;
    if (in < inLow) inLow = in;
    if (out > outHigh) outHigh = out;
    if (out < outLow) outLow = out;
    // Update time for next reading
    nextBin = (nextBin + 1) % HISTORY_SIZE;
    updateDisplay();
  }
  delay(1000);
}
