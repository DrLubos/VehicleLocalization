#include <AltSoftSerial.h>
#include <EEPROM.h>
#include <math.h>

#define BAUD_RATE 38400

#define DEBUGGING_LEVEL 2

#if DEBUGGING_LEVEL == 0
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINT_LN(x)
  #define DETAILED_DEBUG_PRINT(x)
  #define DETAILED_DEBUG_PRINT_LN(x)
#elif DEBUGGING_LEVEL == 1
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINT_LN(x) Serial.println(x)
  #define DETAILED_DEBUG_PRINT(x)
  #define DETAILED_DEBUG_PRINT_LN(x)
#elif DEBUGGING_LEVEL >= 2
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINT_LN(x) Serial.println(x)
  #define DETAILED_DEBUG_PRINT(x) Serial.print(x)
  #define DETAILED_DEBUG_PRINT_LN(x) Serial.println(x)
#endif

AltSoftSerial simcomSerial;     // RX (hnedy) na D8, TX (fialovy) na D9  // Simcom: Hnedy T a Fialovy R
const uint8_t ledPin = 13;
const uint8_t tokenAddress = 0;
const uint8_t tokenMaxLength = 32;
const uint8_t latAddress = 0 + tokenMaxLength;
const uint8_t lonAddress = latAddress + 12;
const uint8_t speedAddress = lonAddress + 12; // Length 6

bool startupSendEnabled = false;
uint8_t minimalDistanceDelta = 5; // meters
uint8_t positionInterval = 15; // seconds

bool locationAcquired = false;
bool locationDeviation = false;

String token = "";
String imei = "";

int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void waitForModemToInitialize() {
  clearBuffer();
  DEBUG_PRINT_LN(F("\nWaiting for modem to initialize..."));
  unsigned long startTime = millis();
  const unsigned long timeout = 5000; // increase
  while (millis() - startTime < timeout) {
    simcomSerial.println("AT");
    delay(100);
    if (simcomSerial.available()) {
      String response = "";
      while (simcomSerial.available()) {
        response += char(simcomSerial.read());
      }
      DETAILED_DEBUG_PRINT(F("Response: "));
      DETAILED_DEBUG_PRINT_LN(response);
      if (response.indexOf("OK") != -1) {
        clearBuffer();
        DEBUG_PRINT_LN(F("Modem Initialized!"));
        return;
      }
    }
    delay(100);
    DETAILED_DEBUG_PRINT(F("Not ready... "));
  }
  DEBUG_PRINT_LN(F("\nModem initialization failed!"));
}

void setup() {
  Serial.begin(BAUD_RATE);
  simcomSerial.begin(115200);
  DEBUG_PRINT_LN(F("Starting communication at 115200 baud..."));
  pinMode(ledPin, OUTPUT);
  waitForModemToInitialize();
  delay(2000);
  DEBUG_PRINT_LN(F("Changing baud rate to 38400..."));
  simcomSerial.println(("AT+IPR=") + String(BAUD_RATE));
  simcomSerial.end();
  delay(250);
  simcomSerial.begin(BAUD_RATE);
  delay(250);
  waitForModemToInitialize();
  readIMEI();
  simcomSerial.println(F("AT+CGNSSPWR=1"));
  clearBuffer();
  while (!simcomSerial.available()) {
    delay(250);
    DETAILED_DEBUG_PRINT(F("Waiting for GPS to power up..."));
  }
  DETAILED_DEBUG_PRINT_LN(F("\nGPS powered up!"));
  connectToNetwork();
  delay(1000);
  while (!tokenManagement()) {
    DEBUG_PRINT_LN(F("-----FATAL-----\nFailed whole token management cycle. Waiting to start new cycle...\n-----FATAL-----\n"));
    wait(5);
  }
  if (startupSendEnabled) {
    while (!sendStartupMessage()) {
      if (!isConnected()) {
        checkSignalAndReconnect();
      }
    }
  }
  clearBuffer();
}

void loop() {
  while (true) {
    delay(10000);
    Serial.print(F("Free memory: "));
    Serial.println(freeMemory());
  }
  uint8_t attempts = 0;
  do {
    getLocation();
    if (!locationAcquired) {
      Serial.println(F("Cant get location, retrying..."));
      delay(200);
      continue;
    }
    if (!locationDeviation) {
      Serial.println(F("Location unchanged, waiting for next cycle..."));
      break;
    }
    if (sendLocation()) {
      Serial.println(F("Location was sent to server, waiting for next cycle..."));
      break;
    }
    Serial.println(F("Location failed to send, retrying..."));
    ++attempts;
    if (attempts > 5) {
      attempts = 0;
      while (!tokenManagement()) {
        Serial.println(F("Waiting for a valid token..."));
        wait(5);
      }
    }
  } while (true);
  wait(positionInterval);
}

// IMEI operations ------------------------------
void readIMEI() {
  clearBuffer();
  simcomSerial.println(F("AT+CGSN"));
  delay(100);
  String response = "";
  while (simcomSerial.available()) {
    response += char(simcomSerial.read());
  }
  DETAILED_DEBUG_PRINT(F("\nIMEI response: "));
  DETAILED_DEBUG_PRINT_LN(response);
  imei = extractIMEI(response);
}

String extractIMEI(String response) {
  response.trim();
  if (response.startsWith("AT+CGSN")) {
    response.remove(0, 7);
    response.trim();
  }
  for (uint8_t i = 0; i < response.length(); ++i) {
    if (!isdigit(response.charAt(i))) {
      response.remove(i, response.length() - i);
      break;
    }
  }
  DEBUG_PRINT(F("\nExtracted IMEI: "));
  DEBUG_PRINT_LN(response);
  return response;
}

bool waitForCommandConfirmation(int maxWaitTime) {
  DEBUG_PRINT(F("\nWaiting for OK..."));
  unsigned long startTime = millis();
  int attempts = 0;
  while (millis() - startTime < maxWaitTime) {
    if (simcomSerial.available()) {
      String response = "";
      while (simcomSerial.available()) {
        response += char(simcomSerial.read());
        if (response.indexOf("OK") != -1) {
          DETAILED_DEBUG_PRINT(F("Response: "));
          DETAILED_DEBUG_PRINT_LN(response);
          return true;
        }
        if (response.indexOf("ERROR") != -1) {
          DETAILED_DEBUG_PRINT(F("Response: "));
          DETAILED_DEBUG_PRINT_LN(response);
          return false;
        }
      }
      DETAILED_DEBUG_PRINT(F("Response: "));
      DETAILED_DEBUG_PRINT_LN(response);
    }
    delay(5);
    ++attempts;
  }
  DEBUG_PRINT_LN(F("Reached maximum wait time!"));
  return false;
}

void connectToNetwork() {
  DEBUG_PRINT_LN(F("\nConnecting to network..."));
  clearBuffer();
  DETAILED_DEBUG_PRINT_LN(F("Setting network mode..."));
  simcomSerial.println(F("AT+CGATT=1"));
  waitForCommandConfirmation(9000);
  DETAILED_DEBUG_PRINT_LN(F("Setting APN..."));
  simcomSerial.println(F("AT+CGDCONT=1,\"IP\",\"internet\""));
  waitForCommandConfirmation(9000);
  DETAILED_DEBUG_PRINT_LN(F("Setting PDP context..."));
  simcomSerial.println(F("AT+CGACT=1,1"));
  waitForCommandConfirmation(9000);
}

bool checkSignalAndReconnect() {
  if (isConnected()) {
    return true;
  } else {
    connectToNetwork();
    if (isConnected()) {
      return true;
    }
    return false;
  }
}

bool isConnected() {
  clearBuffer();
  simcomSerial.println(F("AT+CREG?"));
  String response = "";
  while (simcomSerial.available()) {
    response += (char)simcomSerial.read();
  }
  if (response.indexOf("+CREG: 0,1") > -1 ||
      response.indexOf("+CREG: 0,5") > -1) {
    return true;
  }
  return false;
}

// Token operations -----------------------------
bool tokenManagement() {
  DEBUG_PRINT_LN(F("\nToken management..."));
  while (!requestNewToken()) {
    DEBUG_PRINT_LN(F("Failed to retrieve a valid token. Retrying..."));
    wait(3);
  }
  uint8_t maxVerifyAttempts = 5;
  token = readFromEEPROM(tokenAddress, tokenMaxLength);
  while (maxVerifyAttempts > 0) {
    if (verifyToken()) {
      DEBUG_PRINT_LN(F("-----OK-----\nToken successfully verified.\n-----OK-----\n"));
      return true;
    } else {
     DEBUG_PRINT_LN(F("-----ERROR-----\nInvalid token detected.\n-----ERROR-----\n"));
      return false;
    }
    --maxVerifyAttempts;
    wait(3);
  }
  DEBUG_PRINT_LN(F("-----FATAL-----\nFailed to verify token.\n-----FATAL-----\n"));
  return false;
}

bool requestNewToken() {
  DEBUG_PRINT_LN(F("\nRequesting new token..."));
  clearBuffer();
  simcomSerial.println(F("AT+HTTPINIT"));
  waitForCommandConfirmation(12000);
  simcomSerial.println(F("AT+HTTPPARA=\"URL\",\"http://api.vehiclemap.xyz/request_token\""));
  waitForCommandConfirmation(12000);
  simcomSerial.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  waitForCommandConfirmation(12000);
  String httpString = "{\"imei\":\"" + imei + "\"}";
  simcomSerial.print(F("AT+HTTPDATA="));
  simcomSerial.print(httpString.length());
  simcomSerial.println(F(",10000"));
  delay(50);
  simcomSerial.println(httpString);
  delay(50);
  clearBuffer();
  DETAILED_DEBUG_PRINT(F("Sending request to server: "));
  DETAILED_DEBUG_PRINT_LN(httpString);
  simcomSerial.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  httpString = "";
  delay(1000);
  while (simcomSerial.available()) {
    httpString += (char)simcomSerial.read();
  }
  DEBUG_PRINT("Response from server in request token: ");
  DEBUG_PRINT_LN(httpString);
  if (httpString.indexOf("+HTTPACTION: ") != -1) {
    if (httpString.indexOf("+HTTPACTION: 1,200,") > -1) {
      String responseLength;
      for (uint8_t i = httpString.lastIndexOf(",") + 1; i < httpString.length(); ++i) {
        responseLength += httpString[i];
      }
      clearBuffer();
      DETAILED_DEBUG_PRINT(F("Response length: "));
      DETAILED_DEBUG_PRINT_LN(responseLength);
      httpString = "";
      DETAILED_DEBUG_PRINT(F("Reading response..."));
      simcomSerial.print(F("AT+HTTPREAD="));
      simcomSerial.println(responseLength);
      delay(250);
      while (simcomSerial.available()) {
        httpString += (char)simcomSerial.read();
      }
      DEBUG_PRINT(F("Response: "));
      DEBUG_PRINT_LN(httpString);
      simcomSerial.println(F("AT+HTTPTERM"));
      if (httpString.indexOf("{") != -1) {
        httpString.remove(0, httpString.indexOf("{") - 1);
        token = parseJSON(httpString, "token");
        DEBUG_PRINT(F("New token: "));
        DEBUG_PRINT_LN(token);
        if (token.length() > 0) {
          int value = parseJSON(httpString, "position_check_freq").toInt();
          if (value > 0) {
            positionInterval = (value > 255) ? 255 : value;
          }
          value = parseJSON(httpString, "min_distance_delta").toInt();
          if (value >= 0) {
            minimalDistanceDelta = (value > 255) ? 255 : value;
          }
          String startupResponse = parseJSON(httpString, "manual_start");
          if (startupResponse.startsWith("true")) {
            startupSendEnabled = true;
          } else {
            startupSendEnabled = false;
          }
          DEBUG_PRINT(F("Minimal move distance: "));
          DEBUG_PRINT_LN(String(minimalDistanceDelta));
          DEBUG_PRINT(F("Position Interval: "));
          DEBUG_PRINT_LN(String(positionInterval));
          saveToEEPROM(token, tokenAddress, tokenMaxLength);
          DEBUG_PRINT_LN(F("----OK-----\nToken saved to EEPROM.\n----OK-----\n"));
          return true;
        }
      }
    }
  }
  simcomSerial.println(F("AT+HTTPTERM"));
  DEBUG_PRINT_LN(F("-----ERROR-----\nFailed to retrieve new token.\n-----ERROR-----\n"));
  return false;
}

bool verifyToken() {
  DEBUG_PRINT_LN(F("\nVerifying token..."));
  clearBuffer();
  simcomSerial.println(F("AT+HTTPINIT"));
  waitForCommandConfirmation(12000);
  simcomSerial.println(F("AT+HTTPPARA=\"URL\",\"http://api.vehiclemap.xyz/verify_token\""));
  waitForCommandConfirmation(12000);
  simcomSerial.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  waitForCommandConfirmation(12000);
  String httpString = "{\"token\":\"" + token + "\", \"imei\":\"" + imei + "\"}";
  simcomSerial.print(F("AT+HTTPDATA="));
  simcomSerial.print(httpString.length());
  simcomSerial.println(F(",10000"));
  delay(50);
  simcomSerial.println(httpString);
  delay(50);
  clearBuffer();
  DETAILED_DEBUG_PRINT(F("Sending request to server: "));
  DETAILED_DEBUG_PRINT_LN(httpString);
  simcomSerial.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  httpString = "";
  delay(500);
  while (simcomSerial.available()) {
    httpString += (char)simcomSerial.read();
  }
  DEBUG_PRINT("Response from server in verify token: ");
  DEBUG_PRINT_LN(httpString);
  simcomSerial.println(F("AT+HTTPTERM"));
  waitForCommandConfirmation(12000);
  if (httpString.indexOf("+HTTPACTION: ") != -1) {
    if (httpString.indexOf("+HTTPACTION: 1,200,") > -1) {
      DEBUG_PRINT_LN(F("-----OK-----\nToken verified and saved to EEPROM.\n-----OK-----\n"));
      return true;
    }
  }
  DEBUG_PRINT_LN(F("-----ERROR-----\nToken is NOT verified.\n-----ERROR-----\n"));
  return false;
}

String parseJSON(String jsonString, String key) {
  DETAILED_DEBUG_PRINT(F("\nParsing JSON: "));
  DETAILED_DEBUG_PRINT_LN(jsonString);
  DETAILED_DEBUG_PRINT(F("Key: "));
  DETAILED_DEBUG_PRINT_LN(key);
  jsonString.trim();
  key += "\":";
  int startIndex = jsonString.indexOf(key);
  if (startIndex > -1) {
    startIndex += key.length();
    char firstChar = jsonString[startIndex];
    int endIndex;
    if (firstChar == '"') {
      ++startIndex;
      endIndex = jsonString.indexOf("\"", startIndex);
    } else {
      endIndex = jsonString.indexOf(",", startIndex);
      if (endIndex == -1) {
        endIndex = jsonString.indexOf("}", startIndex);
      }
    }
    if (endIndex > startIndex) {
      DETAILED_DEBUG_PRINT(F("Parsed value: "));
      DETAILED_DEBUG_PRINT_LN(jsonString.substring(startIndex, endIndex));
      return jsonString.substring(startIndex, endIndex);
    }
  }
  DETAILED_DEBUG_PRINT(F("Failed to parse JSON value for key: "));
  DETAILED_DEBUG_PRINT_LN(key);
  return "";
}

// Sending data to server -----------------------
bool sendStartupMessage() {
  DEBUG_PRINT_LN(F("\nSending startup message to server..."));
  simcomSerial.println(F("AT+HTTPINIT"));
  waitForCommandConfirmation(12000);
  simcomSerial.println(F("AT+HTTPPARA=\"URL\",\"http://api.vehiclemap.xyz/route\""));
  waitForCommandConfirmation(12000);
  simcomSerial.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  waitForCommandConfirmation(12000);
  String httpString = "{\"token\":\"" + token + "\"}";
  simcomSerial.print(F("AT+HTTPDATA="));
  simcomSerial.print(httpString.length());
  simcomSerial.println(F(",10000"));
  delay(50);
  simcomSerial.println(httpString);
  delay(50);
  clearBuffer();
  simcomSerial.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  httpString = "";
  delay(500);
  while (simcomSerial.available()) {
    httpString += (char)simcomSerial.read();
  }
  DEBUG_PRINT("Response from server in startup message: ");
  DEBUG_PRINT_LN(httpString);
  simcomSerial.println(F("AT+HTTPTERM"));
  if (httpString.indexOf("+HTTPACTION: 1,200,") > -1) {
    DEBUG_PRINT_LN(F("-----OK-----\nStartup message sent successfully.\n-----OK-----\n"));
    return true;
  }
  DEBUG_PRINT_LN(F("-----ERROR-----\nStartup message was NOT sent.\n-----ERROR-----\n"));
  return false;
}

void getLocation() {
  locationAcquired = false;
  locationDeviation = false;
  digitalWrite(ledPin, HIGH);
  DEBUG_PRINT_LN(F("\nGetting location..."));
  clearBuffer();
  simcomSerial.println(F("AT+CGNSSINFO"));
  waitForCommandConfirmation(9000);
  delay(100);
  String gpsResponse = "";
  while (simcomSerial.available()) {
    gpsResponse += (char)simcomSerial.read();
  }
  DETAILED_DEBUG_PRINT(F("Response from GPS: "));
  DETAILED_DEBUG_PRINT_LN(gpsResponse);
  int startIndex = gpsResponse.indexOf("+CGNSSINFO: ");
  if (startIndex != -1) {
    String lat = "";
    String lon = "";
    String speed = "";
    gpsResponse = gpsResponse.substring(startIndex + 12);
    int startIdx = 0;
    int endIdx;
    int fieldIndex = 0;
    while ((endIdx = gpsResponse.indexOf(',', startIdx)) != -1) {
      String field = gpsResponse.substring(startIdx, endIdx);
      startIdx = endIdx + 1;
      switch (fieldIndex) {
        case 5:
          lat = (field.length() == 0) ? "" : field;
          break;
        case 6:
          if (field.length() > 0 && field[0] == 'S') {
            lat = "-" + lat;
          }
          break;
        case 7:
          lon = (field.length() == 0) ? "" : field;
          break;
        case 8:
          if (field.length() > 0 && field[0] == 'W') {
            lon = "-" + lon;
          }
          break;
        case 12:
          speed = (field.length() == 0) ? "" : field;
          break;
      }
      ++fieldIndex;
    }
    gpsResponse.remove(0, gpsResponse.length());
    gpsResponse = "";
    digitalWrite(ledPin, LOW);
    if (lat.length() < 4 || lon.length() < 4) {
      DEBUG_PRINT_LN(F("-----ERROR-----\nFailed to get location.\n-----ERROR-----\n"));
      return;
    }
    locationAcquired = true;
    float currentLat = lat.toFloat();
    float currentLon = lon.toFloat();
    float savedLat = readFromEEPROM(latAddress, 12).toFloat();
    float savedLon = readFromEEPROM(lonAddress, 12).toFloat();
    if (calculateDistance(currentLat, currentLon, savedLat, savedLon) <
        minimalDistanceDelta) {
      DEBUG_PRINT_LN(F("-----WARNING-----\nLocation is the same as the last one. Skipping...\n-----WARNING-----\n"));
      return;
    }
    locationDeviation = true;
    saveToEEPROM(lat, latAddress, 12);
    saveToEEPROM(lon, lonAddress, 12);
    saveToEEPROM(speed, speedAddress, 6);
  }
}

bool sendLocation() {
  digitalWrite(ledPin, HIGH);
  DEBUG_PRINT_LN(F("\nSending location to server..."));
  clearBuffer();
  simcomSerial.println(F("AT+HTTPINIT"));
  waitForCommandConfirmation(12000);
  simcomSerial.println(F("AT+HTTPPARA=\"URL\",\"http://api.vehiclemap.xyz/location\""));
  waitForCommandConfirmation(12000);
  simcomSerial.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  waitForCommandConfirmation(12000);
  String communicationString;
  communicationString = "{\"token\":\"" + token +
                        "\",\"lat\":\"" + readFromEEPROM(latAddress, 12) +
                        "\",\"lon\":\"" + readFromEEPROM(lonAddress, 12) +
                        "\",\"speed\":\"" + readFromEEPROM(speedAddress, 6) + "\"}";
  simcomSerial.print(F("AT+HTTPDATA="));
  simcomSerial.print(communicationString.length());
  simcomSerial.println(F(",10000"));
  delay(50);
  simcomSerial.println(communicationString);
  delay(50);
  clearBuffer();
  DETAILED_DEBUG_PRINT_LN(communicationString);
  DETAILED_DEBUG_PRINT(F("Sending data: "));
  simcomSerial.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  communicationString.remove(0, communicationString.length());
  communicationString = "";
  delay(500);
  while (simcomSerial.available()) {
    communicationString += (char)simcomSerial.read();
  }
  simcomSerial.println(F("AT+HTTPTERM"));
  digitalWrite(ledPin, LOW);
  DETAILED_DEBUG_PRINT(F("Response from server in send location: "));
  DETAILED_DEBUG_PRINT_LN(communicationString);
  if (communicationString.indexOf("+HTTPACTION: 1,200,") > -1) {
    Serial.println(F("-----OK-----\nLocation sent successfully.\n-----OK-----\n"));
    return true;
  }
  Serial.print(F("-----ERROR-----\nLocation was NOT sent.\n-----ERROR-----\n"));
  return false;
}

// EEPROM operations ----------------------------
void saveToEEPROM(String data, uint8_t address, uint8_t length) {
  DETAILED_DEBUG_PRINT(F("Saving to EEPROM: "));
  DETAILED_DEBUG_PRINT_LN(data);
  for (uint8_t i = 0; i < length; ++i) {
    if (i < data.length()) {
      EEPROM.write(address + i, data[i]);
    } else {
      EEPROM.write(address + i, '\0');
    }
  }
}

String readFromEEPROM(uint8_t address, uint8_t length) {
  String token = "";
  for (int i = 0; i < length; ++i) {
    char c = EEPROM.read(address + i);
    if (c == '\0') break;
    token += c;
  }
  DETAILED_DEBUG_PRINT(F("Read from EEPROM: "));
  DETAILED_DEBUG_PRINT_LN(token);
  return token;
}

// Others ---------------------------------------
void wait(int seconds) {
  for (int i = 0; i < seconds * 2; ++i) {
    digitalWrite(ledPin, (i % 2 == 0) ? HIGH : LOW);
    delay(500);
  }
  digitalWrite(ledPin, LOW);
}

void clearBuffer() {
  while (simcomSerial.available()) {
    simcomSerial.read();
  }
}

float calculateDistance(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6378000;
  float dLat = radians(lat2 - lat1);
  float dLon = radians(lon2 - lon1);

  float a = sin(dLat / 2) * sin(dLat / 2) + cos(radians(lat1)) *
                                                cos(radians(lat2)) *
                                                sin(dLon / 2) * sin(dLon / 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  DEBUG_PRINT(F("Distance between coords: "));
  DEBUG_PRINT_LN(R * c);
  return R * c;
}