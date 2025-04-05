#include <AltSoftSerial.h>
#include <EEPROM.h>
#include <math.h>

#define BAUD_RATE 38400 // Baud rate for SiMCOM communication and PC debugging communication

#define APN "internet" // APN for the SIMCOM module

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
const uint8_t imeiAddress = speedAddress + 6; // Length 15

bool startupSendEnabled = false;
uint8_t minimalDistanceDelta = 5; // minimal distance between saved coordinates and current coordinates in meters
uint8_t positionInterval = 30; // wait time between location checks in seconds

bool locationAcquired = false; // true if location was acquired from SIMCOM
bool locationDeviation = false; // true if location was changed more than minimalDistanceDelta


/**
 * 
 * @brief Initializes the Arduino Nano setup, including serial communication, modem configuration, 
 *        GPS power-up, network connection, and optional startup message transmission.
 * 
 * This function performs the following steps:
 * 1. Initializes the serial communication for debugging and modem communication.
 * 2. Sets up the LED pin as an output.
 * 3. Waits for the modem to initialize and configures the baud rate.
 * 4. Reads the modem's IMEI for identification purposes.
 * 5. Powers up the GPS module and waits for it to become operational.
 * 6. Connects to the network and refreshes the authentication token.
 * 7. Optionally sends a startup message if the `startupSendEnabled` flag is set.
 * 
 * @note This function contains blocking delays and loops, which may affect real-time performance.
 *       Ensure that the `BAUD_RATE`, `ledPin`, and other constants are properly defined before use.
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
  while (!simcomComm.available()) {
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
 * @brief Main loop function for the Arduino Nano IoT node.
 * 
 * This function continuously attempts to acquire the device's location, checks for location changes,
 * and sends the location data to a server. It includes retry mechanisms for both location acquisition
 * and data transmission, as well as a fallback mechanism to refresh the token after repeated failures.
 * 
 * The loop operates as follows:
 * - Attempts to acquire the location using `getLocation()`.
 * - If location acquisition fails, retries up to 5 times with a delay of 500ms between attempts.
 * - If the location is successfully acquired but unchanged, the loop waits for the next cycle.
 * - If the location is successfully acquired and changed, attempts to send the location using `sendLocation()`.
 * - If sending the location fails, retries up to 5 times before refreshing the token.
 * - Logs debug messages at each step to indicate success, failure, or retries.
 * - Waits for a specified interval (`positionInterval`) before starting the next cycle.
 * 
 * @note The function uses the following helper functions:
 * - `getLocation()`: Acquires the current location.
 * - `sendLocation()`: Sends the acquired location to the server.
 * - `refreshToken()`: Refreshes the authentication token after repeated failures.
 * - `wait()`: Pauses execution for the specified interval.
 * 
 * @warning If the loop fails to acquire or send the location after multiple attempts, it refreshes the token,
 * which may involve additional network operations.
 */
void loop() {
  uint8_t attempts = 0;
  do {
    getLocation();
    if (!locationAcquired) {
      DEBUG_PRINT_LN(F("--LOOP-FAIL--\nCant get location, retrying..."));
      simcomComm.println(F("AT+CGNSSPWR=1"));
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
    if (attempts > 5) {
      attempts = 0;
      DEBUG_PRINT_LN(F("-----FATAL-----\nFailed whole loop, starting refresh token\n-----FATAL-----\n"));
      refreshToken();
    }
  } while (true);
  wait(positionInterval);
}

// IMEI operations ------------------------------

/**
 * @brief Reads the IMEI (International Mobile Equipment Identity) from the SIM module.
 * 
 * This function sends the AT command "AT+CGSN" to the SIM module to request the IMEI.
 * It then reads the response from the SIM module, processes it, and extracts the IMEI.
 * The extracted IMEI is stored in the global variable `imei`.
 * 
 * @note The function uses a delay of 100ms to allow the SIM module to respond.
 * @note Ensure that the `simcomComm` object is properly initialized and connected
 *       to the SIM module before calling this function.
 * 
 * @warning This function assumes that the `extractIMEI` function is implemented
 *          and correctly extracts the IMEI from the response string.
 * 
 * @see extractIMEI
 */
void readIMEI() {
  DEBUG_PRINT_LN(F("--Method-Start--readIMEI"));
  clearBuffer();
  int attempts = 0;
  String response = "";
  while (attempts < 5) {
    simcomComm.println(F("AT+CGSN"));
    unsigned long startTime = millis();
    while (millis() - startTime < 9000) {
      if (simcomComm.available()) {
        break;
      }
    }
    while (simcomComm.available()) {
      response += char(simcomComm.read());
    }
    DEBUG_PRINT(F("--Method-Result--readIMEI\nResponse: "));
    DEBUG_PRINT_LN(response);
    response = extractIMEI(response);
    if (response.length() >= 15) {
      break;
    }
    DEBUG_PRINT_LN(F("--Method-Result--readIMEI: IMEI was not read correctly"));
    ++attempts;
  }
  if (response.length() < 15) {
    DEBUG_PRINT_LN(F("--Method-Result--readIMEI: IMEI was not read correctly after 5 attempts"));
    return;
  }
  DEBUG_PRINT_LN(F("--Method-Result--readIMEI: IMEI was read correctly"));
  saveToEEPROM(response, imeiAddress, 15);
}

/**
 * @brief Extracts the IMEI number from a given response string.
 * 
 * This function processes a response string, typically from a modem or 
 * similar device, to extract the IMEI (International Mobile Equipment Identity) 
 * number. It trims unnecessary characters and ensures the result contains 
 * only numeric digits.
 * 
 * @param response The input string containing the response, which may include 
 *                 the IMEI number and other data.
 * @return A string containing the extracted IMEI number.
 * 
 * @note The function assumes that the response starts with "AT+CGSN" if the 
 *       IMEI is included, and it removes this prefix before processing.
 * @warning Function stops processing the response if it encounters a non-digit character.
 */
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
  DEBUG_PRINT(F("--Method-Result--extractIMEI\nResponse: "));
  DEBUG_PRINT_LN(response);
  return response;
}

/**
 * @brief Establishes a connection to the network by configuring the SIMCOM module.
 * 
 * This function performs the following steps:
 * 1. Sets the network attachment mode using the AT+CGATT command.
 * 2. Configures the Access Point Name (APN) using the AT+CGDCONT command.
 * 3. Activates the Packet Data Protocol (PDP) context using the AT+CGACT command.
 * 
 * Each step waits for a confirmation response from the SIMCOM module, with a timeout of 9000 milliseconds.
 * 
 * @note Ensure that the SIMCOM module is properly initialized and connected before calling this function.
 */
void connectToNetwork() {
  DEBUG_PRINT_LN(F("--Method-Start--connectToNetwork"));
  clearBuffer();
  DETAILED_DEBUG_PRINT_LN(F("Setting network mode..."));
  simcomComm.println(F("AT+CGATT=1"));
  waitForCommandConfirmation(9000);
  DETAILED_DEBUG_PRINT_LN(F("Setting APN..."));
  simcomComm.println(String("AT+CGDCONT=1,\"IP\",\"") + APN + "\"");
  waitForCommandConfirmation(9000);
  DETAILED_DEBUG_PRINT_LN(F("Setting PDP context..."));
  simcomComm.println(F("AT+CGACT=1,1"));
  waitForCommandConfirmation(9000);
  DEBUG_PRINT_LN(F("--Method-Result--connectToNetwork"));
}

/**
 * @brief Checks the current network connection status and attempts to reconnect if disconnected.
 * 
 * This function first verifies if the device is currently connected to the network
 * by calling `isConnected()`. If the device is connected, it immediately returns `true`.
 * Otherwise, it attempts to reconnect by calling `connectToNetwork()`. After attempting
 * to reconnect, it checks the connection status again and returns `true` if the connection
 * was successfully re-established, or `false` if the connection attempt failed.
 * 
 * @return true If the device is connected to the network.
 * @return false If the device is not connected and reconnection attempts failed.
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
 * @brief Checks the network connection status of the device.
 * 
 * This function sends an AT command ("AT+CREG?") to the SIMCOM module to query 
 * the network registration status. It reads the response from the module and 
 * determines if the device is connected to the network.
 * 
 * @return true if the device is connected to the network (registered in home 
 *         or roaming network), false otherwise.
 * 
 * @note The function uses the `simcomComm` object for communication with the 
 *       SIMCOM module.
 */
bool isConnected() {
  DEBUG_PRINT_LN(F("--Method-Start--isConnected"));
  clearBuffer();
  simcomComm.println(F("AT+CREG?"));
  String response = "";
  while (simcomComm.available()) {
    response += (char)simcomComm.read();
  }
  if (response.indexOf(F("+CREG: 0,1")) > -1 ||
      response.indexOf(F("+CREG: 0,5")) > -1) {
        DEBUG_PRINT_LN(F("--Method-Result--isConnected: Connected"));
        return true;
  }
  DEBUG_PRINT_LN(F("--Method-Result--isConnected: Not connected"));
  return false;
}

// Token operations -----------------------------

/**
 * @brief Refreshes the authentication token by invoking the token management process.
 * 
 * This function attempts to refresh the token by repeatedly calling the `tokenManagement` 
 * function until it succeeds. If the token management cycle fails, it waits for 5 seconds before retrying.
 * 
 * @note This function blocks execution until the token management process completes successfully.
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
 * @brief Manages the retrieval and verification of a token.
 * 
 * This function handles the process of obtaining a new token if necessary,
 * verifying its validity, and ensuring it is stored correctly. It retries
 * token retrieval and verification a specified number of times before
 * reporting failure.
 * 
 * @return true If the token is successfully retrieved and verified.
 * @return false If the token retrieval or verification fails.
 * 
 * The function performs the following steps:
 * 1. Attempts to request a new token until successful.
 * 2. Reads the token from EEPROM storage.
 * 3. Verifies the token up to a maximum number of attempts.
 */
bool tokenManagement() {
  DEBUG_PRINT_LN(F("--Method-Start--tokenManagement"));
  while (!requestNewToken()) {
    DEBUG_PRINT_LN(F("Failed to retrieve a valid token. Retrying..."));
    wait(3);
  }
  uint8_t maxVerifyAttempts = 5;
  while (maxVerifyAttempts > 0) {
    if (verifyToken()) {
      DEBUG_PRINT_LN(F("---Method-Result--tokenManagement: Verified successfully"));
      return true;
    } else {
     DEBUG_PRINT_LN(F("--Method-Result--tokenManagement: Verification failed"));
      return false;
    }
    --maxVerifyAttempts;
    wait(3);
  }
  DEBUG_PRINT_LN(F("--Method-Result--tokenManagement: Failed to verify token"));
  return false;
}

/**
 * @brief Requests a new token from the server and updates device settings based on the response.
 * 
 * This function initializes an HTTP session, sends a request to the server to retrieve a new token,
 * and processes the server's response. If successful, the token and other configuration parameters
 * are updated and saved to EEPROM.
 * 
 * @return true if a new token is successfully retrieved and processed, false otherwise.
 * 
 * @details
 * - Sends an HTTP POST request to the server with the device's IMEI in JSON format.
 * - Parses the server's response to extract the token and other configuration parameters:
 *   - `position_check_freq`: Updates the interval for position checks (capped at 255).
 *   - `min_distance_delta`: Updates the minimum distance delta for position updates (capped at 255).
 *   - `manual_start`: Determines whether manual startup is enabled.
 * - Saves the retrieved token to EEPROM for persistent storage.
 * - Handles errors and logs debug information during the process.
 * 
 * @note The function uses a SIMCOM communication module for HTTP operations.
 * @note Debugging information is printed using `DEBUG_PRINT` and `DETAILED_DEBUG_PRINT` macros.
 * @note The function assumes the presence of helper functions like `clearBuffer`, `waitForCommandConfirmation`,
 *   `parseJSON`, and `saveToEEPROM`.
 * @note The HTTP session is terminated regardless of success or failure.
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
  String imei = readFromEEPROM(imeiAddress, 15);
  if (imei.length() < 15) {
    DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: IMEI was not read correctly"));
    readIMEI();
    imei = readFromEEPROM(imeiAddress, 15);
    if (imei.length() < 15) {
      DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: IMEI was not read correctly after 5 attempts"));
      return false;
    }
  }
  String httpString = "{\"imei\":\"" + imei + "\"}";
  simcomComm.print(F("AT+HTTPDATA="));
  simcomComm.print(httpString.length());
  simcomComm.println(F(",10000"));
  delay(50);
  simcomComm.println(httpString);
  delay(50);
  clearBuffer();
  DETAILED_DEBUG_PRINT(F("Sending request to server: "));
  DETAILED_DEBUG_PRINT_LN(httpString);
  simcomComm.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  httpString = "";
  delay(500);
  while (simcomComm.available()) {
    httpString += (char)simcomComm.read();
  }
  DEBUG_PRINT(F("Response from server in request token: "));
  DEBUG_PRINT_LN(httpString);
  if (httpString.indexOf(F("+HTTPACTION: ")) != -1) {
    if (httpString.indexOf(F("+HTTPACTION: 1,200,")) > -1) {
      String responseLength;
      for (uint8_t i = httpString.lastIndexOf(F(",")) + 1; i < httpString.length(); ++i) {
        responseLength += httpString[i];
      }
      clearBuffer();
      DETAILED_DEBUG_PRINT(F("Response length: "));
      DETAILED_DEBUG_PRINT_LN(responseLength);
      httpString = "";
      DETAILED_DEBUG_PRINT(F("Reading response..."));
      simcomComm.print(F("AT+HTTPREAD="));
      simcomComm.println(responseLength);
      delay(250);
      while (simcomComm.available()) {
        httpString += (char)simcomComm.read();
      }
      DEBUG_PRINT(F("Response: "));
      DEBUG_PRINT_LN(httpString);
      simcomComm.println(F("AT+HTTPTERM"));
      waitForCommandConfirmation(12000);
      if (httpString.indexOf(F("{")) != -1) {
        httpString.remove(0, httpString.indexOf(F("{")) - 1);
        String token = parseJSON(httpString, "token");
        DEBUG_PRINT(F("New token: "));
        DEBUG_PRINT_LN(token);
        if (token.length() > 0) {
          saveToEEPROM(token, tokenAddress, tokenMaxLength);
          int value = parseJSON(httpString, "position_check_freq").toInt();
          if (value >= 0) {
            positionInterval = (value > 255) ? 255 : value;
          }
          token.remove(0, token.length());
          value = parseJSON(httpString, "min_distance_delta").toInt();
          if (value >= 0) {
            minimalDistanceDelta = (value > 255) ? 255 : value;
          }
          token = parseJSON(httpString, "manual_start");
          if (token.startsWith("true")) {
            startupSendEnabled = true;
          } else {
            startupSendEnabled = false;
          }
          DEBUG_PRINT(F("Minimal move distance: "));
          DEBUG_PRINT_LN(String(minimalDistanceDelta));
          DEBUG_PRINT(F("Position Interval: "));
          DEBUG_PRINT_LN(String(positionInterval));
          DEBUG_PRINT(F("Manual startup: "));
          DEBUG_PRINT_LN(token);
          DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: Token retrieved successfully"));
          return true;
        }
      }
    }
    if (httpString.indexOf(F("+HTTPACTION: 1,400,")) > -1) {
      DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: Invalid IMEI"));
      readIMEI();
      return false;
    }
  }
  simcomComm.println(F("AT+HTTPTERM"));
  waitForCommandConfirmation(12000);
  DEBUG_PRINT_LN(F("--Method-Result--requestNewToken: Failed to get token"));
  return false;
}

/**
 * @brief Verifies the token by sending an HTTP request to the server.
 * 
 * This function initializes an HTTP session, sets the necessary parameters,
 * and sends a JSON payload containing the token and IMEI to the server for verification.
 * It processes the server's response to determine if the token is valid.
 * 
 * @return true if the token is successfully verified (HTTP 200 response), false otherwise.
 * 
 * @note The function uses the SIMCOM module for HTTP communication and assumes
 *       that the `simcomComm` object is properly initialized and configured.
 * @note The function also interacts with EEPROM to save the token upon successful verification.
 * 
 * @details
 * - Sends an HTTP POST request to the URL "http://api.vehiclemap.xyz/verify_token".
 * - The JSON payload includes:
 *   - `token`: The token to be verified.
 *   - `imei`: The IMEI of the device.
 * - Waits for command confirmations and processes the response from the server.
 * - If the response contains "+HTTPACTION: 1,200,", the token is considered verified.
 * - Terminates the HTTP session after processing the response.
 * 
 * @warning Ensure that the `token` and `imei` variables are properly initialized
 *          before calling this function.
 * @warning The function uses blocking delays and may not be suitable for time-critical applications.
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
  String httpString = "{\"token\":\"" + readFromEEPROM(tokenAddress, tokenMaxLength) + "\", \"imei\":\"" + readFromEEPROM(imeiAddress, 15) + "\"}";
  simcomComm.print(F("AT+HTTPDATA="));
  simcomComm.print(httpString.length());
  simcomComm.println(F(",10000"));
  delay(50);
  simcomComm.println(httpString);
  delay(50);
  clearBuffer();
  DETAILED_DEBUG_PRINT(F("Sending request to server: "));
  DETAILED_DEBUG_PRINT_LN(httpString);
  simcomComm.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  httpString = "";
  delay(500);
  while (simcomComm.available()) {
    httpString += (char)simcomComm.read();
  }
  DEBUG_PRINT(F("Response from server in verify token: "));
  DEBUG_PRINT_LN(httpString);
  simcomComm.println(F("AT+HTTPTERM"));
  waitForCommandConfirmation(12000);
  if (httpString.indexOf(F("+HTTPACTION: ")) != -1) {
    if (httpString.indexOf(F("+HTTPACTION: 1,200,")) > -1) {
      DEBUG_PRINT_LN(F("---Method-Result--verifyToken: Was verified"));
      return true;
    }
  }
  DEBUG_PRINT_LN(F("--Method-Result--verifyToken: Was NOT verified"));
  return false;
}

/**
 * @brief Parses a JSON string to extract the value associated with a given key.
 * 
 * This function takes a JSON-formatted string and a key, and attempts to extract
 * the value corresponding to the key. The value can be either a string (enclosed
 * in double quotes) or a non-string value (e.g., a number or boolean).
 * 
 * @param jsonString The JSON string to parse. It should be properly formatted.
 * @param key The key whose associated value needs to be extracted.
 * @return A String containing the value associated with the key, or an empty
 *         string if the key is not found or parsing fails.
 * 
 * @note The function assumes that the JSON string is simple and does not handle
 *       nested objects or arrays. It also does not validate the JSON format.
 * 
 * @example
 * String json = "{\"temperature\":25,\"status\":\"ok\"}";
 * String value = parseJSON(json, "temperature"); // Returns "25"
 * String status = parseJSON(json, "status");     // Returns "ok"
 */
String parseJSON(String jsonString, String key) {
  DETAILED_DEBUG_PRINT(F("--Method-Start--parseJSON\nString: "));
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
      endIndex = jsonString.indexOf(F("\""), startIndex);
    } else {
      endIndex = jsonString.indexOf(F(","), startIndex);
      if (endIndex == -1) {
        endIndex = jsonString.indexOf(F("}"), startIndex);
      }
    }
    if (endIndex > startIndex) {
      DETAILED_DEBUG_PRINT(F("--Method-Result--parseJSON: "));
      DETAILED_DEBUG_PRINT_LN(jsonString.substring(startIndex, endIndex));
      return jsonString.substring(startIndex, endIndex);
    }
  }
  DETAILED_DEBUG_PRINT(F("--Method-Result--parseJSON: cannot find key: "));
  DETAILED_DEBUG_PRINT_LN(key);
  return "";
}

// Receiving coords and sending data to server -----------------------

/**
 * @brief Sends a startup message to the server using HTTP commands via a SIMCOM module.
 * 
 * This function initializes the HTTP service, sets the URL and content type, 
 * sends a JSON payload containing a token, and performs an HTTP POST action. 
 * It then reads the server's response to determine if the message was sent successfully.
 * 
 * @return true if the server responds with HTTP status 200, indicating success.
 * @return false if the server response indicates failure or if an error occurs during the process.
 * 
 * @note The function uses a global `simcomComm` object for communication with the SIMCOM module 
 *       and a global `token` variable for authentication.
 * @note Debug messages are printed using `DEBUG_PRINT` and `DEBUG_PRINT_LN` macros.
 * @note The function includes delays and waits for command confirmations to ensure proper communication.
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
  String httpString = "{\"token\":\"" + readFromEEPROM(tokenAddress, tokenMaxLength) + "\"}";
  simcomComm.print(F("AT+HTTPDATA="));
  simcomComm.print(httpString.length());
  simcomComm.println(F(",10000"));
  delay(50);
  simcomComm.println(httpString);
  delay(50);
  clearBuffer();
  simcomComm.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  httpString = "";
  delay(500);
  while (simcomComm.available()) {
    httpString += (char)simcomComm.read();
  }
  DEBUG_PRINT(F("Response from server in startup message: "));
  DEBUG_PRINT_LN(httpString);
  simcomComm.println(F("AT+HTTPTERM"));
  waitForCommandConfirmation(12000);
  if (httpString.indexOf(F("+HTTPACTION: 1,200,")) > -1) {
    DEBUG_PRINT_LN(F("--Method-Result--sendStartupMessage: Sent successfully"));
    return true;
  }
  if (httpString.indexOf(F("+HTTPACTION: 1,401,")) > -1) {
    DEBUG_PRINT_LN(F("--Method-Result--sendStartupMessage: Invalid token"));
    refreshToken();
    return false;
  }
  DEBUG_PRINT_LN(F("--Method-Result--sendStartupMessage: Was NOT sent"));
  return false;
}

/**
 * @brief Retrieves the current GPS location, processes the data, and stores it if necessary.
 * 
 * This function communicates with a SIMCOM module to acquire the current location, speed, 
 * and other related data. It validates the received data, checks for significant location 
 * changes, and stores the new location in EEPROM if a deviation is detected.
 * 
 * @return void
 * 
 * @details
 * - Sends the "AT+CGNSSINFO" command to the SIMCOM module to request location data.
 * - Parses the response to extract latitude, longitude, and speed.
 * - Validates the extracted data to ensure it is complete and accurate.
 * - Compares the current location with the last saved location to determine if there 
 *   is a significant change (based on `minimalDistanceDelta`).
 * - Updates the EEPROM with the new location and speed if a deviation is detected.
 * 
 * @note The function uses global variables `locationAcquired` and `locationDeviation` 
 *   to indicate the status of the location acquisition process.
 * @note The function assumes the presence of helper functions like `calculateDistance`, 
 *   `readFromEEPROM`, and `saveToEEPROM`.
 * @note The function also assumes the existence of constants such as `latAddress`, 
 *   `lonAddress`, `speedAddress`, and `minimalDistanceDelta`.
 * 
 * @warning If the GPS response is invalid or incomplete, the function will terminate early.
 * @warning If the current location is the same as the last saved location, the function 
 *   will skip updating the EEPROM.
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
  String gpsResponse = "";
  while (simcomComm.available()) {
    gpsResponse += (char)simcomComm.read();
  }
  DETAILED_DEBUG_PRINT(F("Response from GPS: "));
  DETAILED_DEBUG_PRINT_LN(gpsResponse);
  int startIndex = gpsResponse.indexOf(F("+CGNSSINFO: "));
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
      DEBUG_PRINT_LN(F("--Method-Result--getLocation: Failed to get location"));
      return;
    }
    locationAcquired = true;
    float currentLat = lat.toFloat();
    float currentLon = lon.toFloat();
    float savedLat = readFromEEPROM(latAddress, 12).toFloat();
    float savedLon = readFromEEPROM(lonAddress, 12).toFloat();
    if (calculateDistance(currentLat, currentLon, savedLat, savedLon) <
        minimalDistanceDelta) {
      DEBUG_PRINT_LN(F("--Method-Result--getLocation: Location is the same as the last one"));
      return;
    }
    locationDeviation = true;
    saveToEEPROM(lat, latAddress, 12);
    saveToEEPROM(lon, lonAddress, 12);
    saveToEEPROM(speed, speedAddress, 6);
  }
}

/**
 * @brief Sends the current location data to a remote server using HTTP.
 * 
 * This function constructs a JSON payload containing the token, latitude, 
 * longitude, and speed, and sends it to a specified server endpoint using 
 * HTTP POST. It handles the initialization and termination of the HTTP 
 * session, as well as parsing the server's response to determine success 
 * or failure.
 * 
 * @return true if the location was sent successfully (HTTP 200 or 403 response), 
 *         false otherwise (e.g., HTTP 401 or other errors).
 * 
 * @details
 * - The function uses the SIMCOM module for HTTP communication.
 * - It reads the latitude, longitude, and speed values from EEPROM.
 * - If the server responds with HTTP 401, the function attempts to refresh the token.
 * 
 * @note Ensure that the SIMCOM module is properly initialized and connected 
 *       to the network before calling this function.
 * 
 * @warning This function blocks execution for several seconds due to delays 
 *          and waiting for command confirmations.
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
  String communicationString;
  communicationString = "{\"token\":\"" + readFromEEPROM(tokenAddress, 32) +
                        "\",\"lat\":\"" + readFromEEPROM(latAddress, 12) +
                        "\",\"lon\":\"" + readFromEEPROM(lonAddress, 12) +
                        "\",\"speed\":\"" + readFromEEPROM(speedAddress, 6) + "\"}";
  simcomComm.print(F("AT+HTTPDATA="));
  simcomComm.print(communicationString.length());
  simcomComm.println(F(",10000"));
  delay(50);
  simcomComm.println(communicationString);
  delay(50);
  clearBuffer();
  DETAILED_DEBUG_PRINT_LN(communicationString);
  DETAILED_DEBUG_PRINT(F("Sending data: "));
  simcomComm.println(F("AT+HTTPACTION=1"));
  waitForCommandConfirmation(12000);
  communicationString.remove(0, communicationString.length());
  communicationString = "";
  delay(500);
  while (simcomComm.available()) {
    communicationString += (char)simcomComm.read();
  }
  simcomComm.println(F("AT+HTTPTERM"));
  waitForCommandConfirmation(12000);
  digitalWrite(ledPin, LOW);
  DETAILED_DEBUG_PRINT(F("Response from server in send location: "));
  DETAILED_DEBUG_PRINT_LN(communicationString);
  if (communicationString.indexOf(F("+HTTPACTION: 1,200,")) > -1 ||
      communicationString.indexOf(F("+HTTPACTION: 1,403,")) > -1) {
    DEBUG_PRINT_LN(F("--Method-Result--sendLocation: Location sent successfully"));
    return true;
  }
  if (communicationString.indexOf(F("+HTTPACTION: 1,401,")) > -1) {
    DEBUG_PRINT_LN(F("--Method-Result--sendLocation: Location was NOT sent. Invalid token"));
    refreshToken();
    return false;
  }
  DEBUG_PRINT_LN(F("--Method-Result--sendLocation: Location was NOT sent"));
  return false;
}

// EEPROM operations ----------------------------

/**
 * @brief Saves a string to EEPROM.
 * 
 * This function saves a string to the EEPROM starting at the specified address
 * and for the specified length. If the string is shorter than the length, it fills
 * the remaining space with null characters.
 * 
 * @param data The string to save to EEPROM.
 * @param address The starting address in EEPROM to save to.
 * @param length The maximum length of the string to save.
 */
void saveToEEPROM(String data, uint8_t address, uint8_t length) {
  DETAILED_DEBUG_PRINT(F("--Method-Start--saveToEEPROM: Data: "));
  DETAILED_DEBUG_PRINT_LN(data);
  for (uint8_t i = 0; i < length; ++i) {
    if (i < data.length()) {
      EEPROM.write(address + i, data[i]);
    } else {
      EEPROM.write(address + i, '\0');
    }
  }
}

/**
 * @brief Reads a string from EEPROM.
 * 
 * This function reads a string from the EEPROM starting at the specified address
 * and for the specified length. It stops reading when it encounters a null character.
 * 
 * @param address The starting address in EEPROM to read from.
 * @param length The maximum length of the string to read.
 * @return String The string read from EEPROM.
 */
String readFromEEPROM(uint8_t address, uint8_t length) {
  String token = "";
  for (int i = 0; i < length; ++i) {
    char c = EEPROM.read(address + i);
    if (c == '\0') break;
    token += c;
  }
  DETAILED_DEBUG_PRINT(F("--Method-Result--readFromEEPROM: "));
  DETAILED_DEBUG_PRINT_LN(token);
  return token;
}

// Others ---------------------------------------

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
    if (simcomComm.available()) {
      String response = "";
      while (simcomComm.available()) {
        response += char(simcomComm.read());
      }
      DETAILED_DEBUG_PRINT(F("Response: "));
      DETAILED_DEBUG_PRINT_LN(response);
      if (response.indexOf(F("OK")) != -1) {
        clearBuffer();
        DEBUG_PRINT_LN(F("Modem Initialized!"));
        return;
      }
    }
    delay(100);
    DETAILED_DEBUG_PRINT(F("Not ready... "));
  }
  DEBUG_PRINT_LN(F("--Method-Result--waitForModemToInitialize: initialization failed"));
}

/**
 * @brief Checks and adjusts the baud rate of the SIMCOM communication module.
 * 
 * This function verifies if the current baud rate is correct by sending an "AT" 
 * command and waiting for a confirmation. If the baud rate is incorrect, it attempts 
 * to change it to the desired baud rate defined by the `BAUD_RATE` macro. The function 
 * performs the following steps:
 * 
 * 1. Sends an "AT" command to check the current baud rate.
 * 2. If the baud rate is incorrect, it switches to a default baud rate of 115200.
 * 3. Sends an "AT+IPR=<BAUD_RATE>" command to set the desired baud rate.
 * 4. Verifies if the baud rate change was successful by sending another "AT" command.
 * 
 * 
 * @note The function uses a retry mechanism to ensure the baud rate is set correctly.
 *       If the response contains unknown characters or the baud rate cannot be set 
 *       after multiple attempts, it changes baud rate to defined baud rate.
 * 
 * @warning Ensure that the `BAUD_RATE` macro is defined and matches the desired baud 
 *          rate for the SIMCOM module.
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
  int attempts = 0;
  String response;
  do {
    DETAILED_DEBUG_PRINT_LN(F("Changing baud rate"));
    simcomComm.println(("AT+IPR=") + String(BAUD_RATE));
    delay(10);
    response = "";
    while (simcomComm.available()) {
      response += char(simcomComm.read());
    }
    delay(250);
    DETAILED_DEBUG_PRINT(F("Response: "));
    DETAILED_DEBUG_PRINT_LN(response);
    if (attempts > 10) {
      if (response.length() > 0 && (response[0] < 'A' || response[0] > 'z')) {
        DEBUG_PRINT(F("Unknown character received in response: "));
        DEBUG_PRINT_LN(response);
        break;
      }
    }
    ++attempts;
  } while (!(response.indexOf(F("OK")) != -1 || response.indexOf(F("K")) != -1));
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
 * @brief Waits for a command confirmation from the SIMCOM module.
 * 
 * This function checks the serial buffer for a response from the SIMCOM module
 * and waits for a specified maximum time. It looks for "OK", "CGNSS" or "ERROR" in the response.
 * 
 * @param maxWaitTime The maximum time to wait for a response in milliseconds.
 * @return true if "OK" or "CGNSS" is received, false if "ERROR" is received or timeout occurs.
 */
bool waitForCommandConfirmation(int maxWaitTime) {
  delay(2);
  DEBUG_PRINT_LN(F("--Method-Start--waitForCommandConfirmation"));
  unsigned long startTime = millis();
  int attempts = 0;
  while (millis() - startTime < maxWaitTime) {
    if (simcomComm.available()) {
      String response = "";
      while (simcomComm.available()) {
        response += char(simcomComm.read());
        if (response.indexOf(F("OK")) != -1) {
          DETAILED_DEBUG_PRINT(F("Response: "));
          DETAILED_DEBUG_PRINT_LN(response);
          return true;
        }
        if (response.indexOf(F("CGNSS")) != -1) {
          DETAILED_DEBUG_PRINT(F("Response: "));
          DETAILED_DEBUG_PRINT_LN(response);
          return true;
        }
        if (response.indexOf(F("ERROR")) != -1) {
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
