#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>



// Function declarations
void checkUNOResponse();

// Firebase paths - updated for optimized database structure
#define COMMANDS_PATH "/commands/write_rfid"
#define SYSTEM_STATUS_PATH "/system/current"
#define PEOPLE_PATH "/people"
#define TAGS_PATH "/tags"
#define TAG_READINGS_PATH "/tag_readings"
#define LOCATIONS_PATH "/locations"

// Global variables
bool espReady = false;
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

// Additional firebase paths
#define EVENTS_PATH "/tag_readings"

// For backward compatibility with energy dashboard
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
#define COLOR_RECEIVE pixel.Color(50, 0, 50)    // Purple: Received data from Arduino
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

// Custom debug function that sends to both Serial and UNO
void debugLog(const String& msg, bool addNewline = true) {
  // Send to debug Serial
  if (addNewline) {
    Serial.println(msg);
  } else {
    Serial.print(msg);
  }
  
  // Send to UNO with special prefix
  String unoMsg = "DEBUG:" + msg;
  Serial1.println(unoMsg);
  Serial1.flush(); // Make sure message is sent
}

void setup() {
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize system start time
  systemStartTime = millis();
  
  // Initialize serial for debugging
  Serial.begin(115200);
  
  // Initialize serial for communication with Arduino (UNO/MEGA)
  Serial1.begin(9600, SERIAL_8N1, 20, 21);  // RX=GPIO20, TX=GPIO21 for ESP32-C3 UART0
  debugLog("Serial1 initialized for Arduino communication: 9600 baud on pins RX=20, TX=21");
  
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
    debugLog("WARNING: Could not initialize time via NTP");
  }
  
  // Initialize Firebase
  initFirebase();
  
  // Show successful initialization with green blink
  setLED(COLOR_SUCCESS, 3, 200);
  delay(1000);
  setLED(COLOR_IDLE);
  
  // Update system status
  updateSystemStatus();
  
  // Send ready message to Arduino
  Serial1.println(F("ESP:READY"));
  Serial1.flush(); // Make sure message is sent
  debugLog(F("Sent ESP:READY to Arduino"));
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
  
  // Process any queued tags and heartbeats from UNO
  checkUNOResponse();

  // Check for heartbeat timeout (30 seconds)
  if (millis() - lastHeartbeatReceived > 30000) {
    // No heartbeat received for 30 seconds
    setLED(COLOR_WARNING, 2, 200); // Flash yellow to indicate communication issue
    debugLog(F("WARNING: No heartbeat from UNO for >30s"));
  }
  
  // Brief delay to prevent overwhelming the system
  delay(10);
}

void processMessage(const char* message) {
  // Skip empty messages
  if (!message || strlen(message) == 0) return;
  
  // Parse the message
  if (strncmp(message, "TAG:", 4) == 0) {
    // This is from UNO - handle tag data
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
  if (epcHex.isEmpty() || rssi <= 0) {
    Serial.println("Invalid tag data");
    return false;
  }

  if (!firebaseInitialized || WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot upload: Firebase not initialized or WiFi disconnected");
    return false;
  }
  
  // Create timestamp and date values
  String timestamp = getFormattedTime();
  time_t now;
  time(&now);
  unsigned long unixTime = (unsigned long)now;
  
  // Format: YYYY-MM-DD
  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", localtime(&now));
  String dateKey = String(dateStr);
  
  // Prepare JSON payload for tag data
  FirebaseJson tagJson;
  tagJson.set("epc", epcHex);
  tagJson.set("ascii_text", epcAscii);
  tagJson.set("owner_id", ""); // Will be linked if found
  tagJson.set("last_read/timestamp", (int)unixTime);
  tagJson.set("last_read/location", currentLocation);
  tagJson.set("last_read/rssi", rssi);

  // Upload tag data to Firebase
  String tagPath = TAGS_PATH;
  tagPath += "/" + epcHex;
  bool tagSuccess = Firebase.RTDB.setJSON(&fbdo, tagPath.c_str(), &tagJson);

  if (tagSuccess) {
    Serial.print("[DB] Tag data saved to ");
    Serial.println(tagPath);
    successfulUploads++;
  } 
  else {
    Serial.print("[ERROR] Failed to save tag: ");
    Serial.println(fbdo.errorReason());
    failedUploads++;
    return false;
  }

  // Create a tag reading entry
  FirebaseJson readingJson;
  String locationId = currentLocation;
  locationId.toLowerCase(); // This modifies the string in place in Arduino
  locationId.replace(" ", "-"); // Convert "Living Room" to "living-room"
  
  String readingId = String(unixTime) + "_" + epcHex.substring(0, 8);
  String readingPath = TAG_READINGS_PATH;
  readingPath += "/" + locationId;
  readingPath += "/" + dateKey;
  readingPath += "/" + readingId;
  
  readingJson.set("tag_id", epcHex);
  readingJson.set("timestamp", unixTime);
  readingJson.set("rssi", rssi);
  
  bool readingSuccess = Firebase.RTDB.setJSON(&fbdo, readingPath.c_str(), &readingJson);
  
  if (readingSuccess) {
    Serial.print("[DB] Tag reading recorded at ");
    Serial.println(readingPath);
  } 
  else {
    Serial.print("[ERROR] Failed to record tag reading: ");
    Serial.println(fbdo.errorReason());
  }
  
  // Try to find and update person location if this tag is associated with someone
  // First try to find by ASCII text which might contain user ID
  if (epcAscii.length() > 0) {
    String potentialUserId = epcAscii;
    potentialUserId.trim();
    
    if (potentialUserId.length() >= 6) { // Reasonable minimum length for an ID
      // Look for person with matching user_id
      updateUserLocation(potentialUserId, currentLocation);
    }
  }
  
  return true;
}

void updateUserLocation(const String& userId, const String& locationName) {
  if (userId.isEmpty() || locationName.isEmpty()) {
    Serial.println("Invalid user ID or location name");
    return;
  }

  // Normalize location name
  String locationId = locationName;
  locationId.toLowerCase();
  locationId.replace(" ", "-");

  // Current timestamp
  unsigned long now = time(nullptr);
  
  // First update the person's location in the people collection
  String query = String(PEOPLE_PATH) + "?orderBy=\"user_id\"&equalTo=\"" + userId + "\"";
  bool personFound = false;
  String personId = "";

  if (Firebase.RTDB.getJSON(&fbdo, query.c_str())) {
    FirebaseJson* json = fbdo.jsonObjectPtr();
    size_t jsonSize = 0;
    if (json != nullptr) {
      jsonSize = json->iteratorBegin();
    }
    if (json != nullptr && jsonSize > 0) {
      size_t len = json->iteratorBegin();
      for (size_t i = 0; i < len && !personFound; i++) {
        FirebaseJson::IteratorValue value = json->valueAt(i);
        personId = value.key.c_str();
        personFound = true;
      }
      json->iteratorEnd();
    }
  }

  if (personFound) {
    // Update person's current location
    String personPath = String(PEOPLE_PATH) + "/" + personId + "/locations";
    FirebaseJson locationJson;
    locationJson.set("current", locationId);
    
    // Add entry to location history
    String historyPath = personPath + "/history/" + String(now);
    FirebaseJson historyJson;
    historyJson.set("location", locationId);
    historyJson.set("timestamp", now);
    
    Firebase.RTDB.updateNode(&fbdo, personPath.c_str(), &locationJson);
    Firebase.RTDB.setJSON(&fbdo, historyPath.c_str(), &historyJson);
    
    Serial.print("[DB] Updated location for person in PEOPLE collection: ");
    Serial.println(personId);
    
    // Also add person to current location's occupants list
    String occupantsPath = String(LOCATIONS_PATH) + "/" + locationId + "/occupants/" + personId;
    Firebase.RTDB.setBool(&fbdo, occupantsPath.c_str(), true);
  } else {
    Serial.println("[INFO] Person not found in PEOPLE collection with ID: " + userId);
  }
  
  // For backward compatibility, also update energy dashboard visitor information
  String dashboardLocationId = locationId;
  dashboardLocationId.replace("-", "_"); // Convert format for dashboard
  
  String userVisitorPath = VISITORS_ROOMS_PATH "/" + dashboardLocationId + "/users/" + userId;

  // Update energy dashboard visitors information
  FirebaseJson userJson;
  userJson.set("present", true);
  userJson.set("last_seen", now);
  
  if (Firebase.RTDB.updateNode(&fbdo, userVisitorPath.c_str(), &userJson)) {
    Serial.print("[DB] Updated legacy dashboard visitor location at: ");
    Serial.println(userVisitorPath);
    
    // Now update total visitor count
    String visitorsPath = VISITORS_ROOMS_PATH "/" + dashboardLocationId + "/users";
    if (Firebase.RTDB.getJSON(&fbdo, visitorsPath)) {
      FirebaseJson* json = fbdo.jsonObjectPtr();
      if (json != nullptr) {
        size_t len = json->iteratorBegin();
        int userCount = 0;
        unsigned long cutoffTime = now - USER_TIMEOUT_SECONDS;
        
        FirebaseJson updateJson;
        
        // Count users who have been seen recently
        for (size_t i = 0; i < len; i++) {
          FirebaseJson::IteratorValue value = json->valueAt(i);
          String userKey = value.key.c_str();
          
          // Check if this is a valid user entry
          if (userKey.length() > 0 && value.type == FirebaseJson::JSON_OBJECT) {
            // Get the last_seen value for this user
            FirebaseJsonData lastSeen;
            json->get(lastSeen, value.key + "/last_seen");
            
            if (lastSeen.success && lastSeen.typeNum == FirebaseJson::JSON_INT) {
              unsigned long userLastSeen = lastSeen.intValue;
              
              // If user was seen recently, count them
              if (userLastSeen > cutoffTime) {
                userCount++;
              }
              // If user hasn't been seen recently, mark them as not present
              else {
                updateJson.set(value.key + "/present", false);
              }
            }
          }
        }
        
        json->iteratorEnd();
        
        // Update total visitor count
        String totalVisitorsPath = "/energy_dashboard/visitors/total";
        Firebase.RTDB.setInt(&fbdo, totalVisitorsPath.c_str(), userCount);
      }
    }
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

void checkUNOResponse() {
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
        Serial.print("Received ACK from UNO: ");
        Serial.println(ackId);
      }
    } else if (response.startsWith("HEARTBEAT")) {
      // Got heartbeat from UNO, send acknowledgment in the format UNO expects
      Serial1.println(F("HEARTBEAT_ACK"));
      Serial1.flush();
      lastHeartbeatReceived = millis();
      Serial.println(F("Received heartbeat from UNO, sent HEARTBEAT_ACK"));
      
      // Also set ESP ready status
      espReady = true;
    }
  }
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
        
        Serial.print("[DB] Updated command: ");
        Serial.print(key);
        Serial.print(" → ");
        Serial.println(newStatus);
      }
    }
    json->iteratorEnd();
  }
}

void checkNewCommands() {
  if (!firebaseInitialized) {
    debugLog("Firebase not initialized, can't check commands");
    return;
  }

  debugLog("Checking for new commands at path: " + String(COMMANDS_PATH));
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
      
      // Parse the command object
      FirebaseJson commandObj;
      commandObj.setJsonData(value);
      
      // Get command status
      FirebaseJsonData statusData;
      commandObj.get(statusData, "status");
      
      // Only process pending commands
      if (!statusData.success || statusData.stringValue != "pending") continue;
      
      // Get user ID from params
      FirebaseJsonData paramsData;
      commandObj.get(paramsData, "params");
      
      if (!paramsData.success) continue;
      
      FirebaseJson params;
      params.setJsonData(paramsData.stringValue);
      
      FirebaseJsonData userIdData;
      params.get(userIdData, "user_id");
      
      if (!userIdData.success || userIdData.stringValue.isEmpty()) {
        Serial.println("Missing or invalid user_id in command params");
        continue;
      }
      
      // Format command for UNO
      String command = "WRITE:" + userIdData.stringValue;
      
      // Send to UNO
      Serial1.println(command);
      Serial1.flush();
      
      // Wait a bit and send again for reliability
      delay(100);
      Serial1.println(command);
      Serial1.flush();
      
      debugLog("[WRITE] Sent to UNO: " + String(command));
      
      // Update command status
      updateCommandStatus(key.c_str(), "processing");
      
      // Update timestamp
      String timestampPath = String(COMMANDS_PATH) + "/" + key + "/updated_at";
      Firebase.RTDB.setInt(&fbdo, timestampPath.c_str(), (int)time(NULL));
      
      // Only process one command at a time
      break;
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
    debugLog("Firebase authentication successful");
    firebaseInitialized = true;
    setLED(COLOR_SUCCESS, 1, 500); // Green flash for Firebase init
  } else {
    debugLog("Firebase authentication failed");
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
          // Send standard HEARTBEAT_ACK response
          Serial1.println("HEARTBEAT_ACK");
          Serial1.flush(); // Ensure it's sent immediately
          lastHeartbeatReceived = millis();
          espReady = true;
          
          Serial.println("Received heartbeat from Arduino, sent HEARTBEAT_ACK");
        }
        else if (data.startsWith("ACK:")) {
          Serial.print("Received ACK from Arduino: ");
          Serial.println(data);
          
          int colonPos = data.indexOf(':');
          if (colonPos > 0 && colonPos < data.length() - 1) {
            // Handle tag write acknowledgments
            if (data.indexOf("TAG_WRITTEN") > 0) {
              Serial.println("[SUCCESS] RFID Tag was successfully written");
              
              // Update any pending commands to completed
              updatePendingWriteCommands("completed");
              
              // Visual confirmation
              setLED(COLOR_SUCCESS, 3, 200);
            }
          }
        }
        else if (data.startsWith("NAK:")) {
          Serial.print("Received error response from Arduino: ");
          Serial.println(data);
          
          if (data.indexOf("TAG_WRITE_FAILED") > 0 || data.indexOf("TAG_AUTH_FAILED") > 0) {
            Serial.println("[ERROR] RFID Tag write operation failed");
            
            // Update pending commands to failed
            updatePendingWriteCommands("failed");
            
            // Visual indication of failure
            setLED(COLOR_ERROR, 3, 200);
          }
        }
        // Process tag data
        else if (data.startsWith("TAG:")) {
          // Format: TAG:{sequence},{checksum},EPC,ASCII,RSSI
          Serial.println("Received TAG data from Arduino");
          
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
            
            Serial.print("Tag Data: Seq=");
            Serial.print(sequence);
            Serial.print(" EPC=");
            Serial.print(epcHex);
            Serial.print(" RSSI=");
            Serial.println(rssi);
            
            // Always ACK immediately to prevent UNO/Mega from getting stuck
            Serial1.print("ACK:");
            Serial1.println(sequence);
            Serial1.flush();
            Serial.println("Sent acknowledgment: ACK:" + String(sequence));
            
            // Process the tag data
            if (epcHex.length() > 0 && epcAscii.length() > 0) {
              setLED(COLOR_RECEIVE, 1, 200);
              Serial.println("[INFO] Processing tag: " + epcHex + " (" + epcAscii + ")");
              
              if (uploadTagToFirebase(epcHex, epcAscii, rssi)) {
                setLED(COLOR_SUCCESS, 1, 200);
                Serial.println("[SUCCESS] Tag data processing complete");
              } else {
                setLED(COLOR_ERROR, 1, 200);
                Serial.println("[ERROR] Failed to process tag data");
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