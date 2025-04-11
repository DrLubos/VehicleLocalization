#include <AltSoftSerial.h>
#include <math.h>

#define BAUD_RATE 38400 // Baud rate for SIMCOM communication and PC debugging communication

const char APN[] = "internet"; // APN for the SIMCOM module

char globalIMEI[16] = {0}; // Global variable to store the IMEI number
char globalToken[33] = {0}; // Global variable to store the token
char globalLat[13] = {0}; // Global variable to store the latitude
char globalLon[13] = {0}; // Global variable to store the longitude
char globalSpeed[8] = {0}; // Global variable to store the speed

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

AltSoftSerial simcomComm; // D8 = Virtual RX (Connect TX from other device), D9 = Virtual TX (Connect RX from other device)
const uint8_t ledPin = 13;
const uint8_t tokenAddress = 0; // Length 32
const uint8_t tokenMaxLength = 32;
const uint8_t latAddress = tokenMaxLength; // Length 12
const uint8_t lonAddress = latAddress + 12; // Length 12
const uint8_t speedAddress = lonAddress + 12; // Length 6

bool startupSendEnabled = false;
uint8_t minimalDistanceDelta = 5; // minimal distance between saved coordinates and current coordinates in meters
uint8_t positionInterval = 30; // wait time between location checks in seconds

bool locationAcquired = false; // true if location was acquired from SIMCOM
bool locationDeviation = false; // true if location was changed more than minimalDistanceDelta



/**
 * @brief Initializes the Arduino Nano setup, including serial communication, 
 *        modem initialization, GPS power-up, network connection, and optional 
 *        startup message transmission.
 * 
 * This function performs the following steps:
 * - Configures serial communication for debugging and modem communication.
 * - Sets up the LED pin as an output.
 * - Waits for the modem to initialize and configures the baud rate.
 * - Powers up the GPS module and waits for it to become available.
 * - Reads the device IMEI and connects to the network.
 * - Optionally sends a startup message if enabled.
 */
void setup() {
  Serial.begin(BAUD_RATE);
  simcomComm.begin(BAUD_RATE);
  DEBUG_PRINT_LN(F("--Method-Start--Setup"));
  pinMode(ledPin, OUTPUT);
  delay(3000);
  waitForModemToInitialize();
  delay(250);
  checkBaudRate();
  simcomComm.println(F("AT+CGNSSPWR=1"));
  clearBuffer();
  unsigned long startTime = millis();
  while (!simcomComm.available() && millis() - startTime < 9000) {
    delay(250);
    DETAILED_DEBUG_PRINT_LN(F("Waiting for GPS to power up..."));
  }
  DEBUG_PRINT_LN(F("GPS powered up!"));
  readIMEI();
  connectToNetwork();
  delay(1000);
  refreshToken();
  if (startupSendEnabled) {
    while (!sendStartupMessage()) {
      if (!isConnected()) {
        checkSignalAndReconnect();
      }
    }
  }
  clearBuffer();
}


/**
 * Main loop function that continuously attempts to acquire and send location data.
 * - Retries location acquisition and sending up to 5 times before refreshing the token.
 * - Handles scenarios where location is unchanged or fails to send.
 * - Waits for the specified interval before the next cycle.
 */
void loop() {
  clearBuffer();
  uint8_t attempts = 0;
  wait(positionInterval);
  do {
    getLocation();
    if (!locationAcquired) {
      DEBUG_PRINT_LN(F("--LOOP-FAIL--\nCant get location, retrying..."));
      clearBuffer();
      simcomComm.println(F("AT+CGNSSPWR=1"));
      waitForCommandConfirmation(9000);
      clearBuffer();
      delay(1000);
      continue;
    }
    if (!locationDeviation) {
      DEBUG_PRINT_LN(F("--LOOP--PASS--\nLocation unchanged, waiting for next cycle..."));
      break;
    }
    if (sendLocation()) {
      DEBUG_PRINT_LN(F("--LOOP--PASS-PASS--\nLocation was sent to server, waiting for next cycle..."));
      break;
    }
    DEBUG_PRINT_LN(F("--LOOP-PASS-FAIL--\nLocation failed to send, retrying..."));
    ++attempts;
    if (attempts > 3) {
      attempts = 0;
      DEBUG_PRINT_LN(F("-----FATAL-----\nFailed whole loop, starting refresh token\n-----FATAL-----\n"));
      refreshToken();
    }
  } while (true);
}


// IMEI operations ------------------------------


/**
 * @brief Reads the IMEI number from the SIM module and saves it to global variable.
 * 
 * This function sends the AT+CGSN command to the SIM module to retrieve the IMEI.
 * It retries up to 5 times if the IMEI is not read correctly. The IMEI is validated
 * to be 15 characters long before being saved to global variable.
 */
void readIMEI() {
  DEBUG_PRINT_LN(F("--Method-Start--readIMEI"));
  clearBuffer();
  uint8_t attempts = 0;
  char response[32] = {0};
  while (attempts < 3) {
    simcomComm.println(F("AT+CGSN"));
    unsigned long startTime = millis();
    while (millis() - startTime < 9000) {
      if (simcomComm.available()) {
        break;
      }
    }
    uint8_t idx = 0;
    while (simcomComm.available() && idx < sizeof(response) - 1) {
      char c = simcomComm.read();
      if (isPrintable(c)) {
        response[idx++] = c;
      }
    }
    response[idx] = '\0';
    DEBUG_PRINT(F("--Method-Result--readIMEI\nResponse: "));
    DEBUG_PRINT_LN(response);
    extractIMEI(response);
    if (strlen(response) == 15) {
      break;
    }
    DEBUG_PRINT_LN(F("--Method-Result--readIMEI: IMEI was not read correctly"));
    ++attempts;
    delay(1000);
  }
  if (strlen(response) < 15) {
    DEBUG_PRINT_LN(F("--Method-Result--readIMEI: IMEI was not read correctly after 5 attempts"));
    return;
  }
  DEBUG_PRINT_LN(F("--Method-Result--readIMEI: IMEI was read correctly"));
  snprintf(globalIMEI, sizeof(globalIMEI), "%s", response);
}


/**
 * @brief Extracts numeric characters (IMEI) from the input string.
 * 
 * This function filters out all non-digit characters from the input string
 * and modifies the string in place to contain only the numeric characters.
 * 
 * @param response A null-terminated string to process. The result will be stored in the same string.
 */
void extractIMEI(char* response) {
  uint8_t i = 0, j = 0;
  while (response[i] != '\0') {
    if (isdigit(response[i])) {
      response[j++] = response[i];
    }
    ++i;
  }
  response[j] = '\0';
  DEBUG_PRINT(F("--Method-Result--extractIMEI\nResponse: "));
  DEBUG_PRINT_LN(response);
}


/**
 * @brief Establishes a network connection by configuring the SIMCOM module.
 * 
 * This function sets the network mode, configures the APN, and activates the PDP context
 * to enable network connectivity for the device.
 */
void connectToNetwork() {
  DEBUG_PRINT_LN(F("--Method-Start--connectToNetwork"));
  clearBuffer();
  DETAILED_DEBUG_PRINT_LN(F("Setting network mode..."));
  simcomComm.println(F("AT+CGATT=1"));
  waitForCommandConfirmation(9000);
  DETAILED_DEBUG_PRINT_LN(F("Setting APN..."));
  char apnCommand[64] = {0};
  snprintf(apnCommand, sizeof(apnCommand), "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
  simcomComm.println(apnCommand);
  waitForCommandConfirmation(9000);
  DETAILED_DEBUG_PRINT_LN(F("Setting PDP context..."));
  simcomComm.println(F("AT+CGACT=1,1"));
  waitForCommandConfirmation(9000);
  DEBUG_PRINT_LN(F("--Method-Result--connectToNetwork"));
}


/**
 * @brief Checks the network connection status and attempts to reconnect if disconnected.
 * 
 * @return true if the device is connected to the network, false otherwise.
 */
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


/**
 * @brief Checks if the device is connected to the network.
 * 
 * Sends an AT command to query the network registration status and parses
 * the response to determine if the device is connected.
 * 
 * @return true if the device is connected to the network, false otherwise.
 */
bool isConnected() {
  DEBUG_PRINT_LN(F("--Method-Start--isConnected"));
  clearBuffer();
  simcomComm.println(F("AT+CREG?"));
  char response[32] = {0};
  uint8_t idx = 0;
  unsigned long start = millis();
  while (millis() - start < 9000 && idx < sizeof(response) - 1) {
    if (simcomComm.available()) {
      char c = simcomComm.read();
      if (isPrintable(c)) {
        response[idx++] = c;
      }
    }
  }
  response[idx] = '\0';
  if (strstr(response, "+CREG: 0,1") != NULL ||
      strstr(response, "+CREG: 0,5") != NULL) {
        DEBUG_PRINT_LN(F("--Method-Result--isConnected: Connected"));
        return true;
  }
  DEBUG_PRINT_LN(F("--Method-Result--isConnected: Not connected"));
  return false;
}


// Token operations -----------------------------


/**
 * @brief Refreshes the authentication token by repeatedly attempting to obtain a new token.
 *        Ensures the device is connected before retrying.
 */
void refreshToken() {
  DEBUG_PRINT_LN(F("--Method-Start--refreshToken"));
  while (!tokenManagement()) {
    DEBUG_PRINT_LN(F("--Method-Result--refreshToken: Failed to refresh token"));
    wait(2);
    if (!isConnected()) {
      checkSignalAndReconnect();
    }
    wait(2);
  }
}


/**
 * @brief Manages the token retrieval and verification process.
 * 
 * This function attempts to retrieve a valid token and verifies it.
 * It retries the retrieval process until successful and allows a 
 * limited number of verification attempts.
 * 
 * @return true if the token is successfully verified, false otherwise.
 */
bool tokenManagement() {
  DEBUG_PRINT_LN(F("--Method-Start--tokenManagement"));
  while (!requestNewToken()) {
    DEBUG_PRINT_LN(F("Failed to retrieve a valid token. Retrying..."));
    wait(2);
  }
  uint8_t maxVerifyAttempts = 3;
  while (maxVerifyAttempts > 0) {
    if (verifyToken()) {
      DEBUG_PRINT_LN(F("---Method-Result--tokenManagement: Verified successfully"));
      return true;
    } else {
     DEBUG_PRINT_LN(F("--Method-Result--tokenManagement: Verification failed"));
      return false;
    }
    --maxVerifyAttempts;
    wait(2);
  }
  DEBUG_PRINT_LN(F("--Method-Result--tokenManagement: Failed to verify token"));
  return false;
}


/**
 * @brief Requests a new token from the server using the device's IMEI.
 * 
 * This function initializes an HTTP connection, sends a request with the IMEI
 * to the server, and processes the server's response. If successful, it saves
 * the token and updates configuration parameters such as position interval,
 * minimal distance delta, and manual startup flag.
 * 
 * @return true if the token was successfully retrieved and processed, false otherwise.
 */
bool requestNewToken() {
  DEBUG_PRINT_LN(F("--Method-Start--requestNewToken"));
  clearBuffer();
  simcomComm.println(F("AT+HTTPINIT"));
  waitForCommandConfirmation(12000);
  simcomComm.println(F("AT+HTTPPARA=\"URL\",\"http://api.vehiclemap.xyz/request_token\""));
  waitForCommandConfirmation(12000);
  simcomComm.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  waitForCommandConfirmation(12000);
  const char* imei = globalIMEI;
  if (strlen(imei) < 15) {
    DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: IMEI is not loaded, retrying..."));
    readIMEI();
    if (strlen(imei) < 15) {
      DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: IMEI was not read correctly after 3 attempts"));
      return false;
    }
  }
  char httpField[192] = {0};
  snprintf(httpField, sizeof(httpField), "{\"imei\":\"%s\"}", imei);
  simcomComm.print(F("AT+HTTPDATA="));
  simcomComm.print(strlen(httpField));
  simcomComm.println(F(",10000"));
  delay(50);
  simcomComm.println(httpField);
  delay(50);
  DETAILED_DEBUG_PRINT(F("Sending request to server: "));
  DETAILED_DEBUG_PRINT_LN(httpField);
  if (!isConnected()) {
    checkSignalAndReconnect();
    if (!isConnected()) {
      DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: Not connected"));
      simcomComm.println(F("AT+HTTPTERM"));
      waitForCommandConfirmation(12000);
      return false;
    }
  }
  clearBuffer();
  simcomComm.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  memset(httpField, 0, sizeof(httpField));
  delay(500);
  readResponse(httpField, sizeof(httpField));
  DEBUG_PRINT(F("Response from server in request token: "));
  DEBUG_PRINT_LN(httpField);
  char* tokenStart = strstr(httpField, "+HTTPACTION: 1,200,");
  if (tokenStart != NULL) {
    char responseLength[4] = {0};
    char* lastComma = strrchr(tokenStart, ',');
    if (lastComma != NULL) {
      strncpy(responseLength, lastComma + 1, sizeof(responseLength) - 1);
    }
    clearBuffer();
    DETAILED_DEBUG_PRINT(F("Response length: "));
    DETAILED_DEBUG_PRINT_LN(responseLength);
    memset(httpField, 0, sizeof(httpField));
    DETAILED_DEBUG_PRINT(F("Reading response..."));
    simcomComm.print(F("AT+HTTPREAD="));
    simcomComm.println(responseLength);
    delay(250);
    readResponse(httpField, sizeof(httpField));
    DEBUG_PRINT(F("Response: "));
    DEBUG_PRINT_LN(httpField);
    char token[33] = {0};
    char freq[4] = {0};
    char delta[4] = {0};
    char manual[6] = {0};
    simcomComm.println(F("AT+HTTPTERM"));
    waitForCommandConfirmation(12000);
    if (parseJSON(httpField, "token", token, sizeof(token))) {
      snprintf(globalToken, sizeof(globalToken), "%s", token);
    } else {
      DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: Failed to parse token"));
      return false;
    }
    if (parseJSON(httpField, "position_check_freq", freq, sizeof(freq))) {
      int value = atoi(freq);
      positionInterval = (value > 255) ? 255 : value;
    }
    if (parseJSON(httpField, "min_distance_delta", delta, sizeof(delta))) {
      int value = atoi(delta);
      minimalDistanceDelta = (value > 255) ? 255 : value;
    }
    if (parseJSON(httpField, "manual_start", manual, sizeof(manual))) {
      startupSendEnabled = (strncmp(manual, "tru", 3) == 0);
    }
    DEBUG_PRINT(F("Minimal move distance: "));
    DEBUG_PRINT_LN(minimalDistanceDelta);
    DEBUG_PRINT(F("Position Interval: "));
    DEBUG_PRINT_LN(positionInterval);
    DEBUG_PRINT(F("Manual startup: "));
    DEBUG_PRINT_LN(startupSendEnabled ? F("true") : F("false"));
    DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: Token retrieved successfully"));
    return true;
  } else if (strstr(httpField, "+HTTPACTION: 1,400,") != NULL) {
    simcomComm.println(F("AT+HTTPTERM"));
    waitForCommandConfirmation(12000);
    DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: Invalid IMEI"));
    readIMEI();
    return false;
  }
  simcomComm.println(F("AT+HTTPTERM"));
  waitForCommandConfirmation(12000);
  DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: Failed to get token"));
  return false;
}


/**
 * @brief Verifies the token by sending an HTTP request to the server.
 * 
 * This function initializes an HTTP session, prepares a JSON payload with 
 * the token and IMEI read from global variables, and sends it to the server for verification.
 * It checks the server's response to determine if the token is valid.
 * 
 * @return true if the token is successfully verified, false otherwise.
 */
bool verifyToken() {
  DEBUG_PRINT_LN(F("--Method-Start--verifyToken"));
  clearBuffer();
  simcomComm.println(F("AT+HTTPINIT"));
  waitForCommandConfirmation(12000);
  simcomComm.println(F("AT+HTTPPARA=\"URL\",\"http://api.vehiclemap.xyz/verify_token\""));
  waitForCommandConfirmation(12000);
  simcomComm.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  waitForCommandConfirmation(12000);
  char httpBuffer[80] = {0};
  const char* imei = globalIMEI;
  const char* token = globalToken;
  snprintf(httpBuffer, sizeof(httpBuffer), "{\"token\":\"%s\",\"imei\":\"%s\"}", token, imei);
  simcomComm.print(F("AT+HTTPDATA="));
  simcomComm.print(strlen(httpBuffer));
  simcomComm.println(F(",10000"));
  delay(50);
  simcomComm.println(httpBuffer);
  delay(50);
  DETAILED_DEBUG_PRINT(F("Sending request to server: "));
  DETAILED_DEBUG_PRINT_LN(httpBuffer);
  if (!isConnected()) {
    checkSignalAndReconnect();
    if (!isConnected()) {
      DEBUG_PRINT_LN(F("--Method-Result--verifyToken: Not connected"));
      simcomComm.println(F("AT+HTTPTERM"));
      waitForCommandConfirmation(12000);
      return false;
    }
  }
  clearBuffer();
  simcomComm.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  memset(httpBuffer, 0, sizeof(httpBuffer));
  delay(500);
  readResponse(httpBuffer, sizeof(httpBuffer));
  DEBUG_PRINT(F("Response from server in verify token: "));
  DEBUG_PRINT_LN(httpBuffer);
  simcomComm.println(F("AT+HTTPTERM"));
  waitForCommandConfirmation(12000);
  if (strstr(httpBuffer, "+HTTPACTION: 1,200,") != NULL) {
    DEBUG_PRINT_LN(F("---Method-Result--verifyToken: Was verified"));
    return true;
  }
  DEBUG_PRINT_LN(F("--Method-Result--verifyToken: Was NOT verified"));
  return false;
}


/**
 * @brief Parses a JSON string to extract the value associated with a given key.
 * 
 * @param json The JSON string to parse.
 * @param key The key whose value needs to be extracted.
 * @param outputBuffer The buffer to store the extracted value.
 * @param bufferLen The length of the output buffer.
 * @return true if the key is found and the value is successfully extracted, false otherwise.
 */
bool parseJSON(const char* json, const char* key, char* outputBuffer, size_t bufferLen) {
  DETAILED_DEBUG_PRINT(F("--Method-Start--parseJSON\nString: "));
  DETAILED_DEBUG_PRINT_LN(json);
  DETAILED_DEBUG_PRINT(F("Key: "));
  DETAILED_DEBUG_PRINT_LN(key);
  char searchKey[24] = {0};
  snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
  const char* keyPos = strstr(json, searchKey);
  if (!keyPos) {
    DETAILED_DEBUG_PRINT(F("--Method-Result--parseJSON: cannot find key: "));
    DETAILED_DEBUG_PRINT_LN(key);
    return false;
  }
  const char* valueStart = keyPos + strlen(searchKey);
  while (*valueStart == ' ' || *valueStart == '\"') ++valueStart;
  const char* valueEnd = valueStart;
  while (*valueEnd && *valueEnd != ',' && *valueEnd != '}' && *valueEnd != '\"') ++valueEnd;
  size_t valueLen = valueEnd - valueStart;
  if (valueLen >= bufferLen) valueLen = bufferLen - 1;
  strncpy(outputBuffer, valueStart, valueLen);
  outputBuffer[valueLen] = '\0';
  DETAILED_DEBUG_PRINT(F("--Method-Result--parseJSON: "));
  DETAILED_DEBUG_PRINT_LN(outputBuffer);
  return true;
}


// Receiving coords and sending data to server -----------------------


/**
 * @brief Sends a startup message to the server with a token for authentication.
 * 
 * This function initializes an HTTP connection, sets up the request parameters,
 * sends a JSON payload containing the token, and processes the server's response.
 * It handles token validation and refreshes the token if necessary.
 * 
 * @return true if the message was sent successfully and the server responded with HTTP 200.
 * @return false if the message was not sent successfully or the token was invalid.
 */
bool sendStartupMessage() {
  DEBUG_PRINT_LN(F("--Method-Start--sendStartupMessage"));
  clearBuffer();
  simcomComm.println(F("AT+HTTPINIT"));
  waitForCommandConfirmation(12000);
  simcomComm.println(F("AT+HTTPPARA=\"URL\",\"http://api.vehiclemap.xyz/route\""));
  waitForCommandConfirmation(12000);
  simcomComm.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  waitForCommandConfirmation(12000);
  char httpBuffer[64] = {0};
  const char* token = globalToken;
  snprintf(httpBuffer, sizeof(httpBuffer), "{\"token\":\"%s\"}", token);
  simcomComm.print(F("AT+HTTPDATA="));
  simcomComm.print(strlen(httpBuffer));
  simcomComm.println(F(",10000"));
  delay(50);
  simcomComm.println(httpBuffer);
  delay(50);
  if (!isConnected()) {
    checkSignalAndReconnect();
    if (!isConnected()) {
      DEBUG_PRINT_LN(F("--Method-Result--sendStartupMessage: Not connected"));
      simcomComm.println(F("AT+HTTPTERM"));
      waitForCommandConfirmation(12000);
      return false;
    }
  }
  clearBuffer();
  simcomComm.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  memset(httpBuffer, 0, sizeof(httpBuffer));
  delay(500);
  readResponse(httpBuffer, sizeof(httpBuffer));
  DEBUG_PRINT(F("Response from server in startup message: "));
  DEBUG_PRINT_LN(httpBuffer);
  simcomComm.println(F("AT+HTTPTERM"));
  waitForCommandConfirmation(12000);
  if (strstr(httpBuffer, "+HTTPACTION: 1,200,") != NULL) {
    DEBUG_PRINT_LN(F("--Method-Result--sendStartupMessage: Sent successfully"));
    return true;
  }
  if (strstr(httpBuffer, "+HTTPACTION: 1,401,") != NULL) {
    DEBUG_PRINT_LN(F("--Method-Result--sendStartupMessage: Invalid token"));
    refreshToken();
    return false;
  }
  DEBUG_PRINT_LN(F("--Method-Result--sendStartupMessage: Was NOT sent"));
  return false;
}


/**
 * @brief Retrieves the current GPS location, checks for deviations, and saves the data to global variables.
 * 
 * This function communicates with the GPS module to acquire the current latitude, longitude, 
 * and speed. It compares the new location with the previously saved location to detect any 
 * significant movement. If a deviation is detected, the new location and speed are saved to global variables.
 * 
 * @note The function sets `locationAcquired` and `locationDeviation` flags based on the results.
 * 
 * @return void
 */
void getLocation() {
  locationAcquired = false;
  locationDeviation = false;
  digitalWrite(ledPin, HIGH);
  DEBUG_PRINT_LN(F("--Method-Start--getLocation"));
  clearBuffer();
  simcomComm.println(F("AT+CGNSSINFO"));
  waitForCommandConfirmation(9000);
  delay(100);
  char gpsResponse[128] = {0};
  readResponse(gpsResponse, sizeof(gpsResponse));
  DETAILED_DEBUG_PRINT(F("Response from GPS: "));
  DETAILED_DEBUG_PRINT_LN(gpsResponse);
  char* startPtr = strstr(gpsResponse, "+CGNSSINFO: ");
  if (startPtr != NULL) {
    uint8_t fieldIdx = 0;
    startPtr += strlen("+CGNSSINFO: ");
    if (strlen(startPtr) < 30) {
      DEBUG_PRINT_LN(F("--Method-Result--getLocation: GNSS response too short or empty"));
      return;
    }
    char* latPtr = NULL;
    char* lonPtr = NULL;
    char* speedPtr = NULL;
    while (*startPtr != '\0') {
      char* next = strchr(startPtr, ',');
      switch (fieldIdx) {
        case 5:
          latPtr = startPtr;
          break;
        case 6:
          if (*startPtr == 'S') {
            --latPtr;
            *latPtr = '-';
          }
          break;
        case 7:
          lonPtr = startPtr;
          break;
        case 8:
          if(*startPtr == 'W') {
            --lonPtr;
            *lonPtr = '-';
          }
          break;
        case 12:
          speedPtr = startPtr;
          break;
      }
      if (next == NULL) {
        break;
      }
      startPtr = next + 1;
      ++fieldIdx;
    }
    digitalWrite(ledPin, LOW);
    if (latPtr == NULL || lonPtr == NULL) {
      DEBUG_PRINT_LN(F("--Method-Result--getLocation: Failed to get location"));
      return;
    }
    locationAcquired = true;
    char* endPtr = strchr(latPtr, ',');
    if (endPtr != NULL) {
      *endPtr = '\0';
    }
    endPtr = strchr(lonPtr, ',');
    if (endPtr != NULL) {
      *endPtr = '\0';
    }
    if (strlen(globalLat) == 0 || strlen(globalLon) == 0) {
      DEBUG_PRINT_LN(F("No previous location to compare with"));
      locationDeviation = true;
    } else if (calculateDistance(atof(latPtr), atof(lonPtr), atof(globalLat), atof(globalLon)) < minimalDistanceDelta) {
      DEBUG_PRINT_LN(F("--Method-Result--getLocation: Location is the same as the last one"));
      return;
    }
    locationDeviation = true;
    snprintf(globalLat, sizeof(globalLat), "%s", latPtr);
    snprintf(globalLon, sizeof(globalLon), "%s", lonPtr);
    if (speedPtr != NULL) {
      endPtr = strchr(speedPtr, ',');
      if (endPtr != NULL) {
        *endPtr = '\0';
      }
      snprintf(globalSpeed, sizeof(globalSpeed), "%s", speedPtr);
    } else {
      snprintf(globalSpeed, sizeof(globalSpeed), "0");
    }
  }
}


/**
 * @brief Sends the current location data to a remote server via HTTP.
 * 
 * This function initializes an HTTP session, prepares the location data
 * (including token, latitude, longitude, and speed) in JSON format, and
 * sends it to the specified server endpoint. It also handles server responses
 * and checks for success or failure.
 * 
 * @return true if the location data was sent successfully (HTTP 200 or 403).
 * @return false if the location data was not sent (e.g., invalid token or other errors).
 */
bool sendLocation() {
  digitalWrite(ledPin, HIGH);
  DEBUG_PRINT_LN(F("--Method-Start--sendLocation"));
  clearBuffer();
  simcomComm.println(F("AT+HTTPINIT"));
  waitForCommandConfirmation(12000);
  simcomComm.println(F("AT+HTTPPARA=\"URL\",\"http://api.vehiclemap.xyz/location\""));
  waitForCommandConfirmation(12000);
  simcomComm.println(F("AT+HTTPPARA=\"CONTENT\",\"application/json\""));
  waitForCommandConfirmation(12000);
  const char* token = globalToken;
  const char* lat = globalLat;
  const char* lon = globalLon;
  const char* speed = globalSpeed;
  if (strlen(token) < 16 || strlen(lat) < 3 || strlen(lon) < 3) {
    DEBUG_PRINT_LN(F("--Method-Result--sendLocation: Invalid data length"));
    simcomComm.println(F("AT+HTTPTERM"));
    waitForCommandConfirmation(12000);
    return false;
  }
  simcomComm.print(F("AT+HTTPDATA="));
  uint8_t dataLength = strlen(token) + strlen(lat) + strlen(lon) + strlen(speed) + 10 + 9 + 9 + 11 + 2;
  simcomComm.print(dataLength);
  simcomComm.println(F(",10000"));
  delay(50);
  simcomComm.print(F("{\"token\":\""));
  simcomComm.print(token);
  simcomComm.print(F("\",\"lat\":\""));
  simcomComm.print(lat);
  simcomComm.print(F("\",\"lon\":\""));
  simcomComm.print(lon);
  simcomComm.print(F("\",\"speed\":\""));
  simcomComm.print(speed);
  simcomComm.println(F("\"}"));
  waitForCommandConfirmation(10000);
  if (!isConnected()) {
    checkSignalAndReconnect();
    if (!isConnected()) {
      DEBUG_PRINT_LN(F("--Method-Result--sendLocation: Not connected"));
      simcomComm.println(F("AT+HTTPTERM"));
      waitForCommandConfirmation(12000);
      return false;
    }
  }
  clearBuffer();
  simcomComm.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  char communicationBuffer[64] = {0};
  delay(500);
  readResponse(communicationBuffer, sizeof(communicationBuffer));
  simcomComm.println(F("AT+HTTPTERM"));
  waitForCommandConfirmation(12000);
  digitalWrite(ledPin, LOW);
  DETAILED_DEBUG_PRINT(F("Response from server in send location: "));
  DETAILED_DEBUG_PRINT_LN(communicationBuffer);
  if (strstr(communicationBuffer, "+HTTPACTION: 1,200,") != NULL ||
      strstr(communicationBuffer, "+HTTPACTION: 1,403,") != NULL) {
    DEBUG_PRINT_LN(F("--Method-Result--sendLocation: Location sent successfully"));
    return true;
  }
  if (strstr(communicationBuffer, "+HTTPACTION: 1,401,") != NULL) {
    DEBUG_PRINT_LN(F("--Method-Result--sendLocation: Location was NOT sent. Invalid token"));
    refreshToken();
    return false;
  }
  DEBUG_PRINT_LN(F("--Method-Result--sendLocation: Location was NOT sent"));
  return false;
}


// Others ---------------------------------------


/**
 * @brief Reads a response from the simcomComm stream into a buffer.
 * 
 * This function reads characters from the simcomComm stream, ensuring they are
 * printable, and stores them in the provided buffer. The reading stops when the
 * buffer is full (maxLen - 1) or when no more data is available.
 * 
 * @param buffer Pointer to the character array where the response will be stored.
 * @param maxLen Maximum length of the buffer, including the null terminator.
 */
void readResponse(char* buffer, size_t maxLen) {
  uint8_t idx = 0;
  while (simcomComm.available() && idx < maxLen - 1) {
    char c = simcomComm.read();
    if (isPrintable(c)) {
      buffer[idx++] = c;
    }
  }
  buffer[idx] = '\0';
}


/**
 * @brief Waits for a specified number of seconds, blinking the LED.
 * 
 * This function blinks the LED on and off for the specified number of seconds.
 * 
 * @param seconds The number of seconds to wait.
 */
void wait(int seconds) {
  for (int i = 0; i < seconds * 2; ++i) {
    digitalWrite(ledPin, (i % 2 == 0) ? HIGH : LOW);
    delay(500);
  }
  digitalWrite(ledPin, LOW);
}


/**
 * @brief Clears the serial buffer of the SIMCOM modem.
 * 
 * This function reads all available data from the serial buffer
 * to ensure it is empty before sending new commands.
 */
void clearBuffer() {
  delay(10);
  while (simcomComm.available()) {
    simcomComm.read();
  }
}


/**
 * @brief Waits for the SIMCOM modem to initialize.
 * 
 * This function sends an AT command to the modem and waits for a response.
 * It will keep trying until it receives an "OK" response or times out.
 */
void waitForModemToInitialize() {
  clearBuffer();
  DEBUG_PRINT_LN(F("--Method-Start--waitForModemToInitialize"));
  unsigned long startTime = millis();
  const unsigned long timeout = 30000;
  while (millis() - startTime < timeout) {
    simcomComm.println(F("AT"));
    delay(100);
    unsigned long respStart = millis();
    while (!simcomComm.available() && millis() - respStart < 1000);
    if (simcomComm.available()) {
      char response[64] = {0};
      uint8_t idx = 0;
      while (simcomComm.available() && idx < sizeof(response) - 1) {
        char c = simcomComm.read();
        if (isPrintable(c)) {
          response[idx++] = c;
          response[idx] = '\0';
        }
      }
      DETAILED_DEBUG_PRINT(F("Response: "));
      DETAILED_DEBUG_PRINT_LN(response);
      if (strstr(response, "OK") != NULL) {
        clearBuffer();
        DEBUG_PRINT_LN(F("Modem Initialized!"));
        return;
      }
    }
    delay(100);
    DETAILED_DEBUG_PRINT(F("Not ready... "));
    clearBuffer();
  }
  DEBUG_PRINT_LN(F("--Method-Result--waitForModemToInitialize: initialization failed"));
}


/**
 * @brief Checks and adjusts the baud rate for communication with the SIMCOM module.
 * 
 * This function verifies if the current baud rate is correct by sending an "AT" command.
 * If the baud rate is incorrect, it attempts to change it to the predefined BAUD_RATE.
 * The function retries the adjustment process multiple times if necessary.
 */
void checkBaudRate() {
  DEBUG_PRINT_LN(F("--Method-Start--checkBaudRate"));
  clearBuffer();
  simcomComm.println(F("AT"));
  if (waitForCommandConfirmation(7000)) {
    DEBUG_PRINT_LN(F("Baud rate is correct"));
    return;
  }
  DETAILED_DEBUG_PRINT_LN(F("Starting communication with 115200 baud rate"));
  simcomComm.end();
  delay(250);
  simcomComm.begin(115200);
  delay(250);
  uint8_t attempts = 0;
  char response[64];
  do {
    memset(response, 0, sizeof(response));
    delay(100);
    DETAILED_DEBUG_PRINT_LN(F("Changing baud rate"));
    simcomComm.println(("AT+IPR=") + (BAUD_RATE));
    delay(10);
    uint8_t idx = 0;
    unsigned long respStart = millis();
    while (simcomComm.available() && idx < sizeof(response) - 1 && millis() - respStart < 5000) {
      char c = simcomComm.read();
      if (isPrintable(c)) {
        response[idx++] = c;
        response[idx] = '\0';
      }
    }
    delay(250);
    DETAILED_DEBUG_PRINT(F("Response: "));
    DETAILED_DEBUG_PRINT_LN(response);
    if (attempts > 20) {
      if (idx > 0 && (response[0] < 'A' || response[0] > 'z')) {
        DEBUG_PRINT(F("Unknown character received in response: "));
        DEBUG_PRINT_LN(response);
        break;
      }
    }
    ++attempts;
  } while (!(strstr(response, "OK") != NULL || strstr(response, "K") != NULL) && attempts < 30);
  simcomComm.end();
  delay(250);
  simcomComm.begin(BAUD_RATE);
  delay(250);
  if (waitForCommandConfirmation(7000)) {
    DEBUG_PRINT_LN(F("Baud rate changed successfully"));
  } else {
    DEBUG_PRINT_LN(F("Failed to change baud rate"));
  }
}


/**
 * @brief Waits for a command confirmation response from the SIMCOM module.
 * 
 * This function listens for specific responses ("OK", "CGNSS", or "ERROR") 
 * from the SIMCOM communication interface within a specified maximum wait time.
 * 
 * @param maxWaitTime Maximum time to wait for a response, in milliseconds.
 * @return true if a confirmation ("OK" or "CGNSS") is received, false if an 
 *         error ("ERROR") is received or the maximum wait time is reached.
 */
bool waitForCommandConfirmation(int maxWaitTime) {
  delay(2);
  DEBUG_PRINT_LN(F("--Method-Start--waitForCommandConfirmation"));
  unsigned long startTime = millis();
  char response[64] = {0};
  while (millis() - startTime < maxWaitTime) {
    if (simcomComm.available()) {
      uint8_t idx = 0;
      while (simcomComm.available() && idx < sizeof(response) - 1) {
        char c = simcomComm.read();
        if (isPrintable(c)) {
          response[idx++] = c;
          response[idx] = '\0';
          if (strstr(response, "OK") != NULL || strstr(response, "CGNSS") != NULL) {
            DETAILED_DEBUG_PRINT(F("Response: "));
            DETAILED_DEBUG_PRINT_LN(response);
            return true;
          }
          if (strstr(response, "ERROR") != NULL) {
            DETAILED_DEBUG_PRINT(F("Response: "));
            DETAILED_DEBUG_PRINT_LN(response);
            return false;
          }
        }
      }
    }
    delay(5);
  }
  DEBUG_PRINT_LN(F("--Method-Result--waitForCommandConfirmation: Reached maximum wait time"));
  return false;
}


/**
 * @brief Calculates the great-circle distance between two points on the Earth's surface.
 * 
 * This function uses the Haversine formula to compute the shortest distance over the Earth's surface
 * between two geographic coordinates specified in degrees.
 * 
 * @param lat1 Latitude of the first point in degrees.
 * @param lon1 Longitude of the first point in degrees.
 * @param lat2 Latitude of the second point in degrees.
 * @param lon2 Longitude of the second point in degrees.
 * @return float The distance between the two points in meters.
 */
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


/**
 * @brief Calculates the amount of free memory available on the Arduino.
 * 
 * This function estimates the amount of free memory by checking the difference
 * between the address of a local variable and the end of the heap.
 * 
 * @return int The amount of free memory in bytes.
 */
int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
