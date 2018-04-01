# esp32Test

This is a simple sketch to demonstrate the capabilities of the Heltec style ESP32 board, which combines an ESP32 MCU and a 16x8 OLED screen. It starts as a WiFi AP, and will print the SSID name on the screen.  After connecting to an AP, the screen will show the IP address, the NTP time, and marks along the right side corresponding to the state of 4 binary GPIO pins (23,12,22,25) and 4 sigmaDelta pins (21,13,17,2).

There is a simple web page at root that allows you to turn on and off the pins.

You will need to download some libraries used in the code.  The U8g2 library is available straight through the Arduino IDE library manager.  The Webserver and WiFiManager have not yet been updated to support the ESP32 in mainline, so you will need to manually download the libraries from bbx10's github:
- [WiFiManager](https://github.com/bbx10/WiFiManager/tree/esp32)
- [WebServer](https://github.com/bbx10/WebServer_tng)

![Pins Image](http://img.banggood.com/thumb/water/oaupload/banggood/images/E9/26/f7ac0860-f79a-4457-8140-2eda332da664.JPG)
![Pinmap](http://esp32.net/images/Heltec/WIFI-LoRa-32/Heltec_WIFI-LoRa-32_DiagramPinoutFromBottom.jpg)
