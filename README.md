Current state: pre-alpha or alpha.

This is an Arduino sketch to control Tuya light switches in LAN through ESP8266 gateway.

First things first. This sketch has been written with the help of AI.

The primary purpose of (the rubber duck) this sketch is to control Tuya light switches (protocol version 3.3) through ESP8266 based on Arduino IDE. You will need three things (besides Tuya-compatible light switch):

LAN IP-address of the gate;
LAN IP-address of the light switch;
Device ID;
Local Key;
To obtain Device ID and Local Key you need to:

Sign up/sign in into Tuya Developer Platform (https://platform.tuya.com);
Add your switches in Tuya-compatible app (example: Smart Life https://play.google.com/store/apps/details?id=com.tuya.smartlife);
Link app with Tuya Developer Platform (scan QR code);
Look up Device ID under Devices in Tuya Developer Platform;
Open API Explorer - Device Management - Query Device Details in Bulk in Tuya Developer Platform;
Enter Device ID (lookup in Devices after you have linked app and Tuya Developer Platform);
Look up Local Key in the output.

Known issues:

Sometimes you need to send command twice. This could be because switch for some reason goes to sleep;
No device status feedback.
