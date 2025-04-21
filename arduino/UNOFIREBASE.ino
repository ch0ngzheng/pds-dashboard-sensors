#include <Arduino.h>
#include <SoftwareSerial.h>
#include <AltSoftSerial.h>
// Simplified tag system for Arduino Uno with memory constraints
#define MAX_QUEUE_SIZE 3  // Further reduced queue size
#define TAG_DEBOUNCE_TIME 2000
#define BUFFER_SIZE 64    // Reduced buffer size

// Declare SoftwareSerial instances globally so they're accessible in all functions
SoftwareSerial rfidSerial(2, 3); // RX=2, TX=3 for RFID module
AltSoftSerial espSerial;  // RX=8, TX=9 for ESP module (AltSoftSerial uses fixed pins)

// RFID Card structure - simplified
struct CARD {
    uint8_t epc[12];
};

// Global variables - significantly reduced
CARD currentCard;  // Just keep one card instead of an array
uint8_t buffer[BUFFER_SIZE] = {0};  // Reduced buffer size
bool isMultiplePolling = false;
char serialInput[30];  // Reduced input buffer
int serialInputIndex = 0;

// ESP communication variables - COMPLETELY REDESIGNED
bool waitingForESPAck = false;
unsigned long lastESPSendTime = 0;

// Global ESP status - START WITH TRUE. Assume connection until proven otherwise
bool espReady = true;

// Sequence counter for messages
uint8_t espTxSequence = 0;  // Changed from uint16_t to uint8_t

// Communication monitoring
unsigned long lastHeartbeatSent = 0;
unsigned long lastHeartbeatReceived = 0;
unsigned long lastAnyESPActivity = 0;   // Tracks ANY ESP communication
uint8_t consecutiveCommErrors = 0;
bool forceNextHeartbeat = true;          // Force a heartbeat on startup

// Timing variables for rate limiting
unsigned long lastDebugTime = 0;
const unsigned long DEBUG_INTERVAL = 5000;  // 5 seconds between debug prints
unsigned long lastTagCheckTime = 0;
const unsigned long TAG_CHECK_INTERVAL = 100;  // 100ms between tag checks

// Simplified tag queue structure
struct TagQueueItem {
  uint8_t epc[12];
  uint8_t rssi;
  unsigned long timestamp;
  bool valid;
};

TagQueueItem tagQueue[MAX_QUEUE_SIZE];
uint8_t queueHead = 0;
uint8_t queueTail = 0;
bool queueFull = false;

// Function declarations - reduced to essentials
void displayMenu();
void writeAsciiToTag(String tagData);
bool waitMsg(unsigned long timeout);
void cleanBuffer();
void buildCommand(uint8_t* cmd, size_t length);
bool sendAndWaitResponse(uint8_t* cmd, size_t length, unsigned long timeout = 1000);
bool readSingleTag();
void addTagToQueue(uint8_t* epcData, uint8_t rssi);
bool getNextTagFromQueue(uint8_t* epcData, uint8_t* rssi);
void sendTagToESP(uint8_t* epcData, uint8_t length, uint8_t rssi);
void checkESPResponse();
void sendHeartbeatToESP();
void testESPCommunication();
bool checkAccessPassword(uint32_t password);
bool setAccessPassword(uint32_t currentPassword, uint32_t newPassword);
bool writeEpcData(uint8_t* data, uint8_t length, uint32_t password);
void handleAsciiWrite();
void inputEpcData(uint8_t* data, uint8_t maxLength);

// Convert hex array to ASCII string - simplified to avoid String
void hexToAscii(const uint8_t* hexArray, size_t length, char* result, size_t resultSize) {
  size_t j = 0;
  
  for (size_t i = 0; i < length && j < resultSize - 1; i++) {
    // Only add printable ASCII characters (32-126)
    if (hexArray[i] >= 32 && hexArray[i] <= 126) {
      result[j++] = (char)hexArray[i];
    } else if (hexArray[i] != 0) {
      // For non-printable characters, add a placeholder
      result[j++] = '.';
    }
  }
  
  // Null terminate
  result[j] = '\0';
}

// Convert ASCII string to hex array - no change
void asciiToHex(const char* asciiStr, uint8_t* hexArray, size_t maxLength) {
  size_t asciiLen = strlen(asciiStr);
  size_t bytesToConvert = min(asciiLen, maxLength);
  
  for (size_t i = 0; i < bytesToConvert; i++) {
    hexArray[i] = (uint8_t)asciiStr[i];
  }
  
  // If ASCII string is shorter than maxLength, pad with zeros
  if (asciiLen < maxLength) {
    for (size_t i = asciiLen; i < maxLength; i++) {
      hexArray[i] = 0x00;
    }
  }
}

// Send tag data to ESP with sequence number - REDESIGNED
void sendTagToESP(uint8_t* epcData, uint8_t length, uint8_t rssi) {
  static unsigned long lastDebugTime = 0;
  unsigned long currentTime = millis();
  bool shouldPrintDebug = (currentTime - lastDebugTime) >= DEBUG_INTERVAL;
  
  // We no longer check for ESP readiness - we just attempt to send
  // This eliminates the false negative problem
  
  // Check if we're already waiting for acknowledgment
  if (waitingForESPAck) {
    // Check for timeout on previous transmission
    if (currentTime - lastESPSendTime > 3000) {
      // Auto-resolve after timeout to prevent lockups
      if (shouldPrintDebug) {
        Serial.println(F("ESP ack timeout - resending"));
        lastDebugTime = currentTime;
      }
      waitingForESPAck = false;
      // Don't increment consecutive errors - just retry
    } else {
      // Still waiting but don't flood debug messages
      if (shouldPrintDebug) {
        Serial.println(F("Waiting for ESP to acknowledge previous tag"));
        lastDebugTime = currentTime;
      }
      // Don't return - just proceed with sending
    }
  }
  
  // Use fixed-size buffers instead of String objects
  char hexBuf[25] = {0}; // Enough for 12 bytes as hex
  char asciiBuf[13] = {0}; // 12 chars + null terminator
  char message[60] = {0}; // Final message buffer
  
  // Convert EPC to hex and ASCII
  for (int i = 0; i < length && i*2 < sizeof(hexBuf)-1; i++) {
    sprintf(&hexBuf[i*2], "%02X", epcData[i]);
  }
  hexToAscii(epcData, length, asciiBuf, sizeof(asciiBuf));
  
  // Improved checksum calculation
  uint16_t checksum = 0xFFFF; // Start with all bits set
  checksum ^= espTxSequence;
  checksum ^= rssi;
  for(uint8_t i = 0; i < length; i++) {
    checksum ^= epcData[i];
    checksum = (checksum << 1) | (checksum >> 15); // Rotate left by 1
  }
  
  // Build the message using snprintf for safety
  snprintf(message, sizeof(message), "TAG:%d,%d,%s,%s,%02X\n", 
           espTxSequence, checksum, hexBuf, asciiBuf, rssi);
  
  // Send to ESP with explicit flush
  if (shouldPrintDebug) {
    Serial.print(F("Sending to ESP: "));
    Serial.print(message);
    lastDebugTime = currentTime;
  }
  
  // Always track ESP activity when sending data
  lastAnyESPActivity = currentTime;
  
  // Clear any pending data in espSerial buffer
  while (espSerial.available()) {
    espSerial.read();
  }
  
  // Send the message
  espSerial.print(message);
  espSerial.flush(); // Wait for transmission to complete
  
  // Mark that we're waiting for response and increment sequence
  waitingForESPAck = true;
  lastESPSendTime = currentTime;
  espTxSequence = (espTxSequence + 1) % 100; // Wrap at 100
  
  // Turn on LED until we get acknowledgment
  digitalWrite(13, HIGH);
}

// Send a heartbeat to ESP - matches MEGA implementation
void sendHeartbeatToESP() {
    String heartbeat = "HEARTBEAT:" + String(millis());
    espSerial.println(heartbeat);
    lastHeartbeatSent = millis();
}

// Modify this function in your Arduino Uno code to recognize "ESP32:READY" instead of "ESP:READY"

// Enhanced ESP response checker
void checkESPResponse() {
    if (espSerial.available()) {
        String response = espSerial.readStringUntil('\n');
        response.trim();
        
        Serial.print("ESP response: ");
        Serial.println(response);
        
        // Reset consecutive error counter on any response
        consecutiveCommErrors = 0;
        
        // Check for ESP ready message
        if (response == "ESP:READY") {
            Serial.println("ESP is ready to receive RFID data!");
            espReady = true;
            lastHeartbeatReceived = millis();
            
            // Blink LED to show ESP is ready
            for (int i = 0; i < 3; i++) {
                digitalWrite(13, HIGH);
                delay(200);
                digitalWrite(13, LOW);
                delay(200);
            }
        }
        // Check for acknowledgment with sequence number
        else if (response.startsWith("ACK:")) {
            // Extract sequence number
            int colonPos = response.indexOf(':');
            if (colonPos > 0 && colonPos < response.length() - 1) {
                waitingForESPAck = false;
                digitalWrite(13, LOW); // Turn off LED after acknowledgment
            }
        }
        // Check for heartbeat response
        else if (response.startsWith("HEARTBEAT_ACK")) {
            lastHeartbeatReceived = millis();
            espReady = true;
        }
        // Check for generic ACK
        else if (response == "ACK" || response.startsWith("ESP:ACK")) {
            waitingForESPAck = false;
            digitalWrite(13, LOW); // Turn off LED after acknowledgment
        }
        // Check for error/negative acknowledgment
        else if (response == "NAK" || response.startsWith("ESP:NAK")) {
            Serial.println("ESP reported error processing tag data");
            waitingForESPAck = false;
            digitalWrite(13, LOW);
            consecutiveCommErrors++;
        }
        // Handle debug messages from ESP32
        else if (response.startsWith("DEBUG:")) {
            String debugMsg = response.substring(6); // Skip "DEBUG:"
            Serial.print("[ESP32] ");
            Serial.println(debugMsg);
        }
        // Handle WRITE commands from webapp via ESP32
        else if (response.startsWith("WRITE:")) {
            String userData = response.substring(6); // Skip "WRITE:"
            Serial.print("Received write command from webapp: ");
            Serial.println(userData);
            
            // Try to write the data to a tag
            writeAsciiToTag(userData);
        }
    }
    
    // Check for heartbeat timing
    unsigned long currentTime = millis();
    
    // Send heartbeat every 10 seconds if ESP is ready
    if (espReady && currentTime - lastHeartbeatSent > 10000) {
        sendHeartbeatToESP();
        lastHeartbeatSent = currentTime;
    }
    
    // Check for heartbeat timeout (30 seconds)
    if (espReady && lastHeartbeatReceived > 0 && currentTime - lastHeartbeatReceived > 30000) {
        Serial.println("WARNING: ESP heartbeat timeout - marking as not ready");
        espReady = false;
    }
}

// Test ESP32 communication with improved debugging
void testESPCommunication() {
    Serial.println(F("\n--- ESP32 COMMUNICATION TEST ---"));
    Serial.println(F("Sending test message to ESP32..."));
    
    // Clear any pending bytes
    while (espSerial.available()) {
        espSerial.read();
        delay(1);
    }
    
    // Send test message and flush
    espSerial.println(F("TEST:PING"));
    espSerial.flush();
    
    Serial.println(F("Waiting for response (5 sec)..."));
    unsigned long startTime = millis();
    bool received = false;
    
    while (millis() - startTime < 5000) {
        if (espSerial.available()) {
            checkESPResponse(); // Use our buffered reader
            received = true;
            break;
        }
        delay(100); // Longer delay between checks
    }
    
    if (!received) {
        Serial.println(F("ERROR: No response from ESP32"));
        Serial.println(F("Troubleshooting steps:"));
        Serial.println(F("1. Check ESP32 power LED"));
        Serial.println(F("2. Verify TX/RX connections"));
        Serial.println(F("3. Reset ESP32 if needed"));
        Serial.println(F("4. Check ESP32 code upload"));
    }
    
    Serial.println(F("--- END TEST ---\n"));
}

// Add tag to queue - simplified
void addTagToQueue(uint8_t* epcData, uint8_t rssi) {
  // Simple check for duplicates
  for (uint8_t i = 0; i < MAX_QUEUE_SIZE; i++) {
    if (tagQueue[i].valid) {
      bool match = true;
      for (uint8_t j = 0; j < 12; j++) {
        if (tagQueue[i].epc[j] != epcData[j]) {
          match = false;
          break;
        }
      }
      if (match) return; // Duplicate, don't add
    }
  }
  
  // Add to queue
  if (queueFull) return; // Drop if queue is full
  
  memcpy(tagQueue[queueTail].epc, epcData, 12);
  tagQueue[queueTail].rssi = rssi;
  tagQueue[queueTail].timestamp = millis();
  tagQueue[queueTail].valid = true;
  
  // Update tail
  queueTail = (queueTail + 1) % MAX_QUEUE_SIZE;
  if (queueTail == queueHead) queueFull = true;
}

// Get next tag from queue - simplified
bool getNextTagFromQueue(uint8_t* epcData, uint8_t* rssi) {
  if (queueHead == queueTail && !queueFull) {
    return false; // Queue is empty
  }
  
  // Copy tag data
  if (tagQueue[queueHead].valid) {
    memcpy(epcData, tagQueue[queueHead].epc, 12);
    *rssi = tagQueue[queueHead].rssi;
    
    // Mark as invalid and move head
    tagQueue[queueHead].valid = false;
    queueHead = (queueHead + 1) % MAX_QUEUE_SIZE;
    queueFull = false;
    
    return true;
  }
  
  // No valid tag at head
  queueHead = (queueHead + 1) % MAX_QUEUE_SIZE;
  queueFull = false;
  return false;
}

void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    
    // Initialize software serial for ESP32 communication
    espSerial.begin(9600);   // ESP communication on pins 8,9 (AltSoftSerial)
    
    // Initialize software serial for RFID module
    pinMode(2, INPUT);  // RX from RFID module
    pinMode(3, OUTPUT); // TX to RFID module
    rfidSerial.begin(115200);
    
    // LED for status indication
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    
    Serial.println(F("\n=== RFID Tracking System - Uno Version ==="));
    Serial.println(F("Initializing..."));
    
    // Clear any pending data
    while(espSerial.available()) espSerial.read();
    while(rfidSerial.available()) rfidSerial.read();
    
    // Initialize with espReady false - will be set when ESP:READY is received
    espReady = false;
    
    // Initialize heartbeat timers
    lastHeartbeatSent = millis();
    lastHeartbeatReceived = millis();
    
    Serial.println(F("Module Ready!"));
    displayMenu();
}

void loop() {
  // Check for ESP communications first
  checkESPResponse();
  
  // Send heartbeat every 5 seconds if we're ready
  if (millis() - lastHeartbeatSent > 5000) {
    sendHeartbeatToESP();
    lastHeartbeatSent = millis();
  }
  
  // Check for heartbeat timeout (30 seconds)
  unsigned long currentTime = millis();
  if (espReady && lastHeartbeatReceived > 0 && currentTime - lastHeartbeatReceived > 30000) {
    Serial.println(F("WARNING: ESP heartbeat timeout - marking as not ready"));
    espReady = false;
  }
  
  // Process tag queue every 200ms (reduced frequency for Uno)
  static unsigned long lastQueueProcess = 0;
  if (millis() - lastQueueProcess > 200) {
    lastQueueProcess = millis();
    
    // Only process if not waiting for ESP
    if (!waitingForESPAck) {
      uint8_t tempEpc[12];
      uint8_t tempRssi;
      if (getNextTagFromQueue(tempEpc, &tempRssi)) {
        sendTagToESP(tempEpc, 12, tempRssi);
      }
    }
    
    // Check for user commands
    if (Serial.available() > 0) {
      char cmd = Serial.read();
      switch (cmd) {
        case '1': // Read single tag
          Serial.println(F("Reading single tag..."));
          readSingleTag();
          displayMenu();
          break;
        case '2': // Write ASCII to tag
        case 'w':
        case 'W':
          Serial.println(F("Write ASCII Text to Tag"));
          handleAsciiWrite();
          displayMenu();
          break;
        case '3': // Test ESP communication
        case 't':
        case 'T':
          Serial.println(F("Test ESP Communication"));
          testESPCommunication();
          displayMenu();
          break;
        case 'm':
        case 'M':
          displayMenu();
          break;
      }
    }
  }
 
  bool shouldPrintDebug = (currentTime - lastDebugTime) >= DEBUG_INTERVAL;
  if (shouldPrintDebug) {
    lastDebugTime = currentTime;
  }
  checkESPResponse();
  
  // Send heartbeat to ESP if needed
  if (currentTime - lastHeartbeatSent > 5000) {
    sendHeartbeatToESP();
  }
  
  // Don't automatically check for RFID tags - only do it on menu command
  // This matches the MEGA implementation
  
  // No need to process queued tags here as we already do that in the 200ms interval check
  
  // Reset if too many communication errors
  if (consecutiveCommErrors > 10) {
    if (shouldPrintDebug) {
      Serial.println(F("Too many communication errors, resetting..."));
    }
    delay(1000);
    asm volatile ("jmp 0");
  }
  
  // Small delay to prevent tight loop
  delay(10);
}

void displayMenu() {
    Serial.println(F("\n--- RFID Tracking System ---"));
    Serial.println(F("1. Read Single Tag"));
    Serial.println(F("2. Write ASCII to Tag"));
    Serial.println(F("3. Test ESP Communication"));
    Serial.print(F("> "));
}

// Wait for a complete message with timeout - simplified for Uno
bool waitMsg(unsigned long timeout) {
    unsigned long start = millis();
    uint8_t i = 0;
    cleanBuffer();

    // Look for starting byte 0xBB first
    while (millis() - start < timeout) {
        if (rfidSerial.available()) {
            uint8_t b = rfidSerial.read();
            if (b == 0xBB) {
                buffer[0] = b;
                i = 1;
                break;
            }
        }
    }
    
    if (i == 0) {
        Serial.println(F("Timeout waiting for start byte"));
        return false;
    }
    
    // Now read the rest of the message
    start = millis(); // Reset timeout
    while (millis() - start < timeout && i < BUFFER_SIZE) {
        if (rfidSerial.available()) {
            buffer[i] = rfidSerial.read();
            
            // Check if we have a complete message by looking for end marker
            if (buffer[i] == 0x7E) {
                // Print first few bytes for debugging
                Serial.print(F("Received: "));
                for (int j = 0; j <= min(i, 10); j++) {
                    if (buffer[j] < 0x10) Serial.print(F("0"));
                    Serial.print(buffer[j], HEX);
                    Serial.print(F(" "));
                }
                if (i > 10) Serial.print(F("..."));
                Serial.println();
                return true;
            }
            i++;
        }
    }
    
    Serial.println(F("Timeout waiting for complete message"));
    return false;
}

// Helper functions for RFID communication - optimized
void cleanBuffer() {
    memset(buffer, 0, BUFFER_SIZE);
}

// Generate command with proper checksum calculation
void buildCommand(uint8_t* cmd, size_t length) {
    uint8_t checksum = 0;
    // Calculate checksum from Type (index 1) to last Parameter byte (length-3)
    for (int i = 1; i < length - 2; i++) {
        checksum += cmd[i];
    }
    // Set checksum byte
    cmd[length - 2] = checksum;
}

// Helper function to send commands and wait for response
bool sendAndWaitResponse(uint8_t* cmd, size_t length, unsigned long timeout) {
    // Clear any pending bytes in the receive buffer
    while (rfidSerial.available()) {
        rfidSerial.read();
    }
    
    // Clean our response buffer
    cleanBuffer();
    
    // Calculate checksum before sending
    buildCommand(cmd, length);
    
    // Send the command - one byte at a time to avoid buffer overflows
    for (size_t i = 0; i < length; i++) {
        rfidSerial.write(cmd[i]);
        delay(1); // Small delay to ensure reliable transmission
    }
    
    // Wait for response
    return waitMsg(timeout);
}

// Single tag polling - simplified for Uno
bool readSingleTag() {
    uint8_t cmd[] = {0xBB, 0x00, 0x22, 0x00, 0x00, 0x00, 0x7E};
    
    if (sendAndWaitResponse(cmd, sizeof(cmd), 1500)) {
        // Check if it's a notification frame
        if (buffer[1] == 0x02 && buffer[2] == 0x22) {
            // Parse the tag data
            uint8_t rssi = buffer[5];
            
            // Print tag EPC
            Serial.print(F("Tag found! RSSI: "));
            Serial.print(rssi, HEX);
            Serial.print(F(" EPC: "));
            
            // Copy EPC to our structure
            for (int i = 8; i < 20; i++) {
                currentCard.epc[i-8] = buffer[i];
                
                // Print hex value
                if (buffer[i] < 0x10) Serial.print(F("0"));
                Serial.print(buffer[i], HEX);
                Serial.print(F(" "));
            }
            
            // Add ASCII interpretation
            char asciiStr[13]; // 12 chars + null terminator
            hexToAscii(currentCard.epc, 12, asciiStr, sizeof(asciiStr));
            Serial.print(F("\nASCII: \""));
            Serial.print(asciiStr);
            Serial.println(F("\""));
            
            // Add tag to queue to send to ESP
            addTagToQueue(currentCard.epc, rssi);
            
            return true;
        }
    }
    Serial.println(F("No tag found or communication error"));
    return false;
}

// Check if we can access the tag with the provided password - simplified
bool checkAccessPassword(uint32_t password) {
    // Command to read User memory bank
    uint8_t cmd[16] = {
        0xBB,                           // Header
        0x00,                           // Type (command)
        0x39,                           // Command: Read data
        0x00, 0x09,                     // Length: 9 bytes
        (uint8_t)(password >> 24),      // Password MSB
        (uint8_t)(password >> 16),
        (uint8_t)(password >> 8),
        (uint8_t)(password & 0xFF),     // Password LSB
        0x03,                           // Memory bank (03 = User)
        0x00, 0x00,                     // Starting address (0x0000)
        0x00, 0x02,                     // Data length (2 words = 4 bytes)
        0x00,                           // Checksum placeholder
        0x7E                            // End marker
    };
    
    // Send command and wait for response
    if (sendAndWaitResponse(cmd, sizeof(cmd), 2000)) {
        // Check for successful response (not an error)
        if (buffer[1] == 0x01 && buffer[2] == 0x39) {
            return true;
        }
    }
    
    return false;
}

// Set access password for the tag - simplified
bool setAccessPassword(uint32_t currentPassword, uint32_t newPassword) {
    // Command format for setting password
    uint8_t cmd[20] = {
        0xBB,                                // Header
        0x00,                                // Type (command)
        0x49,                                // Command: Write data
        0x00, 0x0D,                          // Length: 13 bytes
        (uint8_t)(currentPassword >> 24),    // Current password MSB
        (uint8_t)(currentPassword >> 16),
        (uint8_t)(currentPassword >> 8),
        (uint8_t)(currentPassword & 0xFF),   // Current password LSB
        0x00,                                // Memory bank (00 = Reserved)
        0x00, 0x02,                          // Starting address (0x0002)
        0x00, 0x02,                          // Data length (2 words = 4 bytes)
        (uint8_t)(newPassword >> 24),        // New password MSB
        (uint8_t)(newPassword >> 16),
        (uint8_t)(newPassword >> 8),
        (uint8_t)(newPassword & 0xFF),       // New password LSB
        0x00,                                // Checksum placeholder
        0x7E                                 // End marker
    };
    
    // Send command and wait for response
    if (sendAndWaitResponse(cmd, sizeof(cmd), 2000)) {
        // Check for successful response
        if (buffer[1] == 0x01 && buffer[2] == 0x49) {
            return true;
        }
    }
    return false;
}

// Write data to the EPC memory bank - optimized
bool writeEpcData(uint8_t* data, uint8_t length, uint32_t password) {
    // Ensure length is valid (max 12 bytes for EPC)
    if (length > 12) {
        Serial.println(F("EPC data too long, maximum is 12 bytes"));
        return false;
    }
    
    // Calculate data length in words (2 bytes per word)
    uint8_t dataLengthWords = length / 2;
    if (length % 2 != 0) {
        dataLengthWords += 1;
    }
    
    // Calculate needed command buffer size
    uint8_t cmdLength = 16 + length;
    if (length % 2 != 0) cmdLength++; // Add room for padding if needed
    
    // Create command buffer on stack instead of heap
    uint8_t cmd[30]; // Max possible size (16 base + 12 data + 2 padding/etc)
    
    // Build the command
    cmd[0] = 0xBB;               // Header
    cmd[1] = 0x00;               // Type (command)
    cmd[2] = 0x49;               // Command: Write data
    cmd[3] = 0x00;               // Length MSB
    cmd[4] = 9 + length;         // Length LSB: 9 fixed bytes + data length
    
    // Use the specified password 
    cmd[5] = (uint8_t)(password >> 24);
    cmd[6] = (uint8_t)(password >> 16);
    cmd[7] = (uint8_t)(password >> 8);
    cmd[8] = (uint8_t)(password & 0xFF);
    
    cmd[9] = 0x01;               // Memory bank (01 = EPC)
    cmd[10] = 0x00;              // Starting address MSB
    cmd[11] = 0x02;              // Starting address LSB
    cmd[12] = 0x00;              // Data length MSB
    cmd[13] = dataLengthWords;   // Data length LSB (in words)
    
    // Copy the data
    for (int i = 0; i < length; i++) {
        cmd[14 + i] = data[i];
    }
    
    // If we have an odd number of bytes, add a padding byte of 0
    if (length % 2 != 0) {
        cmd[14 + length] = 0x00;  // Add padding
        length++;  // Adjust length for the padding
    }
    
    // Checksum and end marker
    cmd[14 + length] = 0x00;     // Checksum placeholder (will be calculated by sendAndWaitResponse)
    cmd[15 + length] = 0x7E;     // End marker
    
    // Send command and wait for response
    bool result = sendAndWaitResponse(cmd, 16 + length, 3000);
    
    if (result) {
        // Check for successful response
        if (buffer[1] == 0x01 && buffer[2] == 0x49) {
            Serial.println(F("EPC data written successfully!"));
            return true;
        }
    }
    Serial.println(F("Failed to write EPC data"));
    return false;
}

// Helper function to input EPC data from Serial - simplified
void inputEpcData(uint8_t* data, uint8_t maxLength) {
    memset(data, 0, maxLength); // Clear the buffer
    
    // Wait for user input
    serialInputIndex = 0;
    memset(serialInput, 0, sizeof(serialInput));
    
    Serial.println(F("Enter hex data (e.g. 112233445566):"));
    
    while (true) {
        if (Serial.available() > 0) {
            char c = Serial.read();
            
            // Process the character
            if (c == '\n' || c == '\r') {
                // End of input
                if (serialInputIndex > 0) {
                    Serial.println(); // New line after input
                    break;
                }
            } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
                // Valid hex character
                if (serialInputIndex < sizeof(serialInput) - 1) {
                    serialInput[serialInputIndex++] = c;
                    Serial.write(c); // Echo the character
                }
            }
        }
    }
    
    // Convert hex string to bytes
    int dataIndex = 0;
    for (int i = 0; i < serialInputIndex; i += 2) {
        if (i + 1 < serialInputIndex && dataIndex < maxLength) {
            char highNibble = serialInput[i];
            char lowNibble = serialInput[i + 1];
            
            // Convert characters to hex values
            uint8_t highVal = (highNibble >= '0' && highNibble <= '9') ? (highNibble - '0') :
                             ((highNibble >= 'A' && highNibble <= 'F') ? (highNibble - 'A' + 10) :
                             ((highNibble >= 'a' && highNibble <= 'f') ? (highNibble - 'a' + 10) : 0));
            
            uint8_t lowVal = (lowNibble >= '0' && lowNibble <= '9') ? (lowNibble - '0') :
                            ((lowNibble >= 'A' && lowNibble <= 'F') ? (lowNibble - 'A' + 10) :
                            ((lowNibble >= 'a' && lowNibble <= 'f') ? (lowNibble - 'a' + 10) : 0));
            
            data[dataIndex++] = (highVal << 4) | lowVal;
        }
    }
}

// Handle ASCII write to tag function - optimized for Uno
void handleAsciiWrite() {
    // Define passwords - standard for most RFID tags
    uint32_t defaultPassword = 0x00000000;
    uint32_t targetPassword = 0x12345678;
    bool authenticated = false;
    
    Serial.println(F("ASCII to RFID Tag Write Process"));
    Serial.println(F("-------------------------------"));
    
    // Authentication process - simplified
    Serial.println(F("Checking authentication..."));
    if (checkAccessPassword(targetPassword)) {
        Serial.println(F("Successfully authenticated with 0x12345678"));
        authenticated = true;
    } else if (checkAccessPassword(defaultPassword)) {
        Serial.println(F("Default password accepted, setting secure password..."));
        if (setAccessPassword(defaultPassword, targetPassword)) {
            Serial.println(F("Set access password to 0x12345678"));
            authenticated = true;
        } else {
            Serial.println(F("Failed to set password"));
        }
    }
    
    if (!authenticated) {
        Serial.println(F("Could not authenticate, aborting"));
        return;
    }
    
    Serial.println(F("-------------------------------"));
    Serial.println(F("Enter ASCII text (max 12 chars):"));
    
    // Wait for user input - use fixed buffer to avoid String
    char userInput[13] = {0}; // 12 chars + null terminator
    uint8_t inputIdx = 0;
    
    while (true) {
        if (Serial.available() > 0) {
            char c = Serial.read();
            
            // End of input
            if (c == '\n' || c == '\r') {
                if (inputIdx > 0) {
                    userInput[inputIdx] = '\0'; // Ensure null termination
                    Serial.println(); // New line after input
                    break;
                }
            } else if (inputIdx < 12) { // Only accept up to 12 chars
                userInput[inputIdx++] = c;
                Serial.write(c); // Echo the character
            }
        }
    }
    
    // Convert ASCII to hex
    uint8_t epcData[12] = {0};
    asciiToHex(userInput, epcData, 12);
    
    // Display the converted data
    Serial.print(F("ASCII: \""));
    Serial.print(userInput);
    Serial.println(F("\""));
    
    Serial.print(F("Hex:   "));
    for (int i = 0; i < 12; i++) {
        if (epcData[i] < 0x10) Serial.print(F("0"));
        Serial.print(epcData[i], HEX);
        Serial.print(F(" "));
    }
    Serial.println();
    
    // Write to tag
    if (writeEpcData(epcData, 12, targetPassword)) {
        Serial.println(F("ASCII text written successfully to tag!"));
    } else {
        Serial.println(F("Failed to write ASCII text to tag"));
    }
}

// Automated function to write ASCII data received from webapp via ESP32
void writeAsciiToTag(String tagData, String commandId) {
    // Define passwords - standard for most RFID tags
    uint32_t defaultPassword = 0x00000000;
    uint32_t targetPassword = 0x12345678;
    bool authenticated = false;
    
    Serial.println(F("======= WRITING TAG FROM WEBAPP ======="));
    Serial.print(F("Data to write: "));
    Serial.println(tagData);
    
    // Make sure the data isn't too long
    if (tagData.length() > 12) {
        Serial.println(F("Warning: Data too long, truncating to 12 characters"));
        tagData = tagData.substring(0, 12);
    }
    
    // Authentication process - simplified
    Serial.println(F("Checking tag authentication..."));
    if (checkAccessPassword(targetPassword)) {
        Serial.println(F("Successfully authenticated with secure password"));
        authenticated = true;
    } else if (checkAccessPassword(defaultPassword)) {
        Serial.println(F("Default password accepted, setting secure password..."));
        if (setAccessPassword(defaultPassword, targetPassword)) {
            Serial.println(F("Set access password to secure value"));
            authenticated = true;
        } else {
            Serial.println(F("Failed to set password"));
        }
    }
    
    if (!authenticated) {
        Serial.println(F("Could not authenticate with tag, aborting"));
        espSerial.println("NAK:TAG_AUTH_FAILED");
        return;
    }
    
    // Convert ASCII to hex
    uint8_t epcData[12] = {0};
    char userInput[13]; // 12 chars + null terminator
    tagData.toCharArray(userInput, sizeof(userInput));
    
    asciiToHex(userInput, epcData, 12);
    
    // Display the converted data
    Serial.print(F("ASCII: \""));
    Serial.print(userInput);
    Serial.println(F("\""));
    
    Serial.print(F("Hex:   "));
    for (int i = 0; i < 12; i++) {
        if (epcData[i] < 0x10) Serial.print(F("0"));
        Serial.print(epcData[i], HEX);
        Serial.print(F(" "));
    }
    Serial.println();
    
    // Write to tag with retry logic
    const int MAX_RETRIES = 3;
    bool success = false;
    
    for (int retry = 0; retry < MAX_RETRIES && !success; retry++) {
        if (retry > 0) {
            Serial.print(F("Retry attempt #"));
            Serial.println(retry);
            delay(500); // Short delay between attempts
        }
        
        // Try to write with the target password first
        success = writeEpcData(epcData, 12, targetPassword);
        
        // If that fails and we authenticated with default, try with default password
        if (!success && checkAccessPassword(defaultPassword)) {
            Serial.println(F("Trying with default password instead..."));
            success = writeEpcData(epcData, 12, defaultPassword);
        }
        
        if (success) {
            Serial.println(F("Successfully wrote data from webapp to tag!"));
            // Send acknowledgment back to ESP32
            // Include command ID and EPC in acknowledgment
            espSerial.print("ACK:TAG_WRITTEN:");
            espSerial.print(commandId);
            espSerial.print(":");
            // Print EPC
            for (int i = 0; i < 12; i++) {
                if (epcData[i] < 0x10) espSerial.print("0");
                espSerial.print(epcData[i], HEX);
            }
            espSerial.println();
            break;
        }
    }
    
    if (!success) {
        Serial.println(F("Failed to write data to tag after multiple attempts"));
        // Send error back to ESP32
        espSerial.println("NAK:TAG_WRITE_FAILED");
    }
    
    // Show menu again after operation is complete
    displayMenu();
}