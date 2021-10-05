/**
 * Ghi du lieu tu cam bien DHT 
 * lib: Wifimanager, AIO, DHT
 * Implements TRIGGEN_PIN button press, press for turn on/off, hold for 3 seconds for reset settings.
 */
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include "DHT.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <EEPROM.h>
#include "secrets.h"

#define TRIGGER_PIN 5 //D1 Nut bam setup wifi 
#define DHT1_PIN 14   //D5 
#define DHT2_PIN 12   //D6 
#define DHT3_PIN 13   //D7
#define RELAY1 15     //D8 the on off button feed turns this RELAY1 on/off
#define S_LED 2       //D4 LED buildin  LED bao trang thai 

#define PRESSED LOW
#define NOT_PRESSED HIGH

long blinkInterval = 100;
unsigned long previousBlink=0;
bool ledState = true;
bool blinkState = true;
bool relayState = false;

#define DHTTYPE DHT22   // DHT11, DHT21 (for DHT 21, AM2301), DHT22 (for DHT 22, AM2302, AM2321)
DHT dht1(DHT1_PIN, DHTTYPE);//for first DHT module
DHT dht2(DHT2_PIN, DHTTYPE);// for 2nd DHT module
DHT dht3(DHT3_PIN, DHTTYPE);// for 3rd DHT module

#define MQTT_UPDATE_INTERVAL 300000 //Thoi gian moi lan update len cloud

float humidity1 = 0.00 ;
float temperature1 = 0.00 ;
float humidity2 = 0.00 ;
float temperature2 = 0.00 ;
float humidity3 = 0.00 ;
float temperature3 = 0.00 ;
float avg_hum;
int hum_setting = 0;
bool res;

bool auto_mode = false;
uint16_t sliderval;
bool esp_state = true; //Trang thai cua ESP khi khoi dong

long previousMillis = 0;        // will store last time LED was updated
long led_interval = 300;           // interval at which to blink (milliseconds)

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
Adafruit_MQTT_Publish pub_infor01 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/infor01");
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
  wm.setConfigPortalTimeout(120); // auto close configportal after n seconds

  res = wm.autoConnect(WIFI_SSID01,WIFI_PW); // password protected ap
  blink_led(3);
  if(!res) {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();
  } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");
    //wifiok = true;
  }
  //DHT begin
  dht1.begin();
  dht2.begin();
  dht3.begin();
  
  // Setup MQTT subscription for onoff & slider feed.
  mqtt.subscribe(&onoffbutton);
  mqtt.subscribe(&slider);
  checkHumSetting();
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
      pub_infor01.publish("Button pressed! Relay ON");
      Serial.println("Relay ON!");
      } else {
        hum_status.publish(0);
        pub_infor01.publish("Button pressed! Relay OFF");
        Serial.println("Relay OFF!");
      }
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      Serial.print("waiting");
      int i = 0;
      while (i<3) {
        Serial.print(".");
        delay(1000);
        i++;
      }
      Serial.println("!");
      if( digitalRead(TRIGGER_PIN) == LOW ){
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wm.resetSettings();
        delay(50);
        ESP.restart();
      }  
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
  if (WiFi.status() == WL_CONNECTED){
    blinkState = false;
    digitalWrite(S_LED, LOW);
    MQTT_connect();
    sub_Button(); //Sub button and slider
    if (esp_state){ // Kiem tra trang thai cua ESP neu moi khoi dong lai
        if (! pub_infor01.publish("ESP just started.")) {
        Serial.println(F("Pub_infor: ESP_status failed to publish!"));
      } else {
        Serial.println(F("Pub_infor: ESP_status published!"));
        hum_status.publish(0);
        esp_state = false;
      }
    }
    //Publish Sensors data and Relay state
    if (millis() - lastPub > MQTT_UPDATE_INTERVAL) {
      temperature1 = getTemp("c", 1); // get DHT1 temperature in C 
      humidity1 = getTemp("h", 1); // get DHT1 humidity
      temperature2 = getTemp("c", 2); // get DHT2 temperature in C 
      humidity2 = getTemp("h", 2); // get DHT2 humidity
      temperature3 = getTemp("c", 3); // get DHT3 temperature in C 
      humidity3 = getTemp("h", 3); // get DHT3 humidity  
      avg_hum = (humidity1 + humidity2 + humidity3)/3;
      pub_DHT();
      Serial.print("Average humidity:");
      Serial.println(avg_hum);
      //hum_status.publish(relayState); 
      if (auto_mode && humidity1>0 && humidity2>0 &&humidity3>0 ) {
      Control_humidifier();
      }  
      lastPub = millis();
    }
    // ping the server to keep the mqtt connection alive
    if(! mqtt.ping()) {
      mqtt.disconnect();
    }
  }
  else{
    blinkState = true; // Nhay Led bao loi wifi
    auto_mode = false; // Che do dieu khien bang tay
  }
  checkButton(); 
  blinkLED();
}

//Chuong trinh con

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
       blink_led(2);
       delay(500);
       blink_led(2);
       //delay(5000);  // wait 5 seconds
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
    if(avg_hum<hum_setting){
        Serial.println("Turn on Humidifier!");
        relayState = true;
        digitalWrite(RELAY1, relayState); 
        pub_infor01.publish("Auto: ON");
        hum_status.publish(1);
        //delay(500);
    }
    else{
        Serial.println("Humidity ok!");
        relayState = false;
        digitalWrite(RELAY1, relayState);  
        Serial.println("Da tat may tao am");
        pub_infor01.publish("Auto: OFF");
        hum_status.publish(0);
        //delay(500);
    }
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
    digitalWrite(S_LED, ledState);
}

void pub_DHT (){
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
  pub_infor01.publish("Updated.");
}

void sub_Button(){
  //Sub button and slider
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    // Check if its the onoff button feed
    if (subscription == &onoffbutton) {
      Serial.print(F("On-Off button: "));
      Serial.println((char *)onoffbutton.lastread);
      if (strcmp((char *)onoffbutton.lastread, "ON") == 0) {
        digitalWrite(RELAY1, HIGH);
        pub_infor01.publish("Manual: ON"); 
        hum_status.publish(1);
      }
      if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
        digitalWrite(RELAY1, LOW);
        pub_infor01.publish("Manual: OFF");
        hum_status.publish(0); 
      }
    }
    
    //check if its the slider feed
    if (subscription == &slider) {
      Serial.print(F("Slider: "));
      Serial.println((char *)slider.lastread);
      sliderval = atoi((char *)slider.lastread);  // convert to a number
      
      if (sliderval <= 30){
         auto_mode = false;
         Serial.println("Manual mode!");
         pub_infor01.publish("Manual mode");
         EEPROM.update(100, 0);
         Serial.println("Update setting to EEPROM:0");
         delay (500);
      } 
      else if (sliderval>100){
         auto_mode = false;
         Serial.println("Wrong input (0-30 Manual; 31-100 Auto)");
         pub_infor01.publish("Wrong input (0-30 Manual; 31-100 Auto)");
         EEPROM.update(100, 0);
         Serial.println("Update setting to EEPROM:0");
         delay (500);
      }
      else {
         auto_mode = true;
         Serial.println("Automatic mode !");
         pub_infor01.publish("Automatic mode");
         hum_setting = map(((int)sliderval),0,100,0,100);
         EEPROM.update(100, hum_setting);
         Serial.println("Update setting to EEPROM!");
         Serial.print("Humidity threshold is:"); 
         Serial.println(hum_setting);
         delay (500);
         pub_infor01.publish(hum_setting);
      }
    }
  }
}

void blink_led(int ledBlinkNum) {
  int i=0;
  while (i < ledBlinkNum) {
    digitalWrite(S_LED, LOW);
    delay (200);
    digitalWrite(S_LED, HIGH);
    delay (200);
    i++;
  }
}

void checkHumSetting() {
   Serial.println("Humidity setting after restart: ");
   hum_setting = EEPROM.read(100);
   if(hum_setting>30) {
       auto_mode = true;
       Serial.println("Automatic mode !");
       pub_infor01.publish("Automatic mode");
       Serial.print("Humidity threshold is:"); 
       Serial.println(hum_setting);
       delay (500);
       pub_infor01.publish(hum_setting);
   }else {
       auto_mode = false;
       Serial.println("Manual mode!");
       pub_infor01.publish("Manual mode");
   }
}
