/*
 ************************************************
 *         Wi-PWN firmware for ESP8266          *
 *     https://github.com/samdenty99/Wi-PWN     *
 *            (c) 2017 Samuel Denty             *
 *----------------------------------------------*
 *  Wi-PWN based on spacehuhn/esp8266_deauther  *
 *              (c) Stefan Kremser              *
 ************************************************
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#ifdef USE_CAPTIVE_PORTAL
  #include "./DNSServer.h"       // Patched lib
#endif
#include <ESP8266WebServer.h>
#include <FS.h>

#define resetPin 4           /* <-- comment out or change if you need GPIO 4 for other purposes */
//#define USE_DISPLAY        /* <-- uncomment that if you want to use the display */
//#define USE_LED16          /* <-- for the Pocket ESP8266 which has a LED on GPIO 16 to indicate if it's running */
//#define USE_CAPTIVE_PORTAL /* <-- enable captive portal (redirects all pages to 192.168.4.1) - most devices flood the ESP8266 with requests */




#ifdef USE_DISPLAY
  #include <Wire.h>
  
  //include the library you need
  #include "SSD1306.h"
  //#include "SH1106.h"

  //create display(Adr, SDA-pin, SCL-pin)
  SSD1306 display(0x3c, 5, 4); //GPIO 5 = D1, GPIO 4 = D2
  //SH1106 display(0x3c, 5, 4);
  
  //button pins
  #define upBtn 12 //GPIO 12 = D6
  #define downBtn 13 //GPIO 13 = D7
  #define selectBtn 14 //GPIO 14 = D5
  #define displayBtn 0 //GPIO 0 = FLASH BUTTON
  
  //render settings
  #define fontSize 8
  #define rowsPerSite 8
  
  int rows = 3;
  int curRow = 0;
  int sites = 1;
  int curSite = 1;
  int lrow = 0;

  bool canBtnPress = true;
  int buttonPressed = 0; //0 = UP, 1 = DOWN, 2 = SELECT, 3 = DISPLAY
  bool displayOn = true;
#endif

String wifiMode = "";
String attackMode = "";
String scanMode = "SCAN";

bool warning = true;

extern "C" {
  #include "user_interface.h"
}

#ifdef USE_CAPTIVE_PORTAL
  const byte        DNS_PORT = 53;          // Capture DNS requests on port 53
  IPAddress         apIP(192, 168, 4, 1);   // IP Address for Wi-PWN (Changing this will cause unwanted side effects - app malfunctioning)
  DNSServer         dnsServer;              // Create the DNS object
#endif
ESP8266WebServer server(80);                // HTTP server

#include <EEPROM.h>
#include "data.h"
#include "NameList.h"
#include "APScan.h"
#include "ClientScan.h"
#include "Attack.h"
#include "Settings.h"
#include "SSIDList.h"
#include "Detector.h"

/* ========== DEBUG ========== */
const bool debug = true;
/* ========== DEBUG ========== */

NameList nameList;

APScan apScan;
ClientScan clientScan;
Attack attack;
Settings settings;
SSIDList ssidList;
Detector detector;

void sniffer(uint8_t *buf, uint16_t len) {
  clientScan.packetSniffer(buf, len);
}

#ifdef USE_DISPLAY
void drawInterface() {
  if(displayOn){
    display.clear();

    int _lrow = 0;
    for (int i = curSite * rowsPerSite - rowsPerSite; i < curSite * rowsPerSite; i++) {
      if (i == 0) display.drawString(3, i * fontSize, " -->  WiFi " + wifiMode);
      else if (i == 1) display.drawString(3, i * fontSize, " -->  " + scanMode);
      else if (i == 2) display.drawString(3, i * fontSize, " -->  " + attackMode + " attack");
      else if (i - 3 < apScan.results) {
        display.drawString(3, _lrow * fontSize, apScan.getAPName(i - 3));
        if (apScan.getAPSelected(i - 3)) {
          display.drawVerticalLine(1, _lrow * fontSize, fontSize);
          display.drawVerticalLine(2, _lrow * fontSize, fontSize);
        }
      }
      if (_lrow == lrow) display.drawVerticalLine(0, _lrow * fontSize, fontSize);
      _lrow++;
    }
  
    display.display();
  }
}
#endif

void startWifi() {
  Serial.println("\nStarting WiFi AP:");
  WiFi.mode(WIFI_STA);
  wifi_set_promiscuous_rx_cb(sniffer);
  #ifdef USE_CAPTIVE_PORTAL
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  #endif
  WiFi.softAP((const char*)settings.ssid.c_str(), (const char*)settings.password.c_str(), settings.apChannel, settings.ssidHidden); //for an open network without a password change to:  WiFi.softAP(ssid);
  Serial.println("SSID          : '" + settings.ssid+"'");
  Serial.println("Password      : '" + settings.password+"'");
  #ifdef USE_CAPTIVE_PORTAL
    if (settings.newUser == 1) {dnsServer.start(DNS_PORT, "*", apIP);Serial.println("Captive Portal: Running");} else {Serial.println("Captive Portal: Stopped");}
  #endif
  if (settings.newUser == 1) {Serial.println("Redirecting to setup page");}
  Serial.println("-----------------------------------------------");
  if (settings.password.length() < 8) Serial.println("WARNING: password must have at least 8 characters!");
  if (settings.ssid.length() < 1 || settings.ssid.length() > 32) Serial.println("WARNING: SSID length must be between 1 and 32 characters!");
  wifiMode = "ON";
}

void stopWifi() {
  Serial.println("stopping WiFi AP");
  Serial.println("-----------------------------------------------");
  WiFi.disconnect();
  wifi_set_opmode(STATION_MODE);
  wifiMode = "OFF";
}

void loadSetupHTML() {
  if(warning){
    sendFile(200, "text/html", data_setup_HTML, sizeof(data_setup_HTML));
  }else{
    sendFile(200, "text/html", data_index_HTML, sizeof(data_index_HTML));
  }
}
void loadIndexHTML() {
  warning = false;
  sendFile(200, "text/html", data_index_HTML, sizeof(data_index_HTML));
}
void loadUsersHTML() {
  sendFile(200, "text/html", data_users_HTML, sizeof(data_users_HTML));
}
void loadAttackHTML() {
  sendFile(200, "text/html", data_attack_HTML, sizeof(data_attack_HTML));
}
void loadDetectorHTML() {
  sendFile(200, "text/html", data_detector_HTML, sizeof(data_detector_HTML));
}
void loadControlHTML() {
  sendFile(200, "text/html", data_control_HTML, sizeof(data_control_HTML));
}
void loadSettingsHTML() {
  sendFile(200, "text/html", data_settings_HTML, sizeof(data_settings_HTML));
}
void load404() {
  sendFile(404, "text/html", data_error_HTML, sizeof(data_error_HTML));
}
void loadInfoHTML(){
  sendFile(200, "text/html", data_info_HTML, sizeof(data_info_HTML));
}

void loadFunctionsJS() {
  sendFile(200, "text/javascript", data_functions_JS, sizeof(data_functions_JS));
}
void loadScanJS() {
  sendFile(200, "text/javascript", data_scan_JS, sizeof(data_scan_JS));
}
void loadUsersJS() {
  sendFile(200, "text/javascript", data_users_JS, sizeof(data_users_JS));
}
void loadAttackJS() {
  attack.ssidChange = true;
  sendFile(200, "text/javascript", data_attack_JS, sizeof(data_attack_JS));
}
void loadControlJS() {
  sendFile(200, "text/javascript", data_control_JS, sizeof(data_control_JS));
}
void loadSettingsJS() {
  sendFile(200, "text/javascript", data_settings_JS, sizeof(data_settings_JS));
}

void loadStyle() {
  sendFile(200, "text/css;charset=UTF-8", data_main_CSS, sizeof(data_main_CSS));
}

void loadDarkMode() {
  if (settings.darkMode) {
    sendFile(200, "text/css;charset=UTF-8", data_dark_CSS, sizeof(data_dark_CSS));
  } else {
    server.send(200, "text/html", "/* Dark mode disabled */");
  }
}

void loadDarkModeForce() {
   sendFile(200, "text/css;charset=UTF-8", data_dark_CSS, sizeof(data_dark_CSS));
}

void loadRedirectHTML() {
    server.send(302, "text/html", "<meta content='0; url=http://192.168.4.1'http-equiv='refresh'>");
}


void startWiFi(bool start) {
  if (start) startWifi();
  else stopWifi();
  clientScan.clearList();
}

//==========AP-Scan==========
void startAPScan() {
  scanMode = "scanning...";
#ifdef USE_DISPLAY
  drawInterface();
#endif
  if (apScan.start()) {

#ifdef USE_DISPLAY
    apScan.sort();
    rows = 3;
    rows += apScan.results;
    sites = rows / rowsPerSite;
    if (rows % rowsPerSite > 0) sites++;
#endif

    server.send ( 200, "text/json", "true");
    attack.stopAll();
    scanMode = "SCAN";
  }
}

void sendAPResults() {
  apScan.sendResults();
}

void selectAP() {
  if (server.hasArg("num")) {
    apScan.select(server.arg("num").toInt());
    server.send( 200, "text/json", "true");
    attack.stopAll();
  }
}

//==========Client-Scan==========
void startClientScan() {
  if (server.hasArg("time") && apScan.getFirstTarget() > -1 && !clientScan.sniffing) {
    server.send(200, "text/json", "true");
    clientScan.start(server.arg("time").toInt());
    attack.stopAll();
  } else server.send( 200, "text/json", "ERROR: No selected Wi-Fi networks!");
}

void sendClientResults() {
  clientScan.send();
}
void sendClientScanTime() {
  server.send( 200, "text/json", (String)settings.clientScanTime );
}

void selectClient() {
  if (server.hasArg("num")) {
    clientScan.select(server.arg("num").toInt());
    attack.stop(0);
    server.send( 200, "text/json", "true");
  }
}

void addClientFromList(){
  if(server.hasArg("num")) {
    int _num = server.arg("num").toInt();
    clientScan.add(nameList.getMac(_num));
    
    server.send( 200, "text/json", "true");
  }else server.send( 200, "text/json", "false");
}

void setClientName() {
  if (server.hasArg("id") && server.hasArg("name")) {
    if(server.arg("name").length()>0){
      nameList.add(clientScan.getClientMac(server.arg("id").toInt()), server.arg("name"));
      server.send( 200, "text/json", "true");
    }
    else server.send( 200, "text/json", "false");
  }
}

void deleteName() {
  if (server.hasArg("num")) {
    int _num = server.arg("num").toInt();
    nameList.remove(_num);
    server.send( 200, "text/json", "true");
  }else server.send( 200, "text/json", "false");
}

void clearNameList() {
  nameList.clear();
  server.send( 200, "text/json", "true" );
}

void editClientName() {
  if (server.hasArg("id") && server.hasArg("name")) {
    nameList.edit(server.arg("id").toInt(), server.arg("name"));
    server.send( 200, "text/json", "true");
  }else server.send( 200, "text/json", "false");
}

void addClient(){
  if(server.hasArg("mac") && server.hasArg("name")){
    String macStr = server.arg("mac");
    macStr.replace(":","");
    Serial.println("add "+macStr+" - "+server.arg("name"));
    if(macStr.length() < 12 || macStr.length() > 12) server.send( 200, "text/json", "false");
    else{
      Mac _newClient;
      for(int i=0;i<6;i++){
        const char* val = macStr.substring(i*2,i*2+2).c_str();
        uint8_t valByte = strtoul(val, NULL, 16);
        Serial.print(valByte,HEX);
        Serial.print(":");
        _newClient.setAt(valByte,i);
      }
      Serial.println();
      nameList.add(_newClient,server.arg("name"));
      server.send( 200, "text/json", "true");
    }
  }
}

//==========Attack==========
void sendAttackInfo() {
  attack.sendResults();
}

void startAttack() {
  if (server.hasArg("num")) {
    int _attackNum = server.arg("num").toInt();
    if (apScan.getFirstTarget() > -1 || _attackNum == 1 || _attackNum == 2) {
      attack.start(server.arg("num").toInt());
      server.send ( 200, "text/json", "true");
    } else server.send( 200, "text/json", "false");
  }
}

void addSSID() {
  if(server.hasArg("ssid") && server.hasArg("num")){
    int num = server.arg("num").toInt();
    if(num > 0){
      ssidList.addClone(server.arg("ssid"),num);
    }else{
      ssidList.add(server.arg("ssid"));
    }
    attack.ssidChange = true;
    server.send( 200, "text/json", "true");
  }else server.send( 200, "text/json", "false");
}

void deleteSSID() {
  ssidList.remove(server.arg("num").toInt());
  attack.ssidChange = true;
  server.send( 200, "text/json", "true");
}

void randomSSID() {
  ssidList._random();
  attack.ssidChange = true;
  server.send( 200, "text/json", "true");
}

void clearSSID() {
  ssidList.clear();
  attack.ssidChange = true;
  server.send( 200, "text/json", "true");
}

void resetSSID() {
  ssidList.load();
  attack.ssidChange = true;
  server.send( 200, "text/json", "true");
}

void saveSSID() {
  ssidList.save();
  server.send( 200, "text/json", "true");
}

void restartESP() {
  server.send( 200, "text/json", "true");
  ESP.restart();
}

void enableRandom() {
  server.send( 200, "text/json", "true");
  attack.changeRandom(server.arg("interval").toInt());
}

void startDetector() {
  Serial.println("Starting Deauth Detector...");
  server.send( 200, "text/json", "true");
  detector.start();
}

//==========Settings==========
void getSettings() {
  settings.send();
}

void saveSettings() {
  if (server.hasArg("ssid")) settings.ssid = server.arg("ssid");
  if (server.hasArg("ssidHidden")) {
    if (server.arg("ssidHidden") == "false") settings.ssidHidden = false;
    else settings.ssidHidden = true;
  }
  if (server.hasArg("password")) settings.password = server.arg("password");
  if (server.hasArg("apChannel")) {
    if (server.arg("apChannel").toInt() >= 1 && server.arg("apChannel").toInt() <= 14) {
      settings.apChannel = server.arg("apChannel").toInt();
    }
  }
  if (server.hasArg("macAp")) {
    String macStr = server.arg("macAp");
    macStr.replace(":","");
    Mac tempMac;
     if(macStr.length() == 12){
       for(int i=0;i<6;i++){
         const char* val = macStr.substring(i*2,i*2+2).c_str();
         uint8_t valByte = strtoul(val, NULL, 16);
         tempMac.setAt(valByte,i);
       }
       if(tempMac.valid()) settings.macAP.set(tempMac);
     } else if(macStr.length() == 0){
       settings.macAP.set(settings.defaultMacAP);
     }
  }
  if (server.hasArg("randMacAp")) {
    if (server.arg("randMacAp") == "false") settings.isMacAPRand = false;
    else settings.isMacAPRand = true;
  }
  if (server.hasArg("ssidEnc")) {
    if (server.arg("ssidEnc") == "false") settings.attackEncrypted = false;
    else settings.attackEncrypted = true;
  }
  if (server.hasArg("scanTime")) settings.clientScanTime = server.arg("scanTime").toInt();
  if (server.hasArg("timeout")) settings.attackTimeout = server.arg("timeout").toInt();
  if (server.hasArg("deauthReason")) settings.deauthReason = server.arg("deauthReason").toInt();
  if (server.hasArg("packetRate")) settings.attackPacketRate = server.arg("packetRate").toInt();
  if (server.hasArg("apScanHidden")) {
    if (server.arg("apScanHidden") == "false") settings.apScanHidden = false;
    else settings.apScanHidden = true;
  }
  if (server.hasArg("beaconInterval")) {
    if (server.arg("beaconInterval") == "false") settings.beaconInterval = false;
    else settings.beaconInterval = true;
  }
  if (server.hasArg("useLed")) {
    if (server.arg("useLed") == "false") settings.useLed = false;
    else settings.useLed = true;
    attack.refreshLed();
  }
  if (server.hasArg("channelHop")) {
    if (server.arg("channelHop") == "false") settings.channelHop = false;
    else settings.channelHop = true;
  }
  if (server.hasArg("multiAPs")) {
    if (server.arg("multiAPs") == "false") settings.multiAPs = false;
    else settings.multiAPs = true;
  }
  if (server.hasArg("multiAttacks")) {
    if (server.arg("multiAttacks") == "false") settings.multiAttacks = false;
    else settings.multiAttacks = true;
  }
  
  if (server.hasArg("ledPin")) settings.setLedPin(server.arg("ledPin").toInt());
  if(server.hasArg("macInterval")) settings.macInterval = server.arg("macInterval").toInt();

  if (server.hasArg("darkMode")) {
    if (server.arg("darkMode") == "false") {
      settings.darkMode = false;
    } else {
      settings.darkMode = true;
    }
  }

  if (server.hasArg("simplify")) {
    if (server.arg("simplify") == "false") settings.simplify = false;
    else settings.simplify = true;
  }

  if (server.hasArg("newUser")) {
    if (server.arg("newUser") == "false") settings.newUser = false;
    else settings.newUser = true;
  }
  
  if (server.hasArg("detectorChannel")) settings.detectorChannel = server.arg("detectorChannel").toInt();

  if (server.hasArg("detectorAllChannels")) {
    if (server.arg("detectorAllChannels") == "false") settings.detectorAllChannels = false;
    else settings.detectorAllChannels = true;
  }
  
  if (server.hasArg("alertPin")) settings.alertPin = server.arg("alertPin").toInt();

  if (server.hasArg("invertAlertPin")) {
    if (server.arg("invertAlertPin") == "false") settings.invertAlertPin = false;
    else settings.invertAlertPin = true;
  }

  if (server.hasArg("detectorScanTime")) settings.detectorScanTime = server.arg("detectorScanTime").toInt();
  
  settings.save();
  server.send( 200, "text/json", "true" );
}

void resetSettings() {
  settings.reset();
  server.send( 200, "text/json", "true" );
}

void setup() {

  randomSeed(os_random());

#ifdef USE_LED16
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);
#endif
  
  Serial.begin(115200);

  attackMode = "START";

  EEPROM.begin(4096);
  SPIFFS.begin();
  
  settings.load();
  if (debug) settings.info();
  settings.syncMacInterface();
  nameList.load();
  ssidList.load();

  attack.refreshLed();

  delay(500); // Prevent bssid leak

  startWifi();
  attack.stopAll();
  attack.generate();

  /* ========== Web Server ========== */
  if (settings.newUser == 1) {
    /* Load certain files (only if newUser) */
    server.onNotFound(loadRedirectHTML);
    server.on("/js/functions.js", loadFunctionsJS);
    server.on ("/main.css", loadStyle);
    server.on ("/", loadSetupHTML);
    server.on ("/index.html", loadSetupHTML);
    server.on ("/dark.css", loadDarkModeForce);
    server.on("/ClientScanTime.json", sendClientScanTime);
    server.on("/settingsSave.json", saveSettings);
    server.on("/restartESP.json", restartESP);
    server.on("/settingsReset.json", resetSettings);
  } else {
    /* HTML */
    server.onNotFound(load404);
  
    server.on("/", loadIndexHTML);
    server.on("/index.html", loadIndexHTML);
    server.on("/users.html", loadUsersHTML);
    server.on("/attack.html", loadAttackHTML);
    server.on("/detector.html", loadDetectorHTML);
    server.on("/control.html", loadControlHTML);
    server.on("/settings.html", loadSettingsHTML);
    server.on("/info.html", loadInfoHTML);
  
    /* JS */
    server.on("/js/scan.js", loadScanJS);
    server.on("/js/users.js", loadUsersJS);
    server.on("/js/attack.js", loadAttackJS);
    server.on("/js/control.js", loadControlJS);
    server.on("/js/settings.js", loadSettingsJS);
    server.on("/js/functions.js", loadFunctionsJS);
  
    /* CSS */
    server.on ("/main.css", loadStyle);
    server.on ("/dark.css", loadDarkMode);
  
    /* JSON */
    server.on("/APScanResults.json", sendAPResults);
    server.on("/APScan.json", startAPScan);
    server.on("/APSelect.json", selectAP);
    server.on("/ClientScan.json", startClientScan);
    server.on("/ClientScanResults.json", sendClientResults);
    server.on("/ClientScanTime.json", sendClientScanTime);
    server.on("/clientSelect.json", selectClient);
    server.on("/setName.json", setClientName);
    server.on("/addClientFromList.json", addClientFromList);
    server.on("/attackInfo.json", sendAttackInfo);
    server.on("/attackStart.json", startAttack);
    server.on("/settings.json", getSettings);
    server.on("/settingsSave.json", saveSettings);
    server.on("/settingsReset.json", resetSettings);
    server.on("/deleteName.json", deleteName);
    server.on("/clearNameList.json", clearNameList);
    server.on("/editNameList.json", editClientName);
    server.on("/addSSID.json", addSSID);
    server.on("/deleteSSID.json", deleteSSID);
    server.on("/randomSSID.json", randomSSID);
    server.on("/clearSSID.json", clearSSID);
    server.on("/resetSSID.json", resetSSID);
    server.on("/saveSSID.json", saveSSID);
    server.on("/restartESP.json", restartESP);
    server.on("/addClient.json", addClient);
    server.on("/enableRandom.json", enableRandom);
    server.on("/detectorStart.json", startDetector);
  }
  server.begin();

#ifdef USE_DISPLAY
  display.init();
  display.flipScreenVertically();
  pinMode(upBtn, INPUT_PULLUP);
  pinMode(downBtn, INPUT_PULLUP);
  pinMode(selectBtn, INPUT_PULLUP);
  if(displayBtn == 0) pinMode(displayBtn, INPUT);
  else pinMode(displayBtn, INPUT_PULLUP);

  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Wi-PWN");
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 16, "Deauther");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 40, "Copyright (c) 2017");
  display.drawString(0, 50, "Sam Denty");
  display.display();

  display.setFont(Roboto_Mono_8);
  
  delay(2000);
#endif

#ifdef resetPin
  pinMode(resetPin, INPUT_PULLUP);
  if(digitalRead(resetPin) == LOW) settings.reset();
#endif

  pinMode(settings.ledPin, OUTPUT);
  digitalWrite(settings.ledPin, HIGH);

  if(debug){
    Serial.println("\nStarting...\n");
#ifndef USE_DISPLAY
    delay(2000);
    pinMode(0, INPUT);
#endif
  }
  
}

void loop() {
  if (detector.detecting) {
    detector.curTime = millis();
    if(detector.curTime - detector.prevTime >= settings.detectorScanTime){
      detector.prevTime = detector.curTime;
      Serial.println((String)detector.c+" - channel "+(String)detector.curChannel);
      
      if(detector.c >= 2){
        if(settings.invertAlertPin) digitalWrite(settings.alertPin, LOW);
        else digitalWrite(settings.alertPin, HIGH);
      }else{
        if(settings.invertAlertPin) digitalWrite(settings.alertPin, HIGH);
        else digitalWrite(settings.alertPin, LOW);
      }
      
      detector.c = 0;
      if(settings.detectorAllChannels){
        detector.curChannel++;
        if(detector.curChannel > 14) detector.curChannel = 1;
        wifi_set_channel(detector.curChannel);
      }
    }
  } else if (settings.newUser == 1) {
    #ifdef USE_CAPTIVE_PORTAL
      dnsServer.processNextRequest();
    #endif
    server.handleClient();
  } else {
    if (clientScan.sniffing) {
      if (clientScan.stop()) startWifi();
    } else {
      server.handleClient();
      attack.run();
    }
  
    if(Serial.available()){
      String input = Serial.readString();
      if(input == "reset" || input == "reset\n" || input == "reset\r" || input == "reset\r\n"){
        settings.reset();
      }
    }
  
  #ifndef USE_DISPLAY
      if(digitalRead(0) == LOW) {
        Serial.println("FLASH button (GPIO0) pressed, executing action...");
        attack.start(0);
        delay(400);
      }
  #endif
    
  
#ifdef USE_DISPLAY

  if (digitalRead(upBtn) == LOW || digitalRead(downBtn) == LOW || digitalRead(selectBtn) == LOW || digitalRead(displayBtn) == LOW){
    if(canBtnPress){
      if(digitalRead(upBtn) == LOW) buttonPressed = 0;
      else if(digitalRead(downBtn) == LOW) buttonPressed = 1;
      else if(digitalRead(selectBtn) == LOW) buttonPressed = 2;
      else if(digitalRead(displayBtn) == LOW) buttonPressed = 3;
      canBtnPress = false;
    }
  }else if(!canBtnPress){
    canBtnPress = true;
    
    // ===== UP =====
    if (buttonPressed == 0 && curRow > 0) {
      curRow--;
      if (lrow - 1 < 0) {
        lrow = rowsPerSite - 1;
        curSite--;
      } else lrow--;
  
    // ===== DOWN ===== 
    } else if (buttonPressed == 1 && curRow < rows - 1) {
      curRow++;
      if (lrow + 1 >= rowsPerSite) {
        lrow = 0;
        curSite++;
      } else lrow++;
      
    // ===== SELECT ===== 
    } else if (buttonPressed == 2) {
      
      // ===== WiFi on/off =====
      if (curRow == 0) {
        if (wifiMode == "ON") stopWifi();
        else startWifi();
      
      // ===== scan for APs ===== 
      } else if (curRow == 1) {
        startAPScan();
        drawInterface();
  
      // ===== start,stop attack ===== 
      } else if (curRow == 2) {
        if (attackMode == "START" && apScan.getFirstTarget() > -1) attack.start(0);
        else if (attackMode == "STOP") attack.stop(0);
      } 
      
      // ===== select APs ===== 
      else if (curRow >= 3) {
        attack.stop(0);
        apScan.select(curRow - 3);
      }
    }
    // ===== DISPLAY ===== 
    else if (buttonPressed == 3) {
      displayOn = !displayOn;
      display.clear();
      display.display();
    }
  }
  drawInterface();
#endif
  }
}
