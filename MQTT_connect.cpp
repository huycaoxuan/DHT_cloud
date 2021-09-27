void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected() || wifiok) {
    return;
  }

  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {// connect will return 0 for connected
      if (retries_status) { // Doan nay chay chua on
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       retries_status = false;
      }
 //      delay(5000);  // wait 5 seconds
  
     /* Updates frequently */
     unsigned long currentTime = millis();

  /* This is the event */
      if (currentTime - previousTime >= mqtt_interval) {
        /* Event code */
        retries--;
        if (retries == 0) {
        // basically die and wait for WDT to reset me
        while (1);
        }
        retries_status = true;
       /* Update the timing for the next time around */
        previousTime = currentTime;
      }
  }
  Serial.println("MQTT Connected!");
}
