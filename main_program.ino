// imports
#include <SPI.h>
#include "ER-EPM042-1B.h"
#include "epdpaint.h"

// colors
#define COLORED    0
#define UNCOLORED  1

// button prompts
#define PUMP_btn     btn_prompt("PUMP", 12, 20);
#define VENT_btn     btn_prompt("VENT", 12, 95);
#define HEAT_btn     btn_prompt("HEAT", 12, 170);
#define TURNOFF_btn  btn_prompt("TURN OFF", 35, 285);

// voltage is vacuumSensorValue * (5.0 / 1023.0)

// PINS
// sensor pins
const int vacuumSensorPin          = A15;
const int heatingIndicatorAndRelay = 48; // its heating
const int heatingTimerSensorPin    = 50; // timer is active
const int temperatureSensorPin     = 52;
// pumps and their LEDs to show their state (on/off)
const int turboPumpOffIndicator    = 2; // turbo off LED
const int scrollPumpOffIndicator   = 3; // scroll off LED
const int turboPumpPower           = 42; // toggle power to the relay, which toggles power to the turbo (fire - spin up, fire again - spin down)
const int turboPumpStatusIndicator = 44; // turbo status LED
const int scrollPumpRelay          = 46; // relay and on LED
// valves
//  OPEN: hold on, trigger on, trigger off
//  CLOSE: hold off
const int valve_A_trigger = 26;
const int valve_A_hold    = 28;
const int valve_B_trigger = 30;
const int valve_B_hold    = 32;
const int valve_C_trigger = 34;
const int valve_C_hold    = 36;
const int valve_D_trigger = 38;
const int valve_D_hold    = 40;
// switches: switch pressed = LOW, HIGH by default / when not pressed
const int switch_heat    = 4;
const int switch_pump    = 5;
const int switch_vent    = 6;
const int switch_turnOff = 7;

int vacuumSensorValue; // create a blank variable to hold the value from the Vacuum Sensor Pin that can be assigned a value later
bool pump_btn = false; bool vent_btn = false; bool heat_btn = false; bool turnoff_btn = false; // create global variables for the buttons on the display - boolean represents visibility (show = 1, hide = 0)

Epd epd;

// allocate memory for the canvas (epd Paint object)
unsigned int title_canvas_memory_size = 1700;
unsigned int info_canvas_memory_size = 3000;
unsigned int btn_canvas_memory_size = 2000;
// create canvases to put objects on (memorysize (bytes), width (px - multiple of 8), height (px))
Paint title_canvas(title_canvas_memory_size, 8*35, 33);
Paint info_canvas(info_canvas_memory_size, 8*50, 20);
Paint btn_canvas(btn_canvas_memory_size, 8*50, 45);

// compacted functions to avoid duplication/redundancy/repitition and to save space
void ClearInfoCanvas() {info_canvas.Clear(UNCOLORED);}
void PlaceInfoCanvas(int y_pos) {epd.SetPartialWindow(info_canvas.GetImage(), 0, y_pos, info_canvas.GetWidth(), info_canvas.GetHeight());}
void ClearBtnCanvas() {btn_canvas.Clear(UNCOLORED);}
void PlaceBtnCanvas(int y_pos) {epd.SetPartialWindow(btn_canvas.GetImage(), 0, y_pos, btn_canvas.GetWidth(), btn_canvas.GetHeight());}
void ClearTitleCanvas() {title_canvas.Clear(UNCOLORED);}
void PlaceTitleCanvas(int y_pos) {epd.SetPartialWindow(title_canvas.GetImage(), 0, y_pos, title_canvas.GetWidth(), title_canvas.GetHeight());}

void btn_prompt(char message[], int arrow_x_offset, int x_pos) {btn_canvas.DrawStringAt(x_pos, 0, message, &Font16, COLORED); btn_canvas.DrawStringAt(x_pos + arrow_x_offset, 15, "V", &Font24, COLORED);}
void reset_btns() {pump_btn = false; vent_btn = false; heat_btn = false; turnoff_btn = false;} // hide all the buttons
void initPinOutput(int pin, bool startState) {pinMode(pin, OUTPUT); digitalWrite(pin, startState);} // compact way to set a pin as an output and its starting state (ONLY FOR DIGITAL OUTPUT)
bool inRange(int minimum, int value, int maximum) {return ((value >= minimum) && (value < maximum));} // return a boolean if a value is within a range

// function to push a new message to the log - new messages are the bottom line and push previous messages upwards
// each extra line must have a new `char previous_msg#[] = "-..."` and a new `ClearInfoCanvas(); info_canvas.DrawStringAt(0, 0, previous_msg#, &Font#, COLORED); PlaceInfoCanvas(#);` below
char previous_msg3[] = "---------------------------------------------------------";
char previous_msg2[] = "---------------------------------------------------------";
char previous_msg1[] = "---------------------------------------------------------";
void display(char message[]) {
  epd.ClearFrame(); // remove everything from the display

  // place objects on the canvas
  // (font size, ~ characters per line: 8,80  12,57  16,36  20,28  24,23)
  ClearTitleCanvas();
  title_canvas.DrawStringAt(110, 0, "Auto-HENRY", &Font24, COLORED);
  title_canvas.DrawStringAt(106, 22, "By Peter and Graham Wyatt", &Font12, COLORED);
  PlaceTitleCanvas(0);

  ClearInfoCanvas(); info_canvas.DrawHorizontalLine(0, 0, 400, COLORED);              PlaceInfoCanvas(36);
  ClearInfoCanvas(); info_canvas.DrawStringAt(0, 0, previous_msg3, &Font12, COLORED); PlaceInfoCanvas(45);
  ClearInfoCanvas(); info_canvas.DrawStringAt(0, 0, previous_msg2, &Font12, COLORED); PlaceInfoCanvas(65);
  ClearInfoCanvas(); info_canvas.DrawStringAt(0, 0, previous_msg1, &Font12, COLORED); PlaceInfoCanvas(85);
  ClearInfoCanvas(); info_canvas.DrawStringAt(0, 0, message, &Font12, COLORED);       PlaceInfoCanvas(105);

  ClearBtnCanvas(); btn_canvas.DrawHorizontalLine(5, 0, 390, COLORED); PlaceBtnCanvas(250);
  ClearBtnCanvas();
  if (pump_btn) {PUMP_btn}
  if (vent_btn) {VENT_btn}
  if (heat_btn) {HEAT_btn}
  btn_canvas.DrawVerticalLine(250, 0, 50, COLORED);
  if (turnoff_btn) {TURNOFF_btn}
  PlaceBtnCanvas(260);

  // write to the display
  epd.DisplayFrame();

  // move each message up a line
  // memmove(TO, FROM, sizeof(TO))
  memmove(previous_msg3, previous_msg2, sizeof(previous_msg3));
  memmove(previous_msg2, previous_msg1, sizeof(previous_msg2));
  memmove(previous_msg1, message, sizeof(previous_msg1));
}

// a loop that pauses the program until a certain value from the analog input has been reached
// pass HIGH or LOW as startState; HIGH means the value starts high and comes down to the targetReading, LOW means the value starts low and increases to the targetReading
void waitForVacuumSensorReading(int targetReading, bool startState) {
  display("  Waiting for vacuum to reach the target ...");
  while (true) {
    vacuumSensorValue = analogRead(vacuumSensorPin);
    if (startState == HIGH && vacuumSensorValue <= targetReading) {break;}
    if (startState == LOW && vacuumSensorValue >= targetReading) {break;}
    delay(100);
  }
}

void pulseTurboActivateSwitch() {digitalWrite(turboPumpPower, LOW); delay(500); digitalWrite(turboPumpPower, HIGH); delay(500); digitalWrite(turboPumpPower, LOW);} // for the turbo pump relay just toggle the button "HIGH,short delay,LOW" to simulate a button press

void openValve(int hold, int trigger) {
  digitalWrite(hold, HIGH);
  delay(1000);
  digitalWrite(trigger, HIGH);
  delay(1000);
  digitalWrite(trigger, LOW);
}

void closeValve(int pin) {digitalWrite(pin, LOW);}

void closeAllValves() {
  closeValve(valve_A_hold); closeValve(valve_A_trigger);
  closeValve(valve_B_hold); closeValve(valve_B_trigger);
  closeValve(valve_C_hold); closeValve(valve_C_trigger);
  closeValve(valve_D_hold); closeValve(valve_D_trigger);
}

// close all the valves and then turn off power to both pumps; the chamber stays at vacuum / air but the system is ready for the next person
void turnOff() {
  display("Turning Off ...");
  closeAllValves(); // close all valves
  digitalWrite(scrollPumpRelay, LOW); digitalWrite(turboPumpStatusIndicator, LOW); pulseTurboActivateSwitch(); // turn off both pumps

  // 30 MIN TIMER

  // TURN ON TURBO OFF INDICATOR AFTER TIMER
  
  display("SHUT DOWN - Reset to turn on");
}

// handle the switching of the relay that handles power to the heating circuitry
void heat() {
  display("Initialising Heating Module ...");
  digitalWrite(heatingIndicatorAndRelay, HIGH);

  // wait until the voltage from timer sensor is high (the heating timer is being set or finished being set and is turning on)
  display("Waiting for heating timer to start ...");
  while (true) {if (digitalRead(heatingTimerSensorPin) == HIGH) {break;} delay(2000);}
  display("Heating timer started. Waiting for heating timer to stop ...");

  // wait until the voltage from timer sensor is low (the heating has finished)
  while (true) {if (digitalRead(heatingTimerSensorPin) == LOW) {break;} delay(2000);}
  display("Heating finished. Waiting for temperature to reach a safe level (<30 C) ...");

  // measure temp from thermistor
  while (true) {if (analogRead(temperatureSensorPin) < 300) {break;} delay(5000);}
  display("Temperature has reached a safe level (<30 C) - ready to be vented");
  digitalWrite(heatingIndicatorAndRelay, LOW);

  // prompts the user to select a function: VENT
  int selectedFunction = 0;
  vent_btn = true; display("PLEASE SELECT A FUNCTION"); reset_btns();
  while (true) {
    if (digitalRead(switch_vent) == LOW) {selectedFunction = 1; break;}
    delay(10);
  }
  if (selectedFunction == 1) {vent();}
}

// vent the chamber (vacuum -> atmosphere)
void vent() {
  display("Venting ...");
  closeAllValves(); openValve(valve_D_hold, valve_D_trigger); // close valves, open D
  waitForVacuumSensorReading(950, LOW); // wait until the vacuum reaches a level low enough where the door is able to be opened
  closeAllValves(); // reset all valves back to closed
  display("Finished Venting");

  // prompts the user to select a function: VENT or PUMP
  int selectedFunction = 0;
  pump_btn = true; display("PLEASE SELECT A FUNCTION"); reset_btns();
  while (true) {
    if (digitalRead(switch_pump) == LOW) {selectedFunction = 1; break;}
    delay(10);
  }

  if (selectedFunction == 1) {pump();}
}

// pump the chamber (atmosphere -> vacuum)
void pump() {
  display("Pumping ...");
  closeAllValves(); // make sure all the valves are closed
  // open valves A and C
  openValve(valve_A_hold, valve_A_trigger);
  openValve(valve_C_hold, valve_C_trigger);
  digitalWrite(scrollPumpRelay, HIGH); // turn on the scroll pump

  display("  Pumping to level 1 vacuum ...");

  waitForVacuumSensorReading(600, HIGH);
  display("  Level 1 vacuum reached. Preparing to pump to level 2 vacuum ...");

  // close A, open B, turn turbo relay on
  closeValve(valve_A_hold);
  openValve(valve_B_hold, valve_B_trigger);
  pulseTurboActivateSwitch();

  // code to detect when the turbo is at the right speed (by measuring when it starts to pull a greater vacuum)
  while (true) {
    if (analogRead(vacuumSensorPin) > 450) {break;}
    digitalWrite(turboPumpStatusIndicator, HIGH); delay(300);
    digitalWrite(turboPumpStatusIndicator, LOW); delay(300);
  }
  digitalWrite(turboPumpStatusIndicator, HIGH); delay(300);
  display("  Turbo Pump At Sufficient Speed. Pumping to level 2 vacuum ...");

  waitForVacuumSensorReading(100, HIGH);
  display("  Level 2 vacuum reached");

  // chamber is fully pumped to maximum vacuum, prompts the user to select another function: VENT or HEAT
  int selectedFunction = 0;
  vent_btn = true; heat_btn = true; turnoff_btn = true; display("PLEASE SELECT A FUNTION"); reset_btns();
  while (true) {
    if (digitalRead(switch_vent) == LOW) {selectedFunction = 1; break;}
    if (digitalRead(switch_heat) == LOW) {selectedFunction = 2; break;}
    if (digitalRead(switch_turnOff) == LOW) {selectedFunction = 3; break;}
    delay(100);
  }

  if (selectedFunction == 1) {vent();}
  if (selectedFunction == 2) {heat();}
  if (selectedFunction == 3) {turnOff();}
}

// run this first, once, when the arduino recieves power or the reset button is pressed
void setup() {
  // check the display has initialised properly
  Serial.begin(9600);
  if (epd.Init() != 0) {Serial.print(" E-Ink Display initialisation failed.\n Make sure it is plugged in properly and there is sufficient power supply.\n If it still doesn't work, the display may be broken; please contact Graham Wyatt."); while (true) {}} // if it fails to detect or initialise the display, output an error message to serial monitor and hang in an empty infinite loop until the program is restarted
  Serial.end();

  display("Starting ...");
  // Initialising Valves
  initPinOutput(valve_A_hold, LOW); initPinOutput(valve_B_hold, LOW); initPinOutput(valve_C_hold, LOW); initPinOutput(valve_D_hold, LOW);
  initPinOutput(valve_A_trigger, LOW); initPinOutput(valve_B_trigger, LOW); initPinOutput(valve_C_trigger, LOW); initPinOutput(valve_D_trigger, LOW);
  initPinOutput(heatingIndicatorAndRelay, LOW);                                                                                                                       // Initialising Heating Circuit LED
  initPinOutput(scrollPumpRelay, LOW); initPinOutput(scrollPumpOffIndicator, HIGH); initPinOutput(turboPumpStatusIndicator, LOW); initPinOutput(turboPumpPower, LOW); // Initialising Scroll and Turbo Pumps
  pinMode(switch_pump, INPUT); pinMode(switch_vent, INPUT); pinMode(switch_heat, INPUT); pinMode(switch_turnOff, INPUT);                                              // Initialising Function Selection Buttons
  pinMode(vacuumSensorPin, INPUT);                                                                                                                                    // Initialising Vacuum Sensor Input
  pinMode(temperatureSensorPin, INPUT);                                                                                                                               // Initialising Temperature Sensor Input
  pinMode(heatingTimerSensorPin, INPUT);                                                                                                                              // Initialising Heating Timer Activation Sensor Input

  delay(300);

  // prompts the user to select a function: VENT or PUMP
  int selectedFunction = 0;
  pump_btn = true; vent_btn = true; display("PLEASE SELECT A FUNCTION"); reset_btns();
  while (true) {
    if (digitalRead(switch_pump) == LOW) {selectedFunction = 1; break;}
    if (digitalRead(switch_vent) == LOW) {selectedFunction = 2; break;}
    delay(10);
  }

  if (selectedFunction == 1) {pump();}
  if (selectedFunction == 2) {vent();}
}

void loop() {}
