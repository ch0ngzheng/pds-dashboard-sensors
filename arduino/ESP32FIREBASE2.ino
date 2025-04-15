#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>
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
#define USERS_PATH "/users"
#define LOCATIONS_PATH "/locations"
#define EVENTS_PATH "/events"
#define TAGS_PATH "/tags"
#define SYSTEM_STATUS_PATH "/system_status"

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
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;      // GMT offset in seconds (adjust for your timezone)
const int daylightOffset_sec = 0;  // Daylight savings time offset (adjust if needed)

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
bool initializeTime();
void logBootInfo();

void setup() {
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize system start time
  systemStartTime = millis();
  
  // Initialize serial for debugging
  Serial.begin(115200);
  
  // Initialize serial for communication with Arduino Mega
  Serial1.begin(9600, SERIAL_8N1, 20, 21);  // RX=GPIO20, TX=GPIO21
  
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
  if (initializeTime()) {
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
  
  // Send ready message to Mega
  Serial1.println("ESP32:READY");
}



void loop() {
  // Check WiFi connection every 30 seconds
  if (millis() - lastWiFiCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      setLED(COLOR_WIFI);
      connectToWiFi();
    }
    lastWiFiCheck = millis();
  }
  
  // Process data from Arduino Mega
  processSerialData();
  
  // Retry Firebase initialization if needed
  if (!firebaseInitialized && millis() - lastFirebaseRetry > 60000) {
    Serial.println("Retrying Firebase initialization...");
    initFirebase();
    lastFirebaseRetry = millis();
  }
  
  // Heartbeat indicator (short blue pulse every 5 seconds)
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    setLED(COLOR_IDLE, 1, 100);
    lastHeartbeat = millis();
    
    // Send heartbeat to Mega
    Serial1.println("HEARTBEAT_ACK");
  }
  
  // Update system status every 5 minutes
  static unsigned long lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate > 300000) {
    updateSystemStatus();
    lastStatusUpdate = millis();
  }
}

void logBootInfo() {
  // Read and increment boot count
  uint32_t bootCount = EEPROM.readUInt(EEPROM_BOOT_COUNT_ADDR);
  bootCount++;
  EEPROM.writeUInt(EEPROM_BOOT_COUNT_ADDR, bootCount);
  
  // Store last reset time
  uint32_t lastResetTime = EEPROM.readUInt(EEPROM_LAST_RESET_ADDR);
  EEPROM.writeUInt(EEPROM_LAST_RESET_ADDR, millis());
  
  // Commit changes
  EEPROM.commit();
  
  Serial.println("==== SYSTEM BOOT ====");
  Serial.print("Boot count: ");
  Serial.println(bootCount);
  Serial.print("Last reset: ");
  if (lastResetTime > 0) {
    Serial.print(lastResetTime / 1000);
    Serial.println(" seconds after previous boot");
  } else {
    Serial.println("First boot or EEPROM reset");
  }
  Serial.println("=====================");
}

bool initializeTime() {
  // Configure time via NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Wait up to 10 seconds for time to be set
  int retry = 0;
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo) && retry < 10) {
    Serial.println("Waiting for NTP time sync...");
    delay(1000);
    retry++;
  }
  
  return (retry < 10);
}

void connectToWiFi() {
  setLED(COLOR_WIFI);
  
  // Determine which WiFi to use
  const char* ssid = useAlternateWiFi ? ALT_WIFI_SSID : WIFI_SSID;
  const char* password = useAlternateWiFi ? ALT_WIFI_PASSWORD : WIFI_PASSWORD;
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);
  
  // Wait for connection with timeout
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    
    // Blink cyan while connecting
    if (attempts % 2 == 0) {
      setLED(COLOR_WIFI);
    } else {
      setLED(COLOR_IDLE);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Show success with green flash
    setLED(COLOR_SUCCESS, 3, 100);
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Reset connection attempts counter
    connectionAttempts = 0;
  } else {
    // Show error with red flash
    setLED(COLOR_ERROR, 3, 100);
    Serial.println("WiFi connection failed");
    
    // Increment attempts and try alternate WiFi if needed
    connectionAttempts++;
    if (connectionAttempts >= 3) {
      useAlternateWiFi = !useAlternateWiFi;
      connectionAttempts = 0;
      Serial.println("Switching to alternate WiFi settings");
    }
  }
  
  // Return to idle color
  setLED(COLOR_IDLE);
}

void initFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot initialize Firebase: WiFi not connected");
    firebaseInitialized = false;
    return;
  }
  
  // Set the database URL
  config.database_url = DATABASE_URL;
  
  // Use database secret authentication
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  
  // Enable auto-reconnect to Firebase
  config.timeout.serverResponse = 10000;
  
  // Initialize the library with the Firebase config
  Firebase.begin(&config, &auth);
  
  // Optional: Set database read timeout
  Firebase.setDoubleDigits(5);
  
  // Check if initialization was successful
  if (Firebase.ready()) {
    firebaseInitialized = true;
    Serial.println("Firebase initialized successfully");
    
    // Initialize system status node
    FirebaseJson json;
    json.set("status", "online");
    json.set("start_time", getFormattedTime());
    json.set("boot_count", EEPROM.readUInt(EEPROM_BOOT_COUNT_ADDR));
    
    Firebase.RTDB.setJSON(&fbdo, SYSTEM_STATUS_PATH, &json);
  } else {
    firebaseInitialized = false;
    Serial.println("Firebase initialization failed");
  }
}

void processSerialData() {
  // Check if we have data from Arduino Mega
  while (Serial1.available() > 0) {
    char c = Serial1.read();
    
    // Check for end of message (newline)
    if (c == '\n' || c == '\r') {
      if (bufferIndex > 0) {
        // Null-terminate the buffer
        serialBuffer[bufferIndex] = '\0';
        
        // Process the complete message
        processMessage(serialBuffer);
        
        // Reset buffer index
        bufferIndex = 0;
      }
    } else if (bufferIndex < BUFFER_SIZE - 1) {
      // Add character to buffer
      serialBuffer[bufferIndex++] = c;
    }
  }
}

void processMessage(const char* message) {
  // Briefly show purple when receiving message
  setLED(COLOR_RECEIVE);
  
  // Check for heartbeat
  if (strncmp(message, "HEARTBEAT:", 10) == 0) {
    Serial1.println("HEARTBEAT_ACK");
    setLED(COLOR_IDLE);
    return;
  }
  
  // Check for tag data format: TAG:{sequence},{checksum},EPC,ASCII,RSSI
  if (strncmp(message, "TAG:", 4) == 0) {
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
      
      // Process the tag data
      totalTagsProcessed++;
      
      // Upload to Firebase
      bool success = uploadTagToFirebase(epcHex, epcAscii, rssi);
      
      if (success) {
        successfulUploads++;
      } else {
        failedUploads++;
      }
      
      // Send acknowledgment back to Arduino Mega
      sendResponse(success, sequence);
      
      // Show success or error
      if (success) {
        setLED(COLOR_SUCCESS, 1, 500);
      } else {
        setLED(COLOR_ERROR, 1, 500);
      }
    } else {
      // Malformed data
      Serial.println("Malformed tag data format");
      setLED(COLOR_ERROR, 3, 100);
      sendResponse(false, sequence);
    }
  } else if (strncmp(message, "TEST:", 5) == 0) {
    // Handle test messages
    Serial1.println("ESP32:ACK");
    setLED(COLOR_SUCCESS, 2, 100);
  }
  
  // Return to idle state
  setLED(COLOR_IDLE);
}

bool uploadTagToFirebase(const String& epcHex, const String& epcAscii, int rssi) {
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
  tagJson.set("last_seen", (int)now);
  
  // Use room name for new structure
  String locationId = ROOM_NAME;
  
  // Path for this specific tag
  String tagPath = String(TAGS_PATH) + "/" + epcHex;
  
  // Additional operations success tracker
  bool additionalSuccess = true;
  
  // Upload tag data to Firebase
  bool tagSuccess = Firebase.RTDB.setJSON(&fbdo, tagPath.c_str(), &tagJson);
  
  if (!tagSuccess) {
    Serial.print("Failed to update tag: ");
    Serial.println(fbdo.errorReason().c_str());
  }
  
  // Update user location (if we have a user ID)
  if (userId.length() > 0) {
    updateUserLocation(userId, ROOM_NAME);
  }
  
  // Log event
  logEvent("tag_detected", epcHex, locationId);
  
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