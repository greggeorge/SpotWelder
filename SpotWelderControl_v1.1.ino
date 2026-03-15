// SpotWelderControl_v1
//
// Using the Beetle (Arduino Leonardo compatible) to provide welding pulses to a FOTEK SSR-40 Solid State Relay (SSR)
// to provide the controlled on pulse to a rewound Microwave oven transformer to provde welding current for
// spot welding
//
// https://github.com/greggeorge/SpotWelder.git
//
// Greg George 17/02/2026 v1.0
//             14/03/2029 v1.1

#define GG_VERSION "v1.1 15 MAR 2026"


#include <Arduino.h>
#include <Button.h>        
#include <LiquidCrystal_I2C.h>  // Library for LCD


// Utility LCD routines
void readyToWeld();
void clearLine(uint8_t row);

// Note: I could not get the A0 or A1 pins to operate as digital input pins with pullup (but Mi & MO work)
#define PIN_WELD_BTN        14   // PIN_MI
#define PIN_UNTIMED_WELD    16   // PIN_MO
#define PIN_UP_BTN           9
#define PIN_DOWN_BTN        10
// #define PIN_SETUP_BTN    16   // PIN_MO
#define PIN_WELD_PULSE      11
#define PIN_LED             13
#define DEFAULT_PULSE_MS    1000         // 1 second weld pulse starting point (for the moment)
#define MIN_PULSE_LEN       20           // Min pulse width is one cycle of 50Hz mains
#define WELD_OFF            1            // Pin controlling the SSR used LOW to activate the SSR
#define WELD_DELAY          1000         // Minimum time between pulses in ms
#define MAX_UNTIMED_WELD_MS 10000        // max weld time when 'Untimed Weld' button is used - mainly as a saftey net
#define RELEASED            0
#define PRESSED             1

Button buttonWeld(PIN_WELD_BTN);      // Connect your button between pin A1 and GND
Button buttonUp(PIN_UP_BTN);
Button buttonDown(PIN_DOWN_BTN);
Button buttonUntimedWeld(PIN_UNTIMED_WELD);

LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 column and 2 rows

unsigned int setup_status         = 0;
unsigned int setup_mode           = 0;
unsigned int pulse_len_ms         = DEFAULT_PULSE_MS;    // Pulse length in milliseconds, limited to ~65 seconds (int is 0 to 65,535)
unsigned int setup_delta          = 100;                 // How much the pulse_len_ms is changed by each press of the up or down buttons
unsigned int start_time           = 0;                   // Used to time an untimed weld so we can display the time taken to the user
unsigned untimed_weld_in_progress = 0;

// Variables to track the Up and Dpwn button presses to allow detection when both are pressed
bool buttonUpState    = RELEASED;     // Current state of the button within loop()
bool buttonDownState  = RELEASED;
long buttonUpMillis   = 0;               // Time the button was last pressed
long buttonDownMillis = 0;               // Time the button was last pressed

void setup() {
  lcd.init(); //initialize the lcd
  delay(1000);

  buttonWeld.begin();
  buttonUp.begin();
  buttonDown.begin();
  buttonUntimedWeld.begin();

  pinMode(PIN_WELD_PULSE, OUTPUT);
  digitalWrite(PIN_WELD_PULSE, WELD_OFF);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  while (!Serial) { };      // Comment out once debugging finished
  Serial.begin(115200);
  delay(3000);
  Serial.print(millis());
  Serial.println(F(" START " __FILE__ " from " __DATE__));

  setup_status = 0;    // Assume we start in ready to weld mode
  
  lcd.backlight(); //open the backlight 
  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.print("GNG SPOT WELDER");
  lcd.setCursor(0, 1);
  lcd.print(GG_VERSION);
  delay(4000);
  readyToWeld();  // LCD display
}

void loop() {
  pinStatus();

  //--------------------------//
  //      WELDING SECTION     //
  //--------------------------//
  if (buttonWeld.pressed() and !setup_status) {
    Serial.print(millis());
    Serial.println(" buttonWeld pressed for welding");
    doWeld();
  }

  if (buttonWeld.pressed() and setup_status) {
    setup_mode++;
    if (setup_mode > 3) { setup_mode = 0; }
    Serial.print(millis());
    Serial.print(" buttonWeld pressed for setup mode change, setup_mode is now: ");
    Serial.println(setup_mode);
  }

  if (buttonUntimedWeld.pressed() and !setup_status) {
    clearLine(1);
    lcd.print("* UNTIMED WELD *");
    Serial.print("* UNTIMED WELD *");
    // We need to time it so we can display it to the user for a while.  They might like to set the default value based on it
    untimed_weld_in_progress = 1;
    start_time = millis();
    digitalWrite(PIN_WELD_PULSE, !WELD_OFF);
  }

  if (buttonUntimedWeld.released() and untimed_weld_in_progress) {
    digitalWrite(PIN_WELD_PULSE, WELD_OFF);
    unsigned int time_taken = millis() - start_time;
    untimed_weld_in_progress = 0;
    // Send the time to the serial port
    Serial.println("\n********************");
    Serial.print("Untimed weld of ");
    Serial.print(time_taken);
    Serial.println(" ms");
    Serial.println("********************");
    // Send the time to the display
    clearLine(1);
    lcd.print("TIME: ");
    lcd.print(time_taken);
    lcd.print(" ms");
    delay(3000);    // Display the time of the untimed weld for the user for 3 sec
    readyToWeld();  // LCD display update
  }

  // Make sure we don't do an untimed weld for too long
  if (untimed_weld_in_progress) {
    unsigned int w_time = millis() - start_time;
    if (w_time >= MAX_UNTIMED_WELD_MS) {
      digitalWrite(PIN_WELD_PULSE, WELD_OFF);
      untimed_weld_in_progress = 0;
      Serial.print("\n====================\nUNTIMED WELD TERMINATED AS IT WENT LONGER THAN 'MAX_UNTIMED_WELD_MS' which is ");
      Serial.print(MAX_UNTIMED_WELD_MS);
      Serial.print(" ms.\nWeld went for ");
      Serial.print(w_time);
      Serial.println(" ms.\n====================\n");
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WELD TERMINATED");
      lcd.setCursor(0, 1);
      lcd.print("AT ");
      lcd.print(w_time);
      lcd.print(" ms");
      delay(3000);
      readyToWeld();  // LCD display update
    }

  }
  
  //-----------------------------//
  //     NON WELDING SECTION     //
  //-----------------------------//

  // Check the status of the Up and Down buttons since they can be used to enter Setup mode if both pressed together
  long now = millis();

  // Make sure it's not too hard to trigger setup mode pressing Up & Down at the same time
  for (int x = 0; x < 3; x++) {
    if (buttonUp.pressed()) {
        buttonUpState = PRESSED;
        buttonUpMillis = now;
        delay(100);
    }
    if (buttonDown.pressed()) {
      buttonDownState = PRESSED;
      buttonDownMillis = now;
      delay(100);
    }
  }
  
  // Check if we have both Up & Down pressed to enter Setup mode
  if (buttonUpState == PRESSED && ((now - buttonUpMillis) < 200) && buttonDownState == PRESSED && ((now - buttonDownMillis) < 200)) {
      setup_status = !setup_status;
      setup_mode = 1;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SETUP: Time inc");
      lcd.setCursor(0, 1);
      lcd.print(setup_delta);
      lcd.print(" ms Up/Down");
      
      Serial.println("Entering setup mode");	
  }

  if (buttonDown.released()) {
      buttonDownState = RELEASED;
  }
  if (buttonUp.released()) {
      buttonUpState = RELEASED;
  }

  // Exit Setup mode
  if (buttonUntimedWeld.pressed() and setup_status) {
    setup_status = !setup_status;
    setup_mode = 0;
    readyToWeld();  // LCD display update
    Serial.println("Exiting setup mode");
  }

  // Normal button Up operation
  if (buttonUpState && !setup_status) {
    pulseLED();
    Serial.print(millis());
    Serial.print(" buttonUp pressed.    Weld pulse length is now ");
    pulse_len_ms += setup_delta;
    Serial.println(pulse_len_ms);
    readyToWeld();  // LCD display update
  } 

  // Normal button Down operation
  if (buttonDownState && !setup_status) {
    pulseLED();
    Serial.print(millis());
    Serial.print(" buttonDown pressed.  Weld pulse length is now ");
    if (pulse_len_ms > setup_delta) {
      pulse_len_ms -= setup_delta;
    }
    if (pulse_len_ms < setup_delta) {
      pulse_len_ms = setup_delta;
    }
    Serial.println(pulse_len_ms);
    readyToWeld();  // LCD display update
  }
  
  
  // Setup button Up operation
  if (buttonUpState && setup_status) {
    pulseLED();
    if (setup_mode == 1) {
      setup_delta += 100;
      Serial.print("Setup 'Up' pressed, setup_delta is now: ");
      Serial.println(setup_delta);
      clearLine(1);
      lcd.print(setup_delta);
      lcd.print(" ms Up/Down");
    }
  }
  
  // Setup button Down operation
  if (buttonDownState && setup_status) {
    pulseLED();  
    if (setup_mode == 1 and setup_delta > 200) {
      setup_delta -= 100;
    }
    Serial.print("Setup 'Down' pressed, setup_delta is now: ");
    Serial.println(setup_delta);
    clearLine(1);
    lcd.print(setup_delta);
    lcd.print(" ms Up/Down");    
  }

  buttonUpState = 0;
  buttonDownState = 0;
}

void doWeld() {
  // Send a pulse pulse_len_ms to the SSD input and them wait WELD_DELAY ms
  clearLine(1);
  lcd.print("WELDING");
  digitalWrite(PIN_LED, HIGH);
  digitalWrite(PIN_WELD_PULSE, !WELD_OFF);
  delay(pulse_len_ms);
  digitalWrite(PIN_WELD_PULSE, WELD_OFF);
  digitalWrite(PIN_LED, LOW);
  lcd.setCursor(0, 1);
  lcd.print("Weld finished");
  delay(WELD_DELAY);
  readyToWeld();  // LCD display update
}

// LCD helper function
void clearLine(uint8_t row) {
  lcd.setCursor(0, row);
  lcd.print("                ");  // 16 spaces
  lcd.setCursor(0, row);
}

// LCD Helper function
void readyToWeld() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready to weld");
  lcd.setCursor(0, 1);
  lcd.print("Time: ");
  lcd.print(pulse_len_ms);
  lcd.print(" ms");
}

void pulseLED() {
  digitalWrite(PIN_LED, HIGH);
  delay(50);
  digitalWrite(PIN_LED, LOW);
  delay(50);
  digitalWrite(PIN_LED, HIGH);
  delay(50);
  digitalWrite(PIN_LED, LOW);
}

void pinStatus() {
  // Serial.print(millis());
  // Serial.print(" PIN_WELD_BTN is:   ");
  // Serial.println(digitalRead(PIN_WELD_BTN));
  // Serial.print(millis());
  // Serial.print(" PIN_UP_BTN is:     ");
  // Serial.println(digitalRead(PIN_UP_BTN));
  // Serial.print(millis());
  // Serial.print(" PIN_DOWN_BTN is:   ");
  // Serial.println(digitalRead(PIN_DOWN_BTN));
  // Serial.print(millis());
  // Serial.print(millis());
  // Serial.print(" PIN_WELD_PULSE is: ");
  // Serial.println(digitalRead(PIN_WELD_PULSE));
  // Serial.print(millis());
  // Serial.print(" setup_status is: ");
  // Serial.println(setup_status);
  // delay(5000);
}

