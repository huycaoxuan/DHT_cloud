/**
 * Ghi du lieu tu cam bien DHT 
 * lib: Wifimanager, AIO, DHT
 * Implements TRIGGEN_PIN button press, press for ondemand configportal, hold for 3 seconds for reset settings.
 */
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include "DHT.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "secrets.h"

#define TRIGGER_PIN 5 //D1 Nut bam setup wifi 
#define DHT1_PIN 14   //D5 
#define DHT2_PIN 12   //D6 
#define DHT3_PIN 13   //D7
#define RELAY1 15     //D8 the on off button feed turns this RELAY1 on/off
#define S_LED 2       //D4 LED buildin  LED bao trang thai 

#define PRESSED LOW
#define NOT_PRESSED HIGH

//const unsigned long shortPress = 100;
//const unsigned long  longPress = 1000;
long blinkInterval = 100;
unsigned long previousBlink=0;

bool ledState = true;
bool blinkState = true;
bool relayState = false;

/*
typedef struct Buttons {
    const byte pin = 5; //D1 Nut bam setup wifi 
    const int debounce = 10;
    unsigned long counter=0;
    bool prevState = NOT_PRESSED;
    bool currentState;
} Button;
// create a Button variable type
Button button;
*/

#define DHTTYPE DHT22   // DHT11, DHT21 (for DHT 21, AM2301), DHT22 (for DHT 22, AM2302, AM2321)
DHT dht1(DHT1_PIN, DHTTYPE);//for first DHT module
DHT dht2(DHT2_PIN, DHTTYPE);// for 2nd DHT module
DHT dht3(DHT3_PIN, DHTTYPE);// for 3rd DHT module

#define MQTT_UPDATE_INTERVAL 60000 //Thoi gian moi lan update len cloud

float humidity1 = 0.00 ;
float temperature1 = 0.00 ;
float humidity2 = 0.00 ;
float temperature2 = 0.00 ;
float humidity3 = 0.00 ;
float temperature3 = 0.00 ;
bool res;
bool wifiok = false;     //Wifi status

bool auto_mode = false;
uint16_t sliderval;

// Variables will change:
//int ledState = LOW;             // ledState used to set the LED
long previousMillis = 0;        // will store last time LED was updated


long led_interval = 300;           // interval at which to blink (milliseconds)

//String dataString;
//char charBuf[100];
//const unsigned long mqtt_interval = 5000; //Thoi gian ket noi lai MQTT
//unsigned long previousTime = 0;
//bool retries_status = false;
unsigned long   lastPub = 0;

// wifimanager can run in a blocking mode or a non blocking mode
// Be sure to know how to process loops with no delay() if using non blocking
bool wm_nonblocking = false; // change to true to use non blocking

WiFiManager wm; // global wm instance
WiFiManagerParameter custom_field; // global param ( for non blocking w params )

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient   wifiClient;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;


// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&wifiClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed for publishing.
Adafruit_MQTT_Publish temp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temp");
Adafruit_MQTT_Publish hum = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish temp2 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temp2");
Adafruit_MQTT_Publish hum2 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity2");
Adafruit_MQTT_Publish temp3 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temp3");
Adafruit_MQTT_Publish hum3 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity3");
Adafruit_MQTT_Publish hum_status = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidifier-status");
//Sub
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/switch1");
Adafruit_MQTT_Subscribe slider = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/slider1"); // for setting the threshold of automatic turn on humidifier

void MQTT_connect();

void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
  Serial.begin(115200);
  Serial.setDebugOutput(true);  
  delay(3000);
  Serial.println("\n Starting");
//  pinMode(button.pin, INPUT_PULLUP);
  pinMode(RELAY1, OUTPUT);     // Initialize the RELAY1 pin as an output
  pinMode(S_LED, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  // Setup statement
  digitalWrite(RELAY1, LOW);
  digitalWrite(S_LED, HIGH);
  wifiok = false;

  // wm.resetSettings(); // wipe settings

  if(wm_nonblocking) wm.setConfigPortalBlocking(false);

  // add a custom input field
  int customFieldLength = 40;
  
  // test custom html(radio)
  const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input
  
  wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);

  // custom menu via array or vector
  // 
  // menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator) (if param is in menu, params will not show up in wifi page!)
  // const char* menu[] = {"wifi","info","param","sep","restart","exit"}; 
  // wm.setMenu(menu,6);
  std::vector<const char *> menu = {"wifi","info","param","sep","restart","exit"};
  wm.setMenu(menu);

  // set dark theme
  wm.setClass("invert");

  // wm.setConnectTimeout(20); // how long to try to connect for before continuing
  wm.setConfigPortalTimeout(60); // auto close configportal after n seconds

  res = wm.autoConnect(WIFI_SSID01,WIFI_PW); // password protected ap

  if(!res) {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();
  } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");
    wifiok = true;
  }
  //DHT begin
  dht1.begin();
  dht2.begin();
  dht3.begin();
  
  // Setup MQTT subscription for onoff & slider feed.
  mqtt.subscribe(&onoffbutton);
  mqtt.subscribe(&slider);
}

//Check button (wifi manager example)
void checkButton(){
  // check for button press
  //if ( digitalRead(TRIGGER_PIN) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    //delay(50);
    if( digitalRead(TRIGGER_PIN) == LOW ){
      Serial.println("Button Pressed");
      relayState = !relayState;
      digitalWrite(RELAY1, relayState);
      if (relayState){
      hum_status.publish(1);
      Serial.println("Relay ON!");
      } else {
        hum_status.publish(0);
        Serial.println("Relay OFF!");
      }
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if( digitalRead(TRIGGER_PIN) == LOW ){
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wm.resetSettings();
        delay(50);
        ESP.restart();
      }  

      /*
      // start portal w delay
      Serial.println("Starting config portal");
      wm.setConfigPortalTimeout(120);
      
      if (!wm.startConfigPortal(WIFI_SSID02,WIFI_PW)) {
        Serial.println("failed to connect or hit timeout");
        delay(3000);
        // ESP.restart();
        wifiok = false;
      } 
      else {
        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
        wifiok = true; 
      }
      */
  }
}



String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(){
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
}

void loop() {
  if(wm_nonblocking) wm.process(); // avoid delays() in loop when non-blocking and other long running code 
  if(res) { 
    wifiok = true;
  }
  checkButton(); 
  
  /* // check the button
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
  */
  
  if (wifiok) {                                     //Neu wifi ok thi ket noi voi MQTT
  blinkState = false;
  digitalWrite(S_LED, LOW);
  MQTT_connect();
  
  //Sub button and slider
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    // Check if its the onoff button feed
    if (subscription == &onoffbutton) {
      Serial.print(F("On-Off button: "));
      Serial.println((char *)onoffbutton.lastread);
      
      if (strcmp((char *)onoffbutton.lastread, "ON") == 0) {
        digitalWrite(RELAY1, HIGH); 
        hum_status.publish(1);
      }
      if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
        digitalWrite(RELAY1, LOW);
        hum_status.publish(0); 
      }
    }
    
    //check if its the slider feed
    if (subscription == &slider) {
      Serial.print(F("Slider: "));
      Serial.println((char *)slider.lastread);
      sliderval = atoi((char *)slider.lastread);  // convert to a number
      
      if (sliderval == 0){
         auto_mode = false;
         Serial.println("Manual mode!");
      } 
      else {
         auto_mode = true;
         Serial.println("Automatic mode !");
         Serial.print("Humidity threshold is:");
         Serial.println(sliderval);
        }
      //analogWrite(PWMOUT, sliderval);
      }
     }
//Publish Sensors data
   if (millis() - lastPub > MQTT_UPDATE_INTERVAL) {
  temperature1 = getTemp("c", 1); // get DHT1 temperature in C 
  humidity1 = getTemp("h", 1); // get DHT1 humidity
  temperature2 = getTemp("c", 2); // get DHT2 temperature in C 
  humidity2 = getTemp("h", 2); // get DHT2 humidity
  temperature3 = getTemp("c", 3); // get DHT3 temperature in C 
  humidity3 = getTemp("h", 3); // get DHT3 humidity             

  //Publish Sensor 1
  Serial.print(F("\nSending Temperature 1 "));
  Serial.print(temperature1);
  Serial.print("...");
  if (! temp.publish(temperature1)) {
    Serial.println(F("Temp1 failed to publish!"));
  } else {
    Serial.println(F("Temp1 published!"));
  }

  Serial.print(F("\nSending Humidity "));
  Serial.print(humidity1);
  Serial.print("...");
  if (! hum.publish(humidity1)) {
    Serial.println(F("Hum1 failed to publish!"));
  } else {
    Serial.println(F("Hum1 published!"));
  }

  //Publish Sensor 2
  Serial.print(F("\nSending Temperature 2 "));
  Serial.print(temperature2);
  Serial.print("...");
  if (! temp2.publish(temperature2)) {
    Serial.println(F("Temp2 failed to publish!"));
  } else {
    Serial.println(F("Temp2 published!"));
  }

  Serial.print(F("\nSending Humidity 2 "));
  Serial.print(humidity2);
  Serial.print("...");
  if (! hum2.publish(humidity2)) {
    Serial.println(F("Hum2 failed to publish!"));
  } else {
    Serial.println(F("Hum2 published!"));
  }

    //Publish Sensor 3
  Serial.print(F("\nSending Temperature 3 "));
  Serial.print(temperature3);                      
  Serial.print("...");
  if (! temp3.publish(temperature3)) {
    Serial.println(F("Temp3 failed to publish!"));
  } else {
    Serial.println(F("Temp3 published!"));
  }

  Serial.print(F("\nSending Humidity 3 "));
  Serial.print(humidity3);                        
  Serial.print("...");
  if (! hum3.publish(humidity3)) {
    Serial.println(F("Hum3 failed to publish!"));
  } else {
    Serial.println(F("Hum3 published!"));
  }
  
/******************Ket thuc chuong trinh chinh********************/

    lastPub = millis();
  }
  if (auto_mode && humidity1>0) {
    Control_humidifier();
  }
  // ping the server to keep the mqtt connection alive
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  }
  else {
    //digitalWrite(S_LED, HIGH);
    //Serial.println("Wifi Error!!!");
    // Nhay LED tai day
    blinkState = true;
  }
  blinkLED();
}

//Chuong trinh con


/*
 * getTemp(String req, int dhtCount)
 * returns the temprature related parameters
 * req is string request
 dhtCount is 1 or 2 or 3 as you wish
 * This code can display temprature in:
 * getTemp("c", 1) is used to get Celsius for first DHT
 * getTemp("h", 2) is used to get humidity for 2nd DHT
 */
float getTemp(String req, int dhtCount){

if(dhtCount ==1){
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h1 = dht1.readHumidity();
  // Read temperature as Celsius (the default)
  float t1 = dht1.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
  float f1 = dht1.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h1) || isnan(t1)|| isnan(f1)) {
    Serial.println("Failed to read from DHT sensor!");
    //return;
  }
  
  if(req =="c"){
    return t1;//return Cilsus
  }else if(req =="h"){
    return h1;// return humidity
  }else{
    return 0.000;// if no reqest found, retun 0.000
  }
}// DHT1 end


if(dhtCount ==2){
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h2 = dht2.readHumidity();
  // Read temperature as Celsius (the default)
  float t2 = dht2.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
  float f2 = dht2.readTemperature(true);


  // Check if any reads failed and exit early (to try again).
  if (isnan(h2) || isnan(t2)|| isnan(f2)) {
    Serial.println("Failed to read from DHT 2 sensor!");
    //return;
  }
  
  if(req =="c"){
    return t2;//return Cilsus
  }else if(req =="h"){
    return h2;// return humidity
  }else{
    return 0.000;// if no reqest found, retun 0.000
  }
}// DHT2 end

if(dhtCount ==3){
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h3 = dht3.readHumidity();
  // Read temperature as Celsius (the default)
  float t3 = dht3.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
  float f3 = dht3.readTemperature(true);


  // Check if any reads failed and exit early (to try again).
  if (isnan(h3) || isnan(t3)|| isnan(f3)) {
    Serial.println("Failed to read from DHT 3 sensor!");
    //return;
  }
  
  if(req =="c"){
    return t3;//return Cilsus
  }else if(req =="h"){
    return h3;// return humidity
  }else{
    return 0.000;// if no reqest found, retun 0.000
  }
}// DHT3 end

}//getTemp end


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       digitalWrite(S_LED, HIGH);
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
  digitalWrite(S_LED, LOW);
  //MQTT_status = true;
}

void Control_humidifier() {
    if(humidity1<sliderval){
        Serial.println("Turn on Humidifier!");
        relayState = true;
        digitalWrite(RELAY1, relayState); 
        hum_status.publish(1);
        delay(500);
    }
    else{
        Serial.println("Humidity ok!");
        relayState = false;
        digitalWrite(RELAY1, relayState);  
        Serial.println("Turn off Humidifier!");
        hum_status.publish(0);
        delay(500);
    }
  }

/*
void handleShortPress() {
    //blinkState = false;
    Serial.println("Button click!");
    relayState = !relayState;
    digitalWrite(RELAY1, relayState);
}
 
void handleLongPress() {
      digitalWrite(S_LED, HIGH);
      Serial.println("Button Held");
      Serial.println("Erasing Config, restarting");
      wm.resetSettings();
      ESP.restart();
    //blinkState = true;
    //ledState = true;
    //blinkInterval = 100;
}
*/
 
void blinkLED() {
    // blink the LED (or don't!)
    if (blinkState) {
        if (millis() - previousBlink >= blinkInterval) {
            // blink the LED
            ledState = !ledState;
            previousBlink = millis();
        }
    } 
    digitalWrite(S_LED, ledState);
}
