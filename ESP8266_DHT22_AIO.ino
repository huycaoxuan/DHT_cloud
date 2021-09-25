#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AutoConnect.h>
#include "DHT.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "secrets.h"


#define DHT1_PIN 14
#define DHT2_PIN 5
#define LED 2 // the on off button feed turns this LED on/off
#define PWMOUT 12 // the slider feed sets the PWM output of this pin

#define DHTTYPE DHT11   // DHT11, DHT21 (for DHT 21, AM2301), DHT22 (for DHT 22, AM2302, AM2321)
DHT dht1(DHT1_PIN, DHTTYPE);//for first DHT module
DHT dht2(DHT2_PIN, DHTTYPE);// for 2nd DHT module and do the same for 3rd and 4th etc.

#define MQTT_UPDATE_INTERVAL 60000 //Thoi gian moi lan update len cloud
float humidity1 = 0.00 ;
float temperature1 = 0.00 ;
float humidity2 = 0.00 ;
float temperature2 = 0.00 ;

String dataString;
char charBuf[100];
unsigned long previousMillis = 0; 
const long interval = 2000; 
unsigned long   lastPub = 0;

AutoConnect      portal;
WiFiClient   wifiClient;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&wifiClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed for publishing.
Adafruit_MQTT_Publish temp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temp");
Adafruit_MQTT_Publish hum = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish temp2 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temp2");
Adafruit_MQTT_Publish hum2 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity2");
//Sub
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/switch1");
Adafruit_MQTT_Subscribe slider = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/slider1");

void MQTT_connect();

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.print("WiFi ");
  if (portal.begin()) {
    Serial.println("connected:" + WiFi.SSID());
    Serial.println("IP:" + WiFi.localIP().toString());
  } else {
    Serial.println("connection failed:" + String(WiFi.status()));
    while (1) {
      delay(100);
      yield();
    }
  }
  
  dht1.begin();//for first DHT module
  dht2.begin();//for 2nd DHT module  and do the same for 3rd and 4th etc.
  
  // Setup MQTT subscription for onoff & slider feed.
  pinMode(LED, OUTPUT);     // Initialize the LED pin as an output
  mqtt.subscribe(&onoffbutton);
  mqtt.subscribe(&slider);
  //Test LED
  Serial.println("Test LED");
  Serial.println("LED ON");
  digitalWrite(LED, HIGH); 
  delay(5000);
  Serial.println("LED OFF");
  digitalWrite(LED, LOW); 
  
}
uint32_t x=0;

void loop() {
  
  MQTT_connect();
  //Sub
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    // Check if its the onoff button feed
    if (subscription == &onoffbutton) {
      Serial.print(F("On-Off button: "));
      Serial.println((char *)onoffbutton.lastread);
      
      if (strcmp((char *)onoffbutton.lastread, "ON") == 0) {
        digitalWrite(LED, LOW); 
      }
      if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
        digitalWrite(LED, HIGH); 
      }
    }
    
    //check if its the slider feed
    if (subscription == &slider) {
      Serial.print(F("Slider: "));
      Serial.println((char *)slider.lastread);
      uint16_t sliderval = atoi((char *)slider.lastread);  // convert to a number
      analogWrite(PWMOUT, sliderval);
    }
   }
  
   if (millis() - lastPub > MQTT_UPDATE_INTERVAL) {
   /************chuong trinh chinh cho vao day***************/
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  temperature1 = getTemp("c", 1); // get DHT1 temperature in C 
  humidity1 = getTemp("h", 1); // get DHT1 humidity
  temperature2 = getTemp("c", 2); // get DHT2 temperature in C 
  humidity2 = getTemp("h", 2); // get DHT2 humidity

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
  
/******************Ket thuc chuong trinh chinh********************/

    lastPub = millis();
  }
  // ping the server to keep the mqtt connection alive
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  portal.handleClient();
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
float getTemp(String req, int dhtCount)
{

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
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}
