// detectButtonPress
// Use millis to detect a short and long button press
// See baldengineer.com/detect-short-long-button-press-using-millis.html for more information
// Created by James Lewis
 
#define PRESSED LOW
#define NOT_PRESSED HIGH
 
const byte led = 13;

const unsigned long shortPress = 100;
const unsigned long  longPress = 1000;
 
long blinkInterval = 100;
unsigned long previousBlink=0;
bool ledState = true;
bool blinkState = true;
 
typedef struct Buttons {
    const byte pin = 2;
    const int debounce = 10;
    unsigned long counter=0;
    bool prevState = NOT_PRESSED;
    bool currentState;
} Button;
 
// create a Button variable type
Button button;
 
void setup() {
  pinMode(led, OUTPUT);
  pinMode(button.pin, INPUT_PULLUP);
}
 
void loop() {
    // check the button
    button.currentState = digitalRead(button.pin);
 
    // has it changed?
    if (button.currentState != button.prevState) {
        delay(button.debounce);
        // update status in case of bounce
        button.currentState = digitalRead(button.pin);
        if (button.currentState == PRESSED) {
            // a new press event occured
            // record when button went down
            button.counter = millis();
        }
 
        if (button.currentState == NOT_PRESSED) {
            // but no longer pressed, how long was it down?
            unsigned long currentMillis = millis();
            //if ((currentMillis - button.counter >= shortPress) && !(currentMillis - button.counter >= longPress)) {
            if ((currentMillis - button.counter >= shortPress) && !(currentMillis - button.counter >= longPress)) {
                // short press detected. 
                handleShortPress();
            }
            if ((currentMillis - button.counter >= longPress)) {
                // the long press was detected
                handleLongPress();
            }
        }
        // used to detect when state changes
        button.prevState = button.currentState;
    } 
 
    blinkLED();
}
 
void handleShortPress() {
    blinkState = false;
    ledState = !ledState;
    digitalWrite(led, ledState);
}
 
void handleLongPress() {
    blinkState = true;
    ledState = true;
//    blinkInterval = 100;
}
 
void blinkLED() {
    // blink the LED (or don't!)
    if (blinkState) {
        if (millis() - previousBlink >= blinkInterval) {
            // blink the LED
            ledState = !ledState;
            previousBlink = millis();
        }
    } 
    digitalWrite(led, ledState);
}
