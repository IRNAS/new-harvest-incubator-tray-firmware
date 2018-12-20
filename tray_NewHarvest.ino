#include <WiFi.h>     
#include <DNSServer.h>
#include <ESP32WebServer.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include "Tray.h"
#include <BlynkSimpleEsp32.h>

Tray tray;
Adafruit_SSD1306 display(OLED_RESET);
ESP32WebServer server(80);

//Varriables
float flow; //Current flow rate
float flow_set; //Set flow rate
float flow_manual; //Set manual flow rate
bool manual; //Manual movement on-off
bool regulate; //Regulation on-off
bool change; //Change syringe on-off
float temp; //Current temperature
float pos1; //Position syringe 1 in %
float pos2; //Position syringe 2 in %

//BLYNK APP==========================================================================================================
//Read flow regulation control
BLYNK_WRITE(V25)
{
  int x = param[0].asInt();
  Serial.println(x);
  tray.set_flow_regulation(x);
}
//Read manual flow regulation control
BLYNK_WRITE(V26)
{
  int x = param[0].asInt();
  tray.set_manual(x);
}
//Write syringe 1 position
BLYNK_READ(V30)
{
  Blynk.virtualWrite(V30, pos1);
}
//Write syringe 2 position
BLYNK_READ(V31)
{
  Blynk.virtualWrite(V31, pos2);
}
//Write current flow rate
BLYNK_READ(V24)
{
  Blynk.virtualWrite(V24, flow);
}
//Write current temperature
BLYNK_READ(V29)
{
  Blynk.virtualWrite(V29, temp);
}
//Write set flow rate
BLYNK_READ(V27)
{
  Blynk.virtualWrite(V27, flow_set);
}
//Write set flow rate
BLYNK_READ(V34)
{
  Blynk.virtualWrite(V34, flow_manual);
}
//Read set flow
BLYNK_WRITE(V28)
{
  float x = param[0].asFloat();
  flow_set = x;
}
//Read set manual flow
BLYNK_WRITE(V35)
{
  float x = param[0].asFloat();
  flow_manual = x;
}
//Read move syringe 1
BLYNK_WRITE(V32)
{
  int x = param[0].asInt();
  tray.set_move_syringe(x+1);
}
//Read move syringe 2
BLYNK_WRITE(V33)
{
  int x = param[0].asInt();
  tray.set_move_syringe(x+4);
}

void setup() {
  Serial.begin(115200);
  tray.Init(&display);

  server.on ( "/", handleRoot );
  server.on ( "/getValues", handleValues );
  server.on ( "/setFlow", handleFlow );
  server.on ( "/startManual", handleStartManual );
  server.on ( "/startRegulation", handleStartRegulation );
  server.on ( "/startChange", handleChange );
  server.on ( "/moveSyringe", handleMoveSyringe );
  server.on ( "/setFlowManual", handleFlowManual );
  server.on ( "/settings", handleSettings );
  server.on ( "/return", handleReturn );
  server.on ( "/resetWiFi", handleReset);
  server.on ( "/save", handleSaveSettings);
  server.begin();

  Blynk.begin(tray.get_token(), WiFi.SSID().c_str(), WiFi.psk().c_str());
}

void loop() {
  tray.switchState();
  updateVariables();
  server.handleClient();
  Blynk.run();
  setVariables();
}

//FUNCTIONS====================================================================================================================
void updateVariables(){
  flow = tray.get_flow();
  flow_set = tray.get_flow_set();
  flow_manual = tray.get_flow_manual();
  manual = tray.get_manual();
  regulate = tray.get_regulation();
  change = tray.get_change_syringe();
  temp = tray.get_T();
  pos1 = 30;
  pos2 = 70;
  //pos1 = tray.get_pos_syringe(1);
  //pos2 = tray.get_pos_syringe(2);
  /*Serial.print(flow);
  Serial.print(" ");
  Serial.print(flow_set);
  Serial.print(" "); 
  Serial.print(flow_manual);
  
  Serial.print(" ");
  Serial.print(manual);
  Serial.print(" ");
  Serial.print(regulate);
  Serial.print(" ");
  Serial.print(temp);
  Serial.println(" ");*/
}

void setVariables(){
  if(flow_set - tray.get_flow_set() != 0)
  {
    tray.set_flow(flow_set);
  }
  if(flow_manual - tray.get_flow_manual() != 0)
  {
    tray.set_manual_flow(flow_manual);
  }
}

//WEB INTERFACE=================================================================================================================
void handleRoot(){ 
  server.send ( 200, "text/html", tray.getPage() );
}

void handleValues()
{
    String sen = "{\"flow\":\""+String(flow)+"\",\"flow_set\":\""+String(flow_set)+"\",\"flow_manual\":\""+String(flow_manual)+"\",\"t\":\""+String(temp)+"\",\"manual\":\""+String(manual)+"\",\"regulate\":\""+String(regulate)+"\",\"change\":\""+String(change)+"\"}";
    server.send ( 200, "text/plane", sen );
}

void handleFlow() {
  String Value = server.arg("flow"); 
  if (Value.length() > 0) {
    flow_set = Value.toFloat();
  }
  server.send( 200, "text/plane", "" );
}

void handleFlowManual() {
  String Value = server.arg("mflow"); 
  if (Value.length() > 0) {
    flow_manual = Value.toFloat();
  }
  server.send( 200, "text/plane", "" );
}

void handleStartManual() {
  String Value = server.arg("status"); 
  bool tmp = false;
  if (Value == "START") {
    tmp = true;
  }
  tray.set_manual(tmp);
  server.send( 200, "text/plane", "" );
}

void handleStartRegulation() {
  String Value = server.arg("status"); 
  bool tmp = false;
  if (Value == "REGULATE") {
    tmp = true;
  }
  tray.set_flow_regulation(tmp);
  server.send( 200, "text/plane", "" );
}

void handleChange() {
  String Value = server.arg("status"); 
  bool tmp = false;
  if (Value == "CHANGE SYRINGE") {
    tmp = true;
  }
  tray.set_change_syringe(tmp);
  server.send( 200, "text/plane", "" );
}

void handleMoveSyringe() {
  String Value = server.arg("move"); 
  int m = Value.toInt();
  bool tmp = false;
  if(m == 0) {
    Serial.println("S1 to positive");
  }
  else if(m == 1) {
    Serial.println("S1 stop");
  }
  else if(m == 2) {
    Serial.println("S1 to negative");
  }
  else if(m == 3) {
    Serial.println("S2 to positive");
  }
  else if(m == 4) {
    Serial.println("S2 stop");
  }
  else if(m == 5) {
    Serial.println("S2 to negative");
  }
  
  server.send( 200, "text/plane", "" );
}


void handleSettings(){
  server.send ( 200, "text/html", tray.getSettings() );
}

void handleReturn(){
  server.send ( 200, "text/html", tray.getPage() );
}

void handleReset(){
  server.send ( 200, "text/html", tray.getReset() );
  delay(1000);
  tray.reset_WiFi();
}

void handleSaveSettings() {
  String Value = server.arg("name"); 
  if (Value.length() > 0) {
    tray.set_tray_name(Value);
  }
  server.send ( 200, "text/html", tray.getPage() );
}




