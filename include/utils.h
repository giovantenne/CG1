void writeStringToEEPROM(int addrOffset, const String &strToWrite);
String formatStringToFloat(String s, short precision);
String formatStringPercChange(String s);
void deleteAllCredentials(void);
void setDefaultValues();
void deleteAllCredentials(void);
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void rootPage();
void connectClient();
bool startCP(IPAddress ip);
bool checkCoin(String testSymbol);
void saveSettings(void);
String readStringFromEEPROM(int addrOffset);
void readCoinConfig();

