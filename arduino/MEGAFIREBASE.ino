#include <Arduino.h>
#include <SoftwareSerial.h>
// Simplified tag system for Arduino Uno
#define MAX_QUEUE_SIZE 5  // Reduced from 20 for memory constraints
#define TAG_DEBOUNCE_TIME 2000  // 2 seconds debounce time

// RFID Card structure
struct CARD {
    uint8_t epc[12];
    String epc_str;
};

// Global variables
CARD cards[5];  // Reduced array size for Uno memory constraints
uint8_t buffer[128] = {0};  // Reduced buffer size
bool isMultiplePolling = false;
char serialInput[50];  // Reduced buffer size
int serialInputIndex = 0;

// ESP communication variables
bool waitingForESPAck = false;
unsigned long lastESPSendTime = 0;
bool espReady = false;
uint16_t espTxSequence = 0;
unsigned long lastHeartbeatSent = 0;
unsigned long lastHeartbeatReceived = 0;
int consecutiveCommErrors = 0;

// Tag queue system
struct TagQueueItem {
  uint8_t epc[12];
  uint8_t rssi;
  unsigned long timestamp;
  uint8_t retryCount;
  bool valid;
  bool priority;
};

TagQueueItem tagQueue[MAX_QUEUE_SIZE];
int queueHead = 0;
int queueTail = 0;
bool queueFull = false;

// Function declarations
void displayMenu();
bool waitMsg(unsigned long timeout);
void cleanBuffer();
bool validateResponse(uint8_t* data, size_t length);
void buildCommand(uint8_t* cmd, size_t length);
bool sendAndWaitResponse(uint8_t* cmd, size_t length, unsigned long timeout = 1000);
bool readSingleTag();
void processTagData();
bool writeEpcData(uint8_t* data, uint8_t length, uint32_t password);
void handleAsciiWrite();
void inputEpcData(uint8_t* data, uint8_t maxLength);
void sendTagToESP(uint8_t* epcData, uint8_t length, uint8_t rssi);
void checkESPResponse();
void sendHeartbeatToESP();
void testESPCommunication();
bool checkAccessPassword(uint32_t password);
bool setAccessPassword(uint32_t currentPassword, uint32_t newPassword);

// Convert hex array to ASCII string
String hexToAscii(const uint8_t* hexArray, size_t length) {
  String result = "";
  
  for (size_t i = 0; i < length; i++) {
    // Only add printable ASCII characters (32-126)
    if (hexArray[i] >= 32 && hexArray[i] <= 126) {
      result += (char)hexArray[i];
    } else if (hexArray[i] != 0) {
      // For non-printable characters (except null), add a placeholder
      result += ".";
    }
  }
  
  return result;
}

// Convert ASCII string to hex array
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

// Send tag data to ESP with sequence number
void sendTagToESP(uint8_t* epcData, uint8_t length, uint8_t rssi) {
  // Check for ESP readiness
  if (!espReady) {
    if (millis() - lastHeartbeatSent > 5000) {
      Serial.println("ESP not ready, sending heartbeat...");
      sendHeartbeatToESP();
      return;
    }
    Serial.println("ESP not ready, waiting for connection");
    return;
  }
  
  // Check if we're already waiting for acknowledgment
  if (waitingForESPAck) {
    // Check for timeout on previous transmission
    if (millis() - lastESPSendTime > 5000) {
      Serial.println("ESP ack timeout - resetting communication");
      waitingForESPAck = false;
      consecutiveCommErrors++;
      
      // If too many consecutive errors, try to reset ESP communication
      if (consecutiveCommErrors > 5) {
        espReady = false;
        consecutiveCommErrors = 0;
      }
    } else {
      Serial.println("Still waiting for ESP to acknowledge previous tag");
      return;
    }
  }
  
  // Format: TAG:{sequence},{checksum},EPC,ASCII,RSSI,LOCATION
  String message = "TAG:";
  
  // Add sequence number
  message += String(espTxSequence);
  message += ",";
  
  // Calculate checksum (simple XOR of all EPC bytes)
  uint8_t checksum = 0;
  for (int i = 0; i < length; i++) {
    checksum ^= epcData[i];
  }
  
  // Add checksum
  message += String(checksum);
  message += ",";
  
  // Add EPC hex
  String epcHex = "";
  for (int i = 0; i < length; i++) {
    if (epcData[i] < 0x10) epcHex += "0";
    epcHex += String(epcData[i], HEX);
  }
  message += epcHex;
  message += ",";
  
  // Add ASCII representation
  message += hexToAscii(epcData, length);
  message += ",";
  
  // Add RSSI in hex format
  if (rssi < 0x10) message += "0";
  message += String(rssi, HEX);
  message += ",";
  
  // Add location (Living Room)
  message += "living-room";
  
  // Send to ESP
  Serial1.println(message);
  
  // Update state
  waitingForESPAck = true;
  lastESPSendTime = millis();
  espTxSequence++;
  
  // Debug output
  Serial.print("Sent to ESP: ");
  Serial.println(message);
  // Turn on LED until we get acknowledgment
  digitalWrite(13, HIGH);
}

// Send a heartbeat to ESP
void sendHeartbeatToESP() {
  String heartbeat = "HEARTBEAT:" + String(millis());
  Serial1.println(heartbeat);
  lastHeartbeatSent = millis();
}

// Enhanced ESP response checker
void checkESPResponse() {
    if (Serial1.available()) {
      String response = Serial1.readStringUntil('\n');
      response.trim();
      
      Serial.print("ESP response: ");
      Serial.println(response);
      
      // Reset consecutive error counter on any response
      consecutiveCommErrors = 0;
      
      // Check for ESP ready message
      if (response == "ESP:READY") {
        Serial.println("ESP is ready to receive RFID data!");
        espReady = true;
        
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
    }
    
    // Check for heartbeat timing
    unsigned long currentTime = millis();
    
    // Send heartbeat every 10 seconds if ESP is ready
    if (espReady && currentTime - lastHeartbeatSent > 10000) {
      sendHeartbeatToESP();
    }
    
    // Check for heartbeat timeout (30 seconds)
    if (espReady && lastHeartbeatReceived > 0 && currentTime - lastHeartbeatReceived > 30000) {
      Serial.println("WARNING: ESP heartbeat timeout - marking as not ready");
      espReady = false;
    }
}

// Add a test function to check ESP communication directly
void testESPCommunication() {
  Serial.println("\n--- ESP COMMUNICATION TEST ---");
  Serial.println("Sending test message to ESP...");
  
  // Send a test message
  Serial1.println("TEST:PING");
  
  // Wait for response with timeout
  unsigned long startTime = millis();
  bool received = false;
  
  while (millis() - startTime < 3000 && !received) {
    if (Serial1.available() > 0) {
      String response = Serial1.readStringUntil('\n');
      response.trim();
      
      Serial.print("ESP response: [");
      Serial.print(response);
      Serial.println("]");
      
      received = true;
    }
    delay(100);
  }
  
  if (!received) {
    Serial.println("ERROR: No response from ESP after 3 seconds");
    Serial.println("Check connections and ESP power/programming");
  }
  
  Serial.println("--- END COMMUNICATION TEST ---\n");
}

// Add tag to queue
void addTagToQueue(uint8_t* epcData, uint8_t rssi, bool priority = false) {
  // Check if this tag was recently added (debounce)
  unsigned long currentTime = millis();
  bool isDuplicate = false;
  
  // First check for duplicates in queue
  int tempIndex = queueHead;
  while (tempIndex != queueTail) {
    if (tagQueue[tempIndex].valid) {
      // Compare EPC
      bool match = true;
      for (int i = 0; i < 12; i++) {
        if (tagQueue[tempIndex].epc[i] != epcData[i]) {
          match = false;
          break;
        }
      }
      
      // If it's a match and was added recently, consider it a duplicate
      if (match && (currentTime - tagQueue[tempIndex].timestamp < TAG_DEBOUNCE_TIME)) {
        isDuplicate = true;
        // Update the RSSI and timestamp on the existing entry
        tagQueue[tempIndex].rssi = rssi;
        tagQueue[tempIndex].timestamp = currentTime;
        // If new entry is priority, make existing entry priority too
        if (priority) {
          tagQueue[tempIndex].priority = true;
        }
        break;
      }
    }
    tempIndex = (tempIndex + 1) % MAX_QUEUE_SIZE;
  }
  
  // Only add if not a duplicate and queue isn't full
  if (!isDuplicate) {
    // Check if queue is full
    if (queueFull) {
      return; // Simply drop the tag if queue is full
    }
    
    // Add to queue
    memcpy(tagQueue[queueTail].epc, epcData, 12);
    tagQueue[queueTail].rssi = rssi;
    tagQueue[queueTail].timestamp = currentTime;
    tagQueue[queueTail].retryCount = 0;
    tagQueue[queueTail].valid = true;
    tagQueue[queueTail].priority = priority;
    
    // Update tail
    int newTail = (queueTail + 1) % MAX_QUEUE_SIZE;
    if (newTail == queueHead) {
      queueFull = true;
    }
    queueTail = newTail;
  }
}

// Get next tag from queue
bool getNextTagFromQueue(uint8_t* epcData, uint8_t* rssi) {
  if (queueHead == queueTail && !queueFull) {
    return false; // Queue is empty
  }
  
  // Process tag at head
  if (tagQueue[queueHead].valid) {
    // Copy tag data
    memcpy(epcData, tagQueue[queueHead].epc, 12);
    *rssi = tagQueue[queueHead].rssi;
    
    // Move head forward
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

// Declare SoftwareSerial globally so it's accessible in all functions
SoftwareSerial rfidSerial(2, 3); // RX, TX

void setup() {
    Serial.begin(9600);    // PC communication
    Serial1.begin(9600);   // ESP communication (using hardware TX/RX on Uno)
    
    // For RFID module we'll use SoftwareSerial since Uno has only one hardware UART
    pinMode(2, INPUT);  // RX from RFID module
    pinMode(3, OUTPUT); // TX to RFID module
    rfidSerial.begin(115200);
    
    pinMode(13, OUTPUT); // Built-in LED for status
    digitalWrite(13, LOW);
    
    Serial.println("RFID Tracking System - Uno Version");
    Serial.println("-------------------------");
    Serial.println("Initializing...");
    
    // Simple initialization for Uno
    Serial.println("Module Ready!");
    
    displayMenu();
}

void loop() {
  // Check for ESP communications first
  checkESPResponse();
  
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
  }
  
  // Check for user input (menu selections)
  if (Serial.available()) {
    char c = Serial.read();  
    switch (c) {
      case '1':
        Serial.println("Reading single tag...");
        readSingleTag();
        displayMenu();
        break;
      case '2':
        Serial.println("Write ASCII Text to Tag");
        // Clear serial input buffer first
        while (Serial.available()) {
          Serial.read();
        }
        handleAsciiWrite();
        displayMenu();
        break;
      case '3':
        Serial.println("Test ESP Communication");
        testESPCommunication();
        displayMenu();
        break;
      default:
        displayMenu();
        break;
    }
  }
}

void displayMenu() {
    Serial.println("\n--- RFID Tracking System ---");
    Serial.println("1. Read Single Tag");
    Serial.println("2. Write ASCII to Tag");
    Serial.println("3. Test ESP Communication");
    Serial.print("> ");
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
        Serial.println("Timeout waiting for start byte");
        return false;
    }
    
    // Now read the rest of the message
    start = millis(); // Reset timeout
    while (millis() - start < timeout && i < sizeof(buffer)) {
        if (rfidSerial.available()) {
            buffer[i] = rfidSerial.read();
            
            // Check if we have a complete message by looking for end marker
            if (buffer[i] == 0x7E) {
                // Print received data for debugging
                Serial.print("Received: ");
                for (int j = 0; j <= i; j++) {
                    if (buffer[j] < 0x10) Serial.print("0");
                    Serial.print(buffer[j], HEX);
                    Serial.print(" ");
                }
                Serial.println();
                return true;
            }
            i++;
        }
    }
    
    Serial.println("Timeout waiting for complete message");
    return false;
}

// Validate a response message - simplified for Uno
bool validateResponse(uint8_t* data, size_t length) {
    // Minimum valid packet: BB + Type + CMD + LEN_MSB + LEN_LSB + DATA(0) + CK + 7E = 8 bytes
    if (length < 8 || data[0] != 0xBB || data[length-1] != 0x7E) {
        return false;
    }
    
    // For all frames, accept them to simplify the Uno implementation
    return true;
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
    
    // Send the command
    for (size_t i = 0; i < length; i++) {
        rfidSerial.write(cmd[i]);
    }
    
    // Wait for response
    return waitMsg(timeout);
}

// Clean the response buffer
void cleanBuffer() {
    memset(buffer, 0, sizeof(buffer));
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
            Serial.print("Tag found! RSSI: ");
            Serial.print(rssi, HEX);
            Serial.print(" EPC: ");
            
            // Copy EPC to our card structure (assuming 12 bytes EPC)
            int currentIndex = 0;
            for (int i = 8; i < 20; i++) {
                cards[currentIndex].epc[i-8] = buffer[i];
                if (buffer[i] < 0x10) Serial.print("0");
                Serial.print(buffer[i], HEX);
                Serial.print(" ");
            }
            
            // Add ASCII interpretation
            String asciiEpc = hexToAscii(cards[currentIndex].epc, 12);
            Serial.print("\nASCII: \"");
            Serial.print(asciiEpc);
            Serial.println("\"");
            
            // Update EPC string representation
            cards[currentIndex].epc_str = "";
            for (int i = 0; i < 12; i++) {
                if (cards[currentIndex].epc[i] < 0x10) {
                    cards[currentIndex].epc_str += "0";
                }
                cards[currentIndex].epc_str += String(cards[currentIndex].epc[i], HEX);
            }
            
            // Add tag to queue to send to ESP
            addTagToQueue(cards[currentIndex].epc, rssi, true);
            
            return true;
        }
    }
    Serial.println("No tag found or communication error");
    return false;
}

void processTagData() {
  uint8_t rssi = buffer[5];
  
  // Build EPC string and store in temporary array
  uint8_t tempEpc[12];
  
  for (int i = 8; i < 20; i++) {
    tempEpc[i-8] = buffer[i];
  }
  
  // Add ASCII interpretation
  String asciiEpc = hexToAscii(tempEpc, 12);
  Serial.print("Tag detected - ASCII: \"");
  Serial.print(asciiEpc);
  Serial.println("\"");
  
  // Add to queue as regular priority
  addTagToQueue(tempEpc, rssi, false);
}

// Check if we can access the tag with the provided password
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

// Set access password for the tag
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
        if (buffer[1] == 0x01 && buffer[2] == 0x49 && buffer[5] == 0x00) {
            return true;
        }
    }
    return false;
}

// Write data to the EPC memory bank
bool writeEpcData(uint8_t* data, uint8_t length, uint32_t password) {
    // Ensure length is valid (max 12 bytes for EPC)
    if (length > 12) {
        Serial.println("EPC data too long, maximum is 12 bytes");
        return false;
    }
    
    // Calculate correct command length
    uint8_t cmdLength = 16 + length;
    uint8_t* cmd = new uint8_t[cmdLength]; 
    
    // Calculate data length in words (2 bytes per word)
    uint8_t dataLengthWords = length / 2;
    if (length % 2 != 0) {
        dataLengthWords += 1;
    }
    
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
        cmdLength++;  // Adjust command length
    }
    
    // Checksum and end marker will be added by buildCommand
    cmd[14 + length] = 0x00;     // Checksum placeholder
    cmd[15 + length] = 0x7E;     // End marker
    
    // Send command and wait for response
    bool result = sendAndWaitResponse(cmd, cmdLength, 3000);
    
    // Free allocated memory
    delete[] cmd;
    
    if (result) {
        // Check for successful response
        if (buffer[1] == 0x01 && buffer[2] == 0x49) {
            Serial.println("EPC data written successfully!");
            return true;
        }
    }
    Serial.println("Failed to write EPC data");
    return false;
}

// Helper function to input EPC data from Serial
void inputEpcData(uint8_t* data, uint8_t maxLength) {
    memset(data, 0, maxLength); // Clear the buffer
    
    // Wait for user input
    serialInputIndex = 0;
    memset(serialInput, 0, sizeof(serialInput));
    
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
            // Ignore other characters
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

void handleAsciiWrite() {
    // Define passwords
    uint32_t defaultPassword = 0x00000000;
    uint32_t targetPassword = 0x12345678;
    bool authenticated = false;
    
    Serial.println("ASCII to RFID Tag Write Process");
    Serial.println("-------------------------------");
    
    // Authentication process
    Serial.println("Checking authentication...");
    if (checkAccessPassword(targetPassword)) {
        Serial.println("Successfully authenticated with 0x12345678");
        authenticated = true;
    } else if (checkAccessPassword(defaultPassword)) {
        Serial.println("Default password accepted, attempting to set secure password...");
        if (setAccessPassword(defaultPassword, targetPassword)) {
            Serial.println("Successfully set access password to 0x12345678");
            authenticated = true;
        } else {
            Serial.println("Failed to set password - password might already be changed");
            if (checkAccessPassword(targetPassword)) {
                Serial.println("Target password authenticated on second attempt");
                authenticated = true;
            }
        }
    }
    
    if (!authenticated) {
        Serial.println("Could not authenticate with any password, aborting");
        return;
    }
    
    Serial.println("-------------------------------");
    Serial.println("Enter ASCII text to write to tag (max 12 characters):");
    
    // Wait for user input
    String userInput = "";
    while (true) {
        if (Serial.available() > 0) {
            char c = Serial.read();
            
            // End of input
            if (c == '\n' || c == '\r') {
                if (userInput.length() > 0) {
                    Serial.println(); // New line after input
                    break;
                }
            } else {
                userInput += c;
                Serial.write(c); // Echo the character
            }
        }
    }
    
    // Truncate if longer than 12 chars
    if (userInput.length() > 12) {
        Serial.println("\nWarning: Input too long, truncating to 12 characters");
        userInput = userInput.substring(0, 12);
    }
    
    // Convert ASCII to hex
    uint8_t epcData[12] = {0};
    asciiToHex(userInput.c_str(), epcData, 12);
    
    // Display the converted data
    Serial.print("ASCII: \"");
    Serial.print(userInput);
    Serial.println("\"");
    
    Serial.print("Hex:   ");
    for (int i = 0; i < 12; i++) {
        if (epcData[i] < 0x10) Serial.print("0");
        Serial.print(epcData[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // Write to tag
    if (writeEpcData(epcData, 12, targetPassword)) {
        Serial.println("ASCII text written successfully to tag!");
    } else {
        Serial.println("Failed to write ASCII text to tag");
    }
}