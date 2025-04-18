#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>



// Function declarations
void checkESPResponse();
void sendHeartbeatToESP();

// Firebase paths
#define COMMANDS_PATH "/commands"
#define SYSTEM_STATUS_PATH "/system/status"
#define USERS_PATH "/users"
#define TAGS_PATH "/tags"
#define ROOMS_PATH "/rooms"

// Global variables
unsigned long lastHeartbeatSent = 0;
unsigned long lastHeartbeatReceived = 0;
String currentLocation = "unconfigured";
#include <time.h>
#include <Adafruit_NeoPixel.h>

// Function declarations
void logBootInfo();
void connectToWiFi();
void initializeTime();
void initFirebase();
void processSerialData();
#include <EEPROM.h>

// EEPROM configuration
#define EEPROM_SIZE 512
#define EEPROM_BOOT_COUNT_ADDR 0
#define EEPROM_LAST_RESET_ADDR 4

// Firebase credentials - REPLACE WITH YOUR VALUES
#define DATABASE_URL "https://pds-studio-default-rtdb.asia-southeast1.firebasedatabase.app"
#define DATABASE_SECRET "iJ4Lm8a8M8KAsC0oe8bhU5PYdQQfaxEJyg2Bskx5"

// WiFi credentials - REPLACE WITH YOUR VALUES
#define WIFI_SSID "SUTD_Guest"
#define WIFI_PASSWORD ""

// Alternative WiFi credentials as backup
#define ALT_WIFI_SSID "HomeNetwork"
#define ALT_WIFI_PASSWORD "homepassword"

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Serial communication buffer
#define BUFFER_SIZE 256
char serialBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// Enhanced Firebase paths
#define LOCATIONS_PATH "/locations"
#define EVENTS_PATH "/events"

// New visitors rooms path
#define VISITORS_ROOMS_PATH "/energy_dashboard/visitors/rooms"
#define ROOM_NAME "Living Room"
#define USER_TIMEOUT_SECONDS 60 // Remove user if not seen for 60 seconds

// RGB LED setup (WS2812)
#define LED_PIN    8
#define LED_COUNT  1
Adafruit_NeoPixel pixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Colors for LED indicators
#define COLOR_IDLE    pixel.Color(0, 0, 50)    // Blue: System idle/ready
#define COLOR_SUCCESS pixel.Color(0, 50, 0)     // Green: Successfully uploaded to Firebase
#define COLOR_ERROR   pixel.Color(50, 0, 0)     // Red: Error occurred
#define COLOR_WIFI    pixel.Color(0, 50, 50)    // Cyan: WiFi connecting
#define COLOR_RECEIVE pixel.Color(50, 0, 50)    // Purple: Received data from Mega
#define COLOR_WARNING pixel.Color(50, 50, 0)    // Yellow: Warning condition

// NTP settings for timestamps
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 28800      // GMT+8 (Singapore)
#define DAY_LIGHT_OFFSET_SEC 0

// System status
bool firebaseInitialized = false;
unsigned long lastWiFiCheck = 0;
unsigned long lastFirebaseRetry = 0;
unsigned long systemStartTime = 0;
uint16_t rxSequence = 0;
int connectionAttempts = 0;
bool useAlternateWiFi = false;
bool timeInitialized = false;

// Statistics
uint32_t totalTagsProcessed = 0;
uint32_t successfulUploads = 0;
uint32_t failedUploads = 0;

// Function declarations
void connectToWiFi();
void initFirebase();
void processSerialData();
bool uploadTagToFirebase(const String& epcHex, const String& epcAscii, int rssi);
void updateUserLocation(const String& tagID, const String& locationID);
void logEvent(const String& eventType, const String& tagID, const String& locationID);
void sendResponse(bool success, uint16_t sequence = 0);
String getFormattedTime();
void setLED(uint32_t color, int blinkCount = 0, int blinkDuration = 200);
void updateSystemStatus();
void initializeTime();
void logBootInfo();
void checkNewCommands();
void updateCommandStatus(const char* userId, const char* status);

void setup() {
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize system start time
  systemStartTime = millis();
  
  // Initialize serial for debugging
  Serial.begin(115200);
  
  // Initialize serial for communication with Arduino UNO
  Serial1.begin(9600, SERIAL_8N1, 20, 21);  // RX=GPIO20, TX=GPIO21 for ESP32-C3 UART0
  
  // Initialize NeoPixel LED
  pixel.begin();
  pixel.clear();
  pixel.show();
  
  // Log boot information
  logBootInfo();
  
  // Set to connecting color
  setLED(COLOR_WIFI);
  
  // Connect to WiFi
  connectToWiFi();
  
  // Configure time via NTP
  initializeTime();
  if (time(nullptr) > 1000000000) {
    timeInitialized = true;
  } else {
    setLED(COLOR_WARNING, 3, 200);
    Serial.println("WARNING: Could not initialize time via NTP");
  }
  
  // Initialize Firebase
  initFirebase();
  
  // Show successful initialization with green blink
  setLED(COLOR_SUCCESS, 3, 200);
  delay(1000);
  setLED(COLOR_IDLE);
  
  // Update system status
  updateSystemStatus();
  
  // Send ready message to UNO
  Serial1.println(F("ESP32-READY"));
  Serial1.flush(); // Make sure message is sent
  Serial.println(F("Sent ESP32-READY to UNO"));
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
  
  // Check Firebase connection
  if (!Firebase.ready()) {
    if (millis() - lastFirebaseRetry > 5000) {
      initFirebase();
      lastFirebaseRetry = millis();
    }
    return;
  }
  
  // Process any incoming serial data
  processSerialData();
  
  // Update system status periodically
  if (millis() - lastWiFiCheck > 60000) {
    updateSystemStatus();
    lastWiFiCheck = millis();
  }
  
  // Check for new commands
  checkNewCommands();
  
  // Process any queued tags
  checkESPResponse();
  
  // Send heartbeat if needed (every 10 seconds to match UNO)
  if (millis() - lastHeartbeatSent > 10000) {
    sendHeartbeatToESP();
    lastHeartbeatSent = millis();
  }

  // Check for heartbeat timeout (30 seconds)
  if (millis() - lastHeartbeatReceived > 30000) {
    // No heartbeat received for 30 seconds
    setLED(COLOR_WARNING, 2, 200); // Flash yellow to indicate communication issue
    Serial.println(F("WARNING: No heartbeat from UNO for >30s"));
  }
  
  // Brief delay to prevent overwhelming the system
  delay(10);
}

void processMessage(const char* message) {
  // Skip empty messages
  if (!message || strlen(message) == 0) return;
  
  // Parse the message
  if (strncmp(message, "TAG:", 4) == 0) {
    // This is from MEGA - handle tag data
    // Format: TAG:{sequence},{checksum},EPC,ASCII,RSSI
    String data = String(message + 4); // Skip "TAG:"
    
    // Parse the sequence number
    int firstComma = data.indexOf(',');
    if (firstComma <= 0) {
      Serial.println("Malformed message: no sequence number");
      sendResponse(false);
      setLED(COLOR_ERROR, 2, 100);
      return;
    }
    
    uint16_t sequence = data.substring(0, firstComma).toInt();
    data = data.substring(firstComma + 1); // Remove sequence
    
    // Parse the checksum
    int secondComma = data.indexOf(',');
    if (secondComma <= 0) {
      Serial.println("Malformed message: no checksum");
      sendResponse(false, sequence);
      setLED(COLOR_ERROR, 2, 100);
      return;
    }
    
    uint8_t receivedChecksum = data.substring(0, secondComma).toInt();
    data = data.substring(secondComma + 1); // Remove checksum
    
    // Parse the comma-separated data
    int thirdComma = data.indexOf(',');
    int fourthComma = data.indexOf(',', thirdComma + 1);
    
    if (thirdComma > 0 && fourthComma > thirdComma) {
      String epcHex = data.substring(0, thirdComma);
      String epcAscii = data.substring(thirdComma + 1, fourthComma);
      int rssi = strtol(data.substring(fourthComma + 1).c_str(), NULL, 16);
      
      // Flash purple to show we're processing
      setLED(COLOR_RECEIVE, 2, 100);
      
      // Upload to Firebase
      bool success = uploadTagToFirebase(epcHex.c_str(), epcAscii.c_str(), rssi);
      
      // Send acknowledgment back to MEGA
      sendResponse(success, sequence);
      
      // Show success or error
      if (success) {
        setLED(COLOR_SUCCESS, 1, 500);
      } else {
        setLED(COLOR_ERROR, 1, 500);
      }
    }
  }
  else if (strcmp(message, "READY") == 0) {
    Serial.println(F("Arduino reports ready"));
  }
  else if (strcmp(message, "ACK") == 0) {
    Serial.println(F("Arduino acknowledged"));
  }
  else if (strncmp(message, "SUCCESS:", 8) == 0) {
    // This is from UNO - handle RFID write success
    const char* userId = message + 8;
    updateCommandStatus(userId, "completed");
    setLED(COLOR_SUCCESS, 2, 200);
  }
  else if (strncmp(message, "ERROR:", 6) == 0) {
    // This is from UNO - handle RFID write error
    const char* userId = message + 6;
    updateCommandStatus(userId, "failed");
    setLED(COLOR_ERROR, 2, 200);
  }
  else {
    Serial.print(F("Unknown message: "));
    Serial.println(message);
  }
  
  // Return to idle state
  setLED(COLOR_IDLE);
}

bool uploadTagToFirebase(const String& epcHex, const String& epcAscii, int rssi) {
  bool additionalSuccess = true;
  if (!firebaseInitialized || WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot upload: Firebase not initialized or WiFi disconnected");
    return false;
  }
  
  // Get current timestamp
  String formattedTime = getFormattedTime();
  time_t now;
  time(&now);
  
  // Extract user ID from ASCII text if available
  // Format is expected to be FirstInitialLastName+DOB like "JDoe000120"
  String userId = "";
  if (epcAscii.length() > 0) {
    userId = epcAscii;
  } else {
    userId = "unknown_" + epcHex.substring(0, 8);
  }
  
  // Create JSON for the tag data
  FirebaseJson tagJson;
  tagJson.set("epc", epcHex);
  tagJson.set("ascii_text", epcAscii);
  tagJson.set("rssi", rssi);
  tagJson.set("timestamp", formattedTime);
  
  // Upload tag data to Firebase
  String tagPath = "/tags/" + epcHex;
  bool tagSuccess = Firebase.RTDB.setJSON(&fbdo, tagPath.c_str(), &tagJson);
  
  if (!tagSuccess) {
    Serial.print("Failed to upload tag data: ");
    Serial.println(fbdo.errorReason().c_str());
    return false;
  }
  
  // Update room user count
  String roomPath = "/rooms/" + currentLocation;
  
  // Get current room data
  if (Firebase.RTDB.getJSON(&fbdo, roomPath.c_str())) {
    FirebaseJson* roomJson = fbdo.jsonObjectPtr();
    FirebaseJsonData activeUsers;
    roomJson->get(activeUsers, "active_users");
    
    // Update room data
    FirebaseJson updateJson;
    updateJson.set("active_users", activeUsers.intValue + 1);
    updateJson.set("last_update", (int)time(NULL));
    
    bool roomSuccess = Firebase.RTDB.updateNode(&fbdo, roomPath.c_str(), &updateJson);
    if (!roomSuccess) {
      Serial.print("Failed to update room data: ");
      Serial.println(fbdo.errorReason().c_str());
    }
  }
  logEvent("tag_detected", epcHex, currentLocation);
  
  // Update system statistics
  FirebaseJson statsJson;
  statsJson.set("total_tags_processed", totalTagsProcessed);
  statsJson.set("successful_uploads", successfulUploads);
  statsJson.set("failed_uploads", failedUploads);
  statsJson.set("last_tag", epcHex);
  statsJson.set("last_update", formattedTime);
  
  bool statsSuccess = Firebase.RTDB.updateNode(&fbdo, SYSTEM_STATUS_PATH + String("/stats"), &statsJson);
  additionalSuccess = additionalSuccess && statsSuccess;
  
  return tagSuccess;
}

bool userExists(const String& userId) {
  String userPath = String(USERS_PATH) + "/" + userId;
  // Use a temporary FirebaseData object to avoid interfering with fbdo
  FirebaseData tempFbdo;
  bool success = Firebase.RTDB.get(&tempFbdo, userPath.c_str());
  if (!success) {
    // Not found or error
    return false;
  }
  // If the node exists and has any value, consider it exists
  return tempFbdo.dataType() != "null";
}

void updateUserLocation(const String& userId, const String& roomName) {
  if (!firebaseInitialized) return;
  // Check if user exists before updating room user list
  if (!userExists(userId)) {
    setLED(COLOR_ERROR, 3, 200); // Blink red 3 times for user not found
    return;
  }
  setLED(COLOR_WIFI, 3, 200); // Blink cyan 3 times for user found

  // --- Sanitize and validate roomName and userId ---
  String cleanRoomName = roomName;
  cleanRoomName.trim();
  cleanRoomName.replace(" ", "_"); // Always use underscores for room names

  String cleanUserId = userId;
  cleanUserId.trim();

  // Check for empty strings
  if (cleanRoomName.length() == 0 || cleanUserId.length() == 0) {
    setLED(COLOR_RECEIVE, 3, 200); // Blink magenta for path error
    return;
  }

  // Check for non-alphanumeric characters (except underscore)
  bool invalidChar = false;
  for (size_t i = 0; i < cleanRoomName.length(); i++) {
    if (!isalnum(cleanRoomName[i]) && cleanRoomName[i] != '_') invalidChar = true;
  }
  for (size_t i = 0; i < cleanUserId.length(); i++) {
    if (!isalnum(cleanUserId[i]) && cleanUserId[i] != '_') invalidChar = true;
  }
  if (invalidChar) {
    setLED(pixel.Color(50, 50, 50), 3, 200); // Blink white for invalid char
    return;
  }

  // --- Build path and update Firebase ---
  String userRoomPath = String(VISITORS_ROOMS_PATH) + "/" + cleanRoomName + "/users/" + cleanUserId;
  FirebaseJson userJson;
  userJson.set("present", true);
  userJson.set("last_seen", (int)time(NULL));
  bool roomUserSuccess = Firebase.RTDB.updateNode(&fbdo, userRoomPath.c_str(), &userJson);
  if (!roomUserSuccess) {
    setLED(COLOR_WARNING, 3, 200); // Blink yellow 3 times for room update failure
    // Serial.print("Failed to update room user list: ");
    // Serial.println(fbdo.errorReason().c_str());
  }
  // Old logic for /users and /locations is now disabled/commented out
  // FirebaseJson userJson;
  // userJson.set("current_location", locationId);
  // userJson.set("last_seen", (int)time(NULL));
  // String userPath = String(USERS_PATH) + "/" + userId;
  // bool userSuccess = Firebase.RTDB.updateNode(&fbdo, userPath.c_str(), &userJson);
  // if (!userSuccess) {
  //   Serial.print("Failed to update user location: ");
  //   Serial.println(fbdo.errorReason().c_str());
  // }
  // FirebaseJson locationJson;
  // String occupantPath = "occupants/" + userId;
  // locationJson.set(occupantPath, true);
  // locationJson.set("last_activity", (int)time(NULL));
  // String locationPath = String(LOCATIONS_PATH) + "/" + locationId;
  // bool locationSuccess = Firebase.RTDB.updateNode(&fbdo, locationPath.c_str(), &locationJson);
  // if (!locationSuccess) {
  //   Serial.print("Failed to update location occupants: ");
  //   Serial.println(fbdo.errorReason().c_str());
  // }
}

void logEvent(const String& eventType, const String& tagId, const String& locationId) {
  if (!firebaseInitialized) return;
  
  // Create a unique event ID with timestamp
  String eventId = String((int)time(NULL)) + "_" + tagId.substring(0, 8);
  
  // Create event JSON
  FirebaseJson eventJson;
  eventJson.set("type", eventType);
  eventJson.set("tag_id", tagId);
  eventJson.set("location", locationId);
  eventJson.set("timestamp", getFormattedTime());
  eventJson.set("unix_time", (int)time(NULL));
  
  // Path for this event
  String eventPath = String(EVENTS_PATH) + "/" + eventId;
  
  // Upload to Firebase
  bool success = Firebase.RTDB.setJSON(&fbdo, eventPath.c_str(), &eventJson);
  
  if (!success) {
    Serial.print("Failed to log event: ");
    Serial.println(fbdo.errorReason().c_str());
  }
}

void sendResponse(bool success, uint16_t sequence) {
  // Enhanced protocol: ACK:{sequence} for success, NAK:{sequence} for failure
  if (success) {
    Serial1.print("ACK:");
    Serial1.println(sequence);
  } else {
    Serial1.print("NAK:");
    Serial1.println(sequence);
  }
}

String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String("Unknown");
  }
  
  char timeString[30];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeString);
}

void checkESPResponse() {
  while (Serial1.available()) {
    String response = Serial1.readStringUntil('\n');
    response.trim();
    
    // Check for invalid characters
    bool isValid = true;
    for (unsigned int i = 0; i < response.length(); i++) {
      if (response[i] < 32 || response[i] > 126) {
        isValid = false;
        break;
      }
    }
    
    if (!isValid) {
      Serial.println(F("Received corrupted message, discarding"));
      continue;
    }
    
    if (response.startsWith("ACK:")) {
      // Handle acknowledgment
      String ackId = response.substring(4);
      if (ackId == "HEARTBEAT") {
        // Got heartbeat acknowledgment
        lastHeartbeatReceived = millis();
        Serial.println(F("UNO acknowledged heartbeat"));
      } else {
        Serial.print(F("Received ACK from UNO: "));
        Serial.println(ackId);
      }
    } else if (response.startsWith("HEARTBEAT")) {
      // Got heartbeat from UNO, send acknowledgment
      Serial1.println(F("ACK:HEARTBEAT"));
      Serial1.flush();
      lastHeartbeatReceived = millis();
      Serial.println(F("Received heartbeat from UNO"));
    }
  }
}

void sendHeartbeatToESP() {
  Serial.println(F("Sending heartbeat to UNO..."));
  Serial1.println(F("HEARTBEAT"));
  Serial1.flush();
  lastHeartbeatSent = millis();
}

void updateSystemStatus() {
  if (!firebaseInitialized) return;
  
  // Create system status JSON
  FirebaseJson statusJson;
  
  // Basic info
  statusJson.set("status", "online");
  statusJson.set("wifi_ssid", WiFi.SSID());
  statusJson.set("wifi_strength", WiFi.RSSI());
  statusJson.set("ip_address", WiFi.localIP().toString());
  statusJson.set("uptime_seconds", millis() / 1000);
  statusJson.set("last_update", getFormattedTime());
  
  // Memory info
  statusJson.set("free_heap", ESP.getFreeHeap());
  
  // Upload to Firebase
  bool success = Firebase.RTDB.updateNode(&fbdo, SYSTEM_STATUS_PATH, &statusJson);
  
  if (!success) {
    Serial.print("Failed to update system status: ");
    Serial.println(fbdo.errorReason().c_str());
  }
}

void updateCommandStatus(const char* commandId, const char* status) {
  if (!firebaseInitialized) return;
  
  String commandPath = String(COMMANDS_PATH) + "/" + commandId + "/status";
  if (!Firebase.RTDB.setString(&fbdo, commandPath.c_str(), status)) {
    Serial.print("Failed to update command status: ");
    Serial.println(fbdo.errorReason().c_str());
  }
}

// Update the status of all pending write_rfid commands in Firebase
void updatePendingWriteCommands(const char* newStatus) {
  if (!firebaseInitialized) return;

  // Get all commands
  if (Firebase.RTDB.getJSON(&fbdo, COMMANDS_PATH)) {
    FirebaseJson *json = fbdo.jsonObjectPtr();
    if (!json) return;

    size_t len = json->iteratorBegin();
    String key, value;
    int type;
    
    for (size_t i = 0; i < len; i++) {
      json->iteratorGet(i, type, key, value);
      
      // Skip non-object entries
      if (type != FirebaseJson::JSON_OBJECT) continue;
      
      FirebaseJsonData statusData;
      FirebaseJsonData typeData;
      FirebaseJson commandJson;
      commandJson.setJsonData(value);
      commandJson.get(statusData, "status");
      commandJson.get(typeData, "type");
      
      // Only update processing write_rfid commands
      if (!statusData.success || !typeData.success) continue;
      
      if (typeData.stringValue == "write_rfid" && 
          statusData.stringValue == "processing") {
        
        // Update command status
        String statusPath = String(COMMANDS_PATH) + "/" + key + "/status";
        Firebase.RTDB.setString(&fbdo, statusPath.c_str(), newStatus);
        
        // Update timestamp
        String timestampPath = String(COMMANDS_PATH) + "/" + key + "/updated_at";
        Firebase.RTDB.setInt(&fbdo, timestampPath.c_str(), (int)time(NULL));
        
        Serial.print("Updated write_rfid command ");
        Serial.print(key);
        Serial.print(" to status: ");
        Serial.println(newStatus);
      }
    }
    json->iteratorEnd();
  }
}

void checkNewCommands() {
  if (!firebaseInitialized) return;

  // Get all commands
  if (Firebase.RTDB.getJSON(&fbdo, COMMANDS_PATH)) {
    FirebaseJson *json = fbdo.jsonObjectPtr();
    if (!json) return;

    size_t len = json->iteratorBegin();
    String key, value;
    int type;
    
    for (size_t i = 0; i < len; i++) {
      json->iteratorGet(i, type, key, value);
      
      // Skip non-object entries
      if (type != FirebaseJson::JSON_OBJECT) continue;
      
      FirebaseJsonData statusData;
      FirebaseJson commandJson;
      commandJson.setJsonData(value);
      commandJson.get(statusData, "status");
      
      // Only process pending commands
      if (!statusData.success || statusData.stringValue != "pending") continue;
      
      FirebaseJsonData typeData;
      FirebaseJsonData userIdData;
      commandJson.get(typeData, "type");
      commandJson.get(userIdData, "user_id");
      
      if (!typeData.success || !userIdData.success) continue;
      
      // Handle write_rfid command
      if (typeData.stringValue == "write_rfid") {
        // Send write command to UNO
        String command = "WRITE:" + userIdData.stringValue;
        Serial1.println(command);
        Serial1.flush(); // Make sure command is sent
        Serial.println("Sent to UNO: " + command); // Debug output
        
        // Update command status
        updateCommandStatus(key.c_str(), "processing");
        
        // Update timestamp
        String timestampPath = String(COMMANDS_PATH) + "/" + key + "/updated_at";
        Firebase.RTDB.setInt(&fbdo, timestampPath.c_str(), (int)time(NULL));
      }
    }
    json->iteratorEnd();
  }
}

void logBootInfo() {
  Serial.println("\n=== ESP32 RFID System ===\n");
  Serial.println("Version: 1.0.0");
  Serial.println("Build Date: " __DATE__ " " __TIME__);
  Serial.println("\nInitializing...");
}

void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    setLED(COLOR_WIFI, 1, 500); // Blue flash for WiFi connection
  } else {
    Serial.println("\nWiFi Connection Failed!");
    setLED(COLOR_ERROR, 3, 200); // Red flashes for error
  }
}

void initializeTime() {
  configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, NTP_SERVER);
  
  Serial.print("Waiting for time sync");
  int attempts = 0;
  while (time(nullptr) < 1000000000 && attempts < 10) {
    Serial.print(".");
    delay(500);
    attempts++;
  }
  Serial.println();
  
  if (time(nullptr) > 1000000000) {
    Serial.println("Time synchronized!");
  } else {
    Serial.println("Time sync failed!");
  }
}

void initFirebase() {
  if (firebaseInitialized) return;
  
  /* Initialize Firebase */
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  if (Firebase.ready()) {
    Serial.println("Firebase authentication successful");
    firebaseInitialized = true;
    setLED(COLOR_SUCCESS, 1, 500); // Green flash for Firebase init
  } else {
    Serial.println("Firebase authentication failed");
    setLED(COLOR_ERROR, 3, 200); // Red flashes for error
  }
}

void processSerialData() {
  static unsigned long lastDebugTime = 0;
  const unsigned long DEBUG_INTERVAL = 5000;  // Debug messages every 5 seconds
  static char buffer[128] = {0};  // Increased buffer size
  static int bufferIndex = 0;
  
  while (Serial1.available()) {
    char c = Serial1.read();
    
    // Add character to buffer if there's space
    if (bufferIndex < sizeof(buffer) - 1) {
      buffer[bufferIndex++] = c;
    }
    
    // Process complete message on newline
    if (c == '\n' || c == '\r' || bufferIndex >= sizeof(buffer) - 1) {
      buffer[bufferIndex] = '\0';  // Null terminate
      String data = String(buffer);
      data.trim();
      
      // Only process non-empty messages
      if (data.length() > 0) {
        unsigned long currentTime = millis();
        bool shouldPrintDebug = (currentTime - lastDebugTime) >= DEBUG_INTERVAL;
        
        if (shouldPrintDebug) {
          lastDebugTime = currentTime;
          Serial.print("UNO message: ");
          Serial.println(data);
        }
        
        // Process heartbeat messages - IMPORTANT FOR UNO-ESP CONNECTIVITY
        if (data.indexOf("HEARTBEAT") >= 0) {
          // Now handles MEGA-style "HEARTBEAT:timestamp" format
          // Send standard HEARTBEAT_ACK response that UNO is expecting
          Serial1.println("HEARTBEAT_ACK");
          Serial1.flush(); // Ensure it's sent immediately
          
          // Also send ESP32:READY to confirm connection status
          Serial1.println("ESP32:READY");
          Serial1.flush();
          
          // Add an explicit ACK for the specific heartbeat format
          if (data.indexOf(":") > 0) {
            String timestamp = data.substring(data.indexOf(":") + 1);
            Serial1.println("ACK:HEARTBEAT:" + timestamp);
            Serial1.flush();
          }
          
          if (shouldPrintDebug) {
            Serial.println("Heartbeat acknowledged");
          }
        }
        else if (data.startsWith("ACK:")) {
          int colonPos = data.indexOf(':');
          if (colonPos > 0 && colonPos < data.length() - 1) {
            // Handle tag write acknowledgments
            if (data.indexOf("TAG_WRITTEN") > 0) {
              Serial.println("Tag was successfully written");
              
              // Update any pending commands to completed
              updatePendingWriteCommands("completed");
              
              // Visual confirmation
              setLED(COLOR_SUCCESS, 3, 200);
            }
          }
        }
        else if (data.startsWith("NAK:")) {
          if (data.indexOf("TAG_WRITE_FAILED") > 0 || data.indexOf("TAG_AUTH_FAILED") > 0) {
            Serial.println("Tag write operation failed");
            
            // Update pending commands to failed
            updatePendingWriteCommands("failed");
            
            // Visual indication of failure
            setLED(COLOR_ERROR, 3, 200);
          }
        }
        // Process tag data
        else if (data.startsWith("TAG:")) {
          // Format: TAG:{sequence},{checksum},EPC,ASCII,RSSI
          int firstComma = data.indexOf(',', 4);
          int secondComma = data.indexOf(',', firstComma + 1);
          int thirdComma = data.indexOf(',', secondComma + 1);
          int fourthComma = data.indexOf(',', thirdComma + 1);
          
          if (firstComma > 0 && secondComma > 0 && thirdComma > 0 && fourthComma > 0) {
            // Extract tag data
            int sequence = data.substring(4, firstComma).toInt();
            String epcHex = data.substring(secondComma + 1, thirdComma);
            String epcAscii = data.substring(thirdComma + 1, fourthComma);
            int rssi = data.substring(fourthComma + 1).toInt();
            
            // Always ACK immediately to prevent UNO from getting stuck
            Serial1.print("ACK:");
            Serial1.println(sequence);
            Serial1.flush();
            
            // Process the tag data
            if (epcHex.length() > 0 && epcAscii.length() > 0) {
              setLED(COLOR_RECEIVE, 1, 200);
              Serial.println("Processing tag: " + epcHex);
              
              if (uploadTagToFirebase(epcHex, epcAscii, rssi)) {
                setLED(COLOR_SUCCESS, 1, 200);
              } else {
                setLED(COLOR_ERROR, 1, 200);
              }
            }
          }
        }
      }
      
      // Reset buffer
      bufferIndex = 0;
      memset(buffer, 0, sizeof(buffer));
    }
  }
}

void setLED(uint32_t color, int blinkCount, int blinkDuration) {
  if (blinkCount <= 0) {
    // Solid color
    pixel.setPixelColor(0, color);
    pixel.show();
  } else {
    // Blink the specified number of times
    for (int i = 0; i < blinkCount; i++) {
      pixel.setPixelColor(0, color);
      pixel.show();
      delay(blinkDuration);
      pixel.setPixelColor(0, pixel.Color(0, 0, 0)); // Off
      pixel.show();
      delay(blinkDuration);
    }
    pixel.setPixelColor(0, color);
    pixel.show();
  }
}