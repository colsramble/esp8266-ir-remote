#include <ESP8266WiFi.h>
#include "sampler.h"
#include <FS.h>

//////////////////////
// WiFi Definitions //
// Ht Me on: 
//  http://192.168.4.1/read
//  http://192.168.4.1/led/1
//  http://192.168.4.1/record
//  http://192.168.4.1/play
//////////////////////
const char WiFiAPPSK[] = "sparkfun";

/////////////////////
// Pin Definitions //
/////////////////////
const int INDICATOR_LED = D2; 
const int IR_LED        = D3; 
const int IR_DETECT_PIN = D6; 
const int ANALOG_PIN    = A0; 

#define IR_ON   digitalWrite(IR_LED, HIGH)
#define IR_OFF  digitalWrite(IR_LED, LOW)
#define IND_ON  digitalWrite(INDICATOR_LED, HIGH)
#define IND_OFF digitalWrite(INDICATOR_LED, LOW)

#define BUF_SIZE  2500     // IR Buffer size in Bytes (1 Byte = 800uS, 
#define SAMPLE_PERIOD  100

#define DBG_OUTPUT_PORT Serial
#define IR_DATA_PATH "/ir_data"

byte        irbuffer[BUF_SIZE];
WiFiServer  server(80);

void setup()
{
  initHardware();
  setupWiFi();
  server.begin();
}

void loop()
{
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  DBG_OUTPUT_PORT.println(req);
  client.flush();

  // Match the request
  int val = -1; // We'll use 'val' to keep track of both the
  // request type (read/set) and value if set.
  if (req.indexOf("/led/0") != -1)
    val = 0; // Will write LED low
  else if (req.indexOf("/led/1") != -1)
    val = 1; // Will write LED high
  else if (req.indexOf("/read") != -1)
    val = -2; // Will print pin reads
  else if (req.indexOf("/record") != -1)
    val = -3; // Will print pin reads
  else if (req.indexOf("/play") != -1)
    val = -4; // Will print pin reads
  // Otherwise request will be invalid. We'll say as much in HTML

  // Set GPIO5 according to the request
  if (val == 0) {
    IND_OFF;
    IR_OFF;
  } else if (val == 1) {
    IND_ON;
    IR_ON;
  }
    
  client.flush();

  // Prepare the response. Start with the common header:
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE HTML>\r\n<html>\r\n";
  // If we're setting the LED, print out a message saying we did
  if (val >= 0)
  {
    s += "LED is now ";
    s += (val) ? "on" : "off";
  }
  else if (val == -2)
  { // If we're reading pins, print out those values:
    s += "Analog Pin = ";
    s += String(analogRead(ANALOG_PIN));
    s += "<br>"; // Go to the next line.
    s += "Digital Pin 12 = ";
    s += String(digitalRead(IR_DETECT_PIN));
  }
  else
  {
    s += "Invalid Request.<br> Try /led/1, /led/0, or /read.";
  }
  s += "</html>\n";

  // Send the response to the client
  client.print(s);
  delay(1);
  DBG_OUTPUT_PORT.println("Client disonnected");

  if (val == -3)
    record_ir();

  if (val == -4)
    play_ir();

  // The client will actually be disconnected
  // when the function returns and 'client' object is detroyed
}

void setupWiFi()
{
  WiFi.mode(WIFI_AP);

  // Do a little work to get a unique-ish name. Append the
  // last two bytes of the MAC (HEX'd) to "Thing-":
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String AP_NameString = "ESP8266 Thing " + macID;

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i = 0; i < AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);

  WiFi.softAP(AP_NameChar, WiFiAPPSK);
}

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

void initHardware()
{
  DBG_OUTPUT_PORT.begin(115200);
  pinMode(IR_DETECT_PIN, INPUT);
  pinMode(INDICATOR_LED, OUTPUT);
  pinMode(IR_LED, OUTPUT);
  IND_OFF;
  IR_OFF;

  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }  
}

void dumpBuff(byte *buf, int len) {
  DBG_OUTPUT_PORT.println("\nBuffer :\n");
  for (int i=0; i<len; i++) {
    DBG_OUTPUT_PORT.printf("%02x\n", buf[i]);
  }
  DBG_OUTPUT_PORT.println("\n--------\n");
}

void record_ir() {
  IND_ON;
  digitalRecord(IR_DETECT_PIN, SAMPLE_PERIOD, irbuffer, BUF_SIZE);
  IND_OFF;

  // save to SPIFFS
  File file = SPIFFS.open("/ir_data/test1", "w");
  if(file) {
    // write buffer to file
    file.write(irbuffer, BUF_SIZE);
    file.close();
  } else {
    DBG_OUTPUT_PORT.println("Failed to create file\n");
  }
}

void play_ir() {
  // Load data if there
  File file = SPIFFS.open("/ir_data/test1", "r");
  if(file) {
    // read buffer to file
    file.readBytes((char*) irbuffer, file.size());
    file.close();
  }
  
  IR_OFF;
  IND_ON;
  digitalPlayInverted(IR_LED, SAMPLE_PERIOD, irbuffer, BUF_SIZE);
  IND_OFF;
  IR_OFF;
}

