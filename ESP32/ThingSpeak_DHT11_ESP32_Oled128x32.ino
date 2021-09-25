//Upload lên Thingspeak nhiệt độ và độ ẩm sau mỗi 5 phút, Cam biến DHT22, Màn hỉnh OLED 128x32

#include <WiFi.h>
#include "secrets.h"
#include "ThingSpeak.h" // always include thingspeak header file after other header files and custom macros
#include "DHT.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "PMS.h"


HardwareSerial SerialPMS(1);
PMS pms(SerialPMS);
PMS::DATA data;

#define RXD2 26
#define TXD2 25

// ESP32 --> Pantower PMS7003
// 26    --> TX
// 25    --> RX
// GND   --> GND

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 14
// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE); 

#define LEDPIN 13
#define THINGSPEAK_TIME 5 //thời gian up lên thingspeak tính bằng phút

char ssid[] = SECRET_SSID;   // your network SSID (name) 
char pass[] = SECRET_PASS;   // your network password
int keyIndex = 0;            // your network key Index number (needed only for WEP)
WiFiClient  client;

unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

void setup() {
  pinMode(13, OUTPUT);  // Đèn LED ở chân 13
  Serial.begin(9600);  //Initialize serial
  SerialPMS.begin(9600, SERIAL_8N1, RXD2, TXD2);
  pms.passiveMode();
  dht.begin();
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo native USB port only
  }
  
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.clearDisplay();             // Clear logo Adafruid
  display.setCursor(0, 0);
  display.println("Temp Humidity PM2.5");
  display.println("upload to thingSpeak");
  display.println("___By HuyCX");
  display.display();      // Show initial text
  delay(3000);
  
  WiFi.mode(WIFI_STA);   
  ThingSpeak.begin(client);  // Initialize ThingSpeak
}

void loop() {
// Chưa có phần PM2.5 từ đây

  // Connect or reconnect to WiFi
  if(WiFi.status() != WL_CONNECTED){
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      conectingtext();
      delay(5000);     
    } 
    Serial.println("\nConnected.");
    ConnectedMss();
  }
  pms.wakeUp();
  delay(30000);
  Serial.println("Send read request...");
  pms.requestRead();
  Serial.println("Wait max. 1 second for read...");
  
  //upto thingspeak
  uploadtothingspeak();
  digitalWrite(LEDPIN, HIGH);   // turn the LED on 
  delay(500);                   
  digitalWrite(LEDPIN, LOW);    
  delay(THINGSPEAK_TIME*60000); // Wait * minutes to update the channel again
}

//Các hàm - chương trình con

void ConnectedMss(void) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Connected."));
  display.display();      // Show initial text
  delay(2000);
}

void conectingtext(void) {
  display.clearDisplay();
  display.setCursor(0,0);             // Start at top-left corner
  display.println("Connecting to Wifi");
  display.display();
}

void uploadtothingspeak(void){

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);
  // Read PM2.5
  float d;
  if (pms.readUntil(data))
  {

//    Serial.print("PM 1.0 (ug/m3): ");
//    Serial.println(data.PM_AE_UG_1_0);
//
    d = data.PM_AE_UG_2_5;
//
//    Serial.print("PM 10.0 (ug/m3): ");
//    Serial.println(data.PM_AE_UG_10_0);
  }
  else
  {
    Serial.println("No data from Dust Sensor.");
  }    

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  //float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  //float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print("PM 2.5 (ug/m3): ");
  Serial.print(d);
  
  //Hiển thị thông số nhiệt độ và độ ẩm lên OLED
  display.clearDisplay();
  display.setCursor(0,0);             // Start at top-left corner
  display.print("Humidity: ");
  display.println(h);
  display.print("Temperature: ");
  display.println(t);
  display.print("PM 2.5 (ug/m3): ");
  display.println(d);
  display.display();
  delay(100);

  // set the fields with the values
  ThingSpeak.setField(1, d);
  ThingSpeak.setField(2, t);
  ThingSpeak.setField(3, h);

  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("Channel update successful.");
    display.println("Channel updated!");
    display.display();
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Update Fail !!!");
    display.display();
  } 
}