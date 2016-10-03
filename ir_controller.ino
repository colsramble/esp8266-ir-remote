#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "sampler.h"
#include <FS.h>
#include "config.h"

/////////////////////
// Pin Definitions //
/////////////////////
const int INDICATOR_LED = D2; 
const int IR_LED        = D3; 
const int IR_DETECT_PIN = D6; 

#define IR_ON   digitalWrite(IR_LED, HIGH)
#define IR_OFF  digitalWrite(IR_LED, LOW)
#define IND_ON  digitalWrite(INDICATOR_LED, HIGH)
#define IND_OFF digitalWrite(INDICATOR_LED, LOW)

#define MAX_BUF_SIZE   8000     // IR Buffer size in Bytes (1 Byte = 8 samples) 
#define DFLT_BUF_SIZE  2500
#define MIN_BUF_SIZE      0
#define SAMPLE_PERIOD   100     // Sample period in uS

#define DBG_OUTPUT_PORT Serial
#define IR_DATA_PATH    "/ir_data"

byte irbuffer[MAX_BUF_SIZE];

const char WiFiAPPSK[] = WIFI_AP_SECRET;
const char* ssid       = WIFI_SSID;
const char* password   = WIFI_PASSWORD;
const char* host       = HOSTNAME;

ESP8266WebServer server(SERVER_PORT);

void handleRecord() {
  String path = server.hasArg("path") ? server.arg("path") : "/default";
  String sampleDuration = server.hasArg("duration") ? server.arg("duration") : "2000";
  int bufSize = (sampleDuration.toInt() * 10) / 8;

  if (bufSize < MIN_BUF_SIZE)
    bufSize = DFLT_BUF_SIZE; 
    
  if (bufSize > MAX_BUF_SIZE)
    bufSize = MAX_BUF_SIZE; 

  DBG_OUTPUT_PORT.println(sampleDuration);
  DBG_OUTPUT_PORT.printf(" Save buffer size of %d\n", bufSize);
  
  IND_ON;
  digitalRecord(IR_DETECT_PIN, SAMPLE_PERIOD, irbuffer, bufSize);
  IND_OFF;

  // save to SPIFFS
  File file = SPIFFS.open(path, "w");
  if(file) {
    // write buffer to file
    file.write(irbuffer, bufSize);
    file.close();
  } else {
    DBG_OUTPUT_PORT.println("Failed to create file\n");
  }
  
  server.send(200, "text/json", "OK");
}

void handlePlay() {
  String path = server.hasArg("path") ? server.arg("path") : "/default";
  int bufSize = 0;
  
  // Load data if there
  File file = SPIFFS.open(path, "r");
  if(file) {
    // read buffer to file
    bufSize = file.size();
    file.readBytes((char*) irbuffer, bufSize);
    file.close();
  }
  
  IR_OFF;
  IND_ON;
  digitalPlayInverted(IR_LED, SAMPLE_PERIOD, irbuffer, bufSize);
  IND_OFF;
  IR_OFF;  
  
  server.send(200, "text/json", "OK");
}

void handleList() {
  String path = server.hasArg("path") ? server.arg("path") : "";
  
  DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  
  output += "]";
  server.send(200, "text/json", output);
}

void handleClear() {
  String path = server.hasArg("path") ? server.arg("path") : "dummy";
  
  DBG_OUTPUT_PORT.println("handleClear: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()) {
    SPIFFS.remove(dir.fileName());
    output += "\"";
    output += String(dir.fileName());
    output += "\",";
  }
  
  output += "]";
  server.send(200, "text/json", output);
}

void setupWiFi()
{
  //WIFI INIT
  WiFi.mode(WIFI_STA);
  delay(300);
  DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);
  if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.begin(ssid, password);
  }
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBG_OUTPUT_PORT.print(".");
  }
  DBG_OUTPUT_PORT.println("");
  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());

  MDNS.begin(host);
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(host);
  DBG_OUTPUT_PORT.println(".local/edit to see the file browser");

  server.on("/list",   HTTP_GET, handleList);
  server.on("/record", HTTP_GET, handleRecord);
  server.on("/play",   HTTP_GET, handlePlay);
  server.on("/clear",  HTTP_GET, handleClear);
  server.onNotFound([](){
      server.send(404, "text/plain", "FileNotFound");
  });
  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");
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

void setup()
{
  initHardware();
  setupWiFi();
  //server.begin();
}

void loop()
{
  server.handleClient();
}

