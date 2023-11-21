#include <WebServer.h>
#include <AutoConnect.h>
#include <Arduino.h>
#include <heltec.h>
#include "logo.h"
#include <WebSocketsClient.h>
#include "utils.h"

String defaultsSymbol = "btcusdt";
int defaultCoinPrecision = 2;

String zticker_version = "v2.00";

#define EEPROM_SIZE 128

const unsigned long wsTimedTaskInterval = 50;
bool hideWsDisconnected = false;
WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig Config;
WebSocketsClient client;
String value;
String newValue;
String percChange;
String valChange;
String symbol;
String receivedSymbol;
String event;
String tmp;
short separatorPosition;
int coinPrecision;
short brightness;
unsigned int pingDelayCounter;
bool updateDisplay;
bool looped;
unsigned long wsTimedTaskmDetection;

ACText(caption1, "Do you want to perform a factory reset?");
ACText(credits, "<hr>Follow me on Twitter: <a href='https://twitter.com/CryptoGadgetsIT'>@CryptoGadgetsIT</a>");
ACSubmit(save1, "Yes, reset the device", "/delconnexecute");
AutoConnectAux aux1("/delconn", "Reset", true, {caption1, save1});
AutoConnectAux aux1Execute("/delconnexecute", "Wifi reset", false);

String initialize2(AutoConnectAux&, PageArgument&);
ACText(caption2, "");
ACInput(input1, "", "Binance symbol (eg. BTCUSDT)", "", "BTCUSDT");
ACInput(input2, "", "Decimal digits (0-8)", "", "2");
ACInput(input3, "", "Display brightness (0-100)", "", "100");
ACSubmit(save2, "Save", "/setupexecute");
AutoConnectAux aux2("/setup", "Settings", true, {caption2, input1, input2, input3, save2, credits});

String initialize2(AutoConnectAux& aux, PageArgument& args) {
  String tmpSymbol;
  tmpSymbol = symbol;
  tmpSymbol.toUpperCase();
  input1.value = tmpSymbol;
  input2.value = coinPrecision;
  input3.value = brightness;
  return String();
}
AutoConnectAux aux3("/setupexecute", "", false);

void setup() {
  Serial.begin(115200);
  Serial.println();
  EEPROM.begin(EEPROM_SIZE);
  delay(2000);
  Heltec.begin(true, false, true);
  Heltec.display->init();
  /* Heltec.display->flipScreenVertically(); */
  Heltec.display->clear();
  readCoinConfig();
  Heltec.display->clear();
  Heltec.display->drawXbm(32, 0, btc_logo_width, btc_logo_height, btc_logo_bits);
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);
  Heltec.display->drawString(127, 52, zticker_version);
  Heltec.display->display();
  Server.on("/", rootPage);
  Server.on("/delconnexecute", deleteAllCredentials);
  Server.on("/setupexecute", saveSettings);
  Config.autoReconnect = true;
  /* Config.reconnectInterval = 6; */
  Config.ota = AC_OTA_BUILTIN;
  Config.title = "CryptoGadgets " + zticker_version;
  Config.apid = "ToTheMoon";
  Config.menuItems = AC_MENUITEM_CONFIGNEW | AC_MENUITEM_UPDATE;
  Config.boundaryOffset = EEPROM_SIZE;
  Portal.onDetect(startCP);
  Portal.config(Config);
  aux2.on(initialize2, AC_EXIT_AHEAD);
  Portal.join({aux2, aux1, aux1Execute, aux3});

  value = "";
  pingDelayCounter = 0;

  if (Portal.begin()) {
    Serial.println("Connected to WiFi");
    if (WiFi.localIP().toString() != "0.0.0.0") {
      Heltec.display->clear();
      Heltec.display->setFont(ArialMT_Plain_10);
      Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
      Heltec.display->drawString(64, 10, "WiFi connected:");
      Heltec.display->drawString(64, 35, WiFi.localIP().toString());
      Heltec.display->display();
      Serial.println("Connection Opened");
      delay(3000);
      if (WiFi.getMode() & WIFI_AP) {
        WiFi.softAPdisconnect(true);
        WiFi.enableAP(false);
        Serial.println("SoftAP disconnected");
      }
    }
    if(!checkCoin(symbol)){
      if(symbol != "btcusdt"){
        setDefaultValues();
      }
    }
    looped = false;
    connectClient();
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnect wifi...");
    Heltec.display->clear();
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
    Heltec.display->drawString(64, 12, "WiFi disconnected");
    Heltec.display->display();
    WiFi.reconnect();
    delay(3000);
    connectClient();
  } else {
    Portal.handleClient();
    pingDelayCounter++;
    if(pingDelayCounter == 500000) {
      Serial.println("Ping server...");
      pingDelayCounter = 0;
      connectClient();
      looped = true;
      client.loop();
      looped = false;
    } else {
      looped = true;
      client.loop();
      looped = false;
    }
  }
}

bool startCP(IPAddress ip){
  Heltec.display->clear();
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
  Heltec.display->drawString(64, 0, "Please connect to WiFi");
  Heltec.display->drawString(64, 13, "SSID: ToTheMoon");
  Heltec.display->drawString(64, 26, "Password: 12345678");
  Heltec.display->drawString(64, 39, "and browse to");
  Heltec.display->drawString(64, 52, "http://172.217.28.1");
  Heltec.display->display();
  return true;
}

void connectClient(){
  client.disconnect();
  client.beginSSL("data-stream.binance.com", 9443, "/ws/" + symbol + "@ticker/" + symbol + "@aggTrade");
  client.onEvent(webSocketEvent);
}

void rootPage() {
  Server.sendHeader("Location", "/_ac");
  Server.sendHeader("Cache-Control", "no-cache");
  Server.send(301);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      if(!hideWsDisconnected){
        Heltec.display->setFont(ArialMT_Plain_10);
        Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
        Heltec.display->drawString(1, 1, "D");
        Heltec.display->display();
        Serial.println("[WSc] Disconnected!\n");
        delay(2000);
      }
      connectClient();
      break;
    case WStype_CONNECTED:
      {
        Serial.println("[WSc] Connected");
      }
      break;
    case WStype_TEXT:
      if (millis() - wsTimedTaskmDetection > wsTimedTaskInterval) {
        wsTimedTaskmDetection = millis();
        if (looped) {
          looped = false;
          String event;
          updateDisplay = false;
          StaticJsonDocument<1024> myObject;
          deserializeJson(myObject, payload);
          event = myObject["e"].as<String>();
          if(event == "24hrTicker"){
            percChange = myObject["P"].as<String>();
            valChange = myObject["p"].as<String>();
            updateDisplay = true;
          } else if (event == "aggTrade") {
            value = myObject["p"].as<String>();
            receivedSymbol = myObject["s"].as<String>();
            updateDisplay = true;
          }
          if(updateDisplay && value != ""){
            Heltec.display->setBrightness(brightness);
            Heltec.display->clear();
            Heltec.display->setFont(ArialMT_Plain_10);
            Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
            Heltec.display->drawString(64, 0, receivedSymbol);
            Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
            Heltec.display->drawString(64, 52, "24h: " + formatStringToFloat(valChange, coinPrecision) + " " + formatStringPercChange(percChange) + "%");
            Heltec.display->setFont(ArialMT_Plain_24);
            Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
            Heltec.display->drawString(64, 20, formatStringToFloat(value, coinPrecision));
            Heltec.display->display();
          }
        }
      }
      break;
    case WStype_BIN:
      Serial.println("[WSc] get binary length: " + length);
      break;
    case WStype_ERROR:			
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

void deleteAllCredentials(void) {
  AutoConnectCredential credential;
  station_config_t config;
  uint8_t ent = credential.entries();

  Serial.println("Delete all credentials");
  while (ent--) {
    credential.load((int8_t)0, &config);
    credential.del((const char*)&config.ssid[0]);
  }

  setDefaultValues();

  char content[] = "Factory reset; Device is restarting...";
  Server.send(200, "text/plain", content);
  delay(3000);
  WiFi.disconnect(false, true);
  delay(3000);
  ESP.restart();
}

void setDefaultValues() {
  Serial.println("Setting up default values!");
  Heltec.display->clear();
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
  Heltec.display->drawString(64, 10, "Setting up");
  Heltec.display->drawString(64, 35, "default values");
  Heltec.display->display();
  symbol = defaultsSymbol;
  coinPrecision = defaultCoinPrecision;
  brightness = 50;
  EEPROM.write(0, brightness);
  EEPROM.write(sizeof(brightness), coinPrecision);
  writeStringToEEPROM(sizeof(brightness) + sizeof(coinPrecision), symbol);
  EEPROM.commit();
  delay(3000);
}


String formatStringToFloat(String s, short precision){
  separatorPosition = s.indexOf('.');
  if(precision == 0) {
    return s.substring(0, separatorPosition);
  } else {
    return s.substring(0, separatorPosition) + "." + s.substring(separatorPosition + 1, separatorPosition + 1 + precision);
  }
}

String formatStringPercChange(String s){
  tmp = formatStringToFloat(s, 2);
  if(tmp.indexOf("-") == -1)
    tmp = "+" + tmp;
  return tmp;
}

String httpGETRequest(const char* serverName) {
  HTTPClient http;
  http.begin(serverName);
  int httpResponseCode = http.GET();
  String payload = "{}";
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  return payload;
}

bool checkCoin(String testSymbol) {
  hideWsDisconnected = true;
  testSymbol.toUpperCase();
  String serverPath = "https://data.binance.com/api/v3/avgPrice?symbol=" + testSymbol;
  String payload = httpGETRequest(serverPath.c_str());
  Serial.println(payload);
  StaticJsonDocument<1024> myObject;
  deserializeJson(myObject, payload);
  hideWsDisconnected = true;
  if(myObject.containsKey("code") || payload == ""){
    return false;
  }
  return true;
}

void saveSettings(void){
  String newSymbol = Server.arg("input1");
  short newCoinPrecision = Server.arg("input2").toInt();
  short newBrightness = Server.arg("input3").toInt();
  bool coinValid = checkCoin(newSymbol);
  if(coinValid){
    if(newCoinPrecision >= 0 && newCoinPrecision <= 8 && newBrightness >= 0 && newBrightness <= 100){
      symbol = newSymbol;
      symbol.toLowerCase();
      coinPrecision = newCoinPrecision;
      newSymbol.toUpperCase();
      brightness = newBrightness;

      Serial.println("WRITE TO EEPROM");
      EEPROM.write(0, brightness);
      EEPROM.write(sizeof(brightness), coinPrecision);
      writeStringToEEPROM(sizeof(brightness) + sizeof(coinPrecision), symbol);
      EEPROM.commit();

      Heltec.display->clear();
      Heltec.display->setFont(ArialMT_Plain_16);
      Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
      Heltec.display->drawString(64, 20, newSymbol);
      Heltec.display->display();

      value = "";
      connectClient();
      Server.sendHeader("Location", "/setup?valid=true");
    }else{
      Server.sendHeader("Location", "/setup?valid=false&msg=invalidPrecision");
    }
  }else{
    Server.sendHeader("Location", "/setup?valid=false&msg=invalidSymbol");
  }
  Server.sendHeader("Cache-Control", "no-cache");
  Server.send(301);
}

void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
}

String readStringFromEEPROM(int addrOffset)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
  return String(data);
}

void readCoinConfig(){
  Serial.println("--- READ FROM EEPROM ---");
  brightness = EEPROM.read(0);
  coinPrecision = EEPROM.read(sizeof(brightness));
  symbol = readStringFromEEPROM(sizeof(brightness) + sizeof(coinPrecision));
  if(coinPrecision < 0 || coinPrecision > 8 || brightness < 0 || brightness > 100){
    setDefaultValues();
  }
}
