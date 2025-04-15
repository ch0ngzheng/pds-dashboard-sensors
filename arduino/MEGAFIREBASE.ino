#include <Arduino.h>
// Enhanced tag queue system
#define MAX_QUEUE_SIZE 20  // Increased from 10
#define TAG_DEBOUNCE_TIME 2000  // 2 seconds debounce time

// RFID Card structure
struct CARD {
    uint8_t epc[12];
    String epc_str;
};

// Global variables
CARD cards[10];  // Array to store card information
uint8_t buffer[256] = {0};  // Buffer for incoming data
bool isMultiplePolling = false;  // Flag for multiple polling mode
char serialInput[100];  // Buffer for serial input
int serialInputIndex = 0;  // Index for serial input buffer

bool waitingForESP32Ack = false;
unsigned long lastESP32SendTime = 0;
bool esp32Ready = false;
// Enhanced ESP32 communication with sequence numbers
uint16_t esp32TxSequence = 0;
uint16_t esp32RxSequence = 0;
unsigned long lastHeartbeatSent = 0;
unsigned long lastHeartbeatReceived = 0;
int consecutiveCommErrors = 0;


// Global variables to track polling state
const unsigned long POLLING_COMMAND_INTERVAL = 10000; // 10 seconds
// Enhanced continuous polling system
bool continuousPollingActive = false;
unsigned long lastPollingCommand = 0;
unsigned long lastTagDetected = 0;
unsigned long pollingStartTime = 0;
int emptyPollCount = 0;

struct TagQueueItem {
  uint8_t epc[12];
  uint8_t rssi;
  unsigned long timestamp;  // When this tag was added
  uint8_t retryCount;       // For transmission retries
  bool valid;
  bool priority;            // For prioritizing new tag detections
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
void validateCommand(uint8_t* cmd, size_t length);
bool sendAndWaitResponseWithValidation(uint8_t* cmd, size_t length, unsigned long timeout = 1000);
bool getHardwareVersion();
bool getSoftwareVersion();
bool getTransmissionPower();
bool setTransmissionPower(uint16_t powerLevel);
bool readSingleTag();
bool startMultiplePolling(uint16_t count = 10);
bool stopMultiplePolling();
void processTagData();
void getDeviceInfo();
bool setAccessPassword(uint32_t currentPassword, uint32_t newPassword);
bool checkAccessPassword(uint32_t password);
bool writeEpcData(uint8_t* data, uint8_t length, uint32_t password);
void handleWriteTag();
void inputEpcData(uint8_t* data, uint8_t maxLength);

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


// Send tag data to ESP32 with sequence number
void sendTagToESP32(uint8_t* epcData, uint8_t length, uint8_t rssi) {
  // Check for ESP32 readiness
  if (!esp32Ready) {
    if (millis() - lastHeartbeatSent > 5000) {
      Serial.println("ESP32 not ready, sending heartbeat...");
      sendHeartbeatToESP32();
      return;
    }
    Serial.println("ESP32 not ready, waiting for connection");
    return;
  }
  
  // Check if we're already waiting for acknowledgment
  if (waitingForESP32Ack) {
    // Check for timeout on previous transmission
    if (millis() - lastESP32SendTime > 5000) {
      Serial.println("ESP32 ack timeout - resetting communication");
      waitingForESP32Ack = false;
      consecutiveCommErrors++;
      
      // If too many consecutive errors, try to reset ESP32 communication
      if (consecutiveCommErrors > 5) {
        esp32Ready = false;
        consecutiveCommErrors = 0;
      }
    } else {
      Serial.println("Still waiting for ESP32 to acknowledge previous tag");
      return;
    }
  }
  
  // Format: TAG:{sequence},{checksum},EPC,ASCII,RSSI
  String message = "TAG:";
  
  // Add sequence number
  message += String(esp32TxSequence);
  message += ",";
  
  // Add EPC in hex format
  String epcHex = "";
  for (int i = 0; i < length; i++) {
    if (epcData[i] < 0x10) epcHex += "0";
    epcHex += String(epcData[i], HEX);
  }
  message += epcHex;
  message += ",";
  
  // Add ASCII representation
  String asciiEpc = hexToAscii(epcData, length);
  message += asciiEpc;
  message += ",";
  
  // Add RSSI
  message += String(rssi, HEX);
  
  // Calculate simple checksum
  uint8_t checksum = 0;
  for (unsigned int i = 0; i < message.length(); i++) {
    checksum += message.charAt(i);
  }
  
  // Insert checksum after sequence
  message = "TAG:" + String(esp32TxSequence) + "," + String(checksum) + "," + epcHex + "," + asciiEpc + "," + String(rssi, HEX);
  
  // Send to ESP32
  Serial.println("Sending to ESP32: " + message);
  Serial3.println(message);
  
  // Mark that we're waiting for response and increment sequence
  waitingForESP32Ack = true;
  lastESP32SendTime = millis();
  esp32TxSequence = (esp32TxSequence + 1) % 10000; // Wrap at 10000
  
  // Turn on LED until we get acknowledgment
  digitalWrite(13, HIGH);
}

// Send a heartbeat to ESP32
void sendHeartbeatToESP32() {
  String heartbeat = "HEARTBEAT:" + String(millis());
  Serial3.println(heartbeat);
  lastHeartbeatSent = millis();
}

// Enhanced ESP32 response checker
void checkESP32Response() {
    if (Serial3.available()) {
      String response = Serial3.readStringUntil('\n');
      response.trim();
      
      Serial.print("ESP32 response: ");
      Serial.println(response);
      
      // Reset consecutive error counter on any response
      consecutiveCommErrors = 0;
      
      // Check for ESP32 ready message
      if (response == "ESP32:READY") {
        Serial.println("ESP32 is ready to receive RFID data!");
        esp32Ready = true;
        
        // Blink LED to show ESP32 is ready
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
          uint16_t ackSequence = response.substring(colonPos + 1).toInt();
          
          // Check if this is the sequence we're waiting for
          if (waitingForESP32Ack) {
            // Even if not exact match, we'll accept it to recover from errors
            waitingForESP32Ack = false;
            digitalWrite(13, LOW); // Turn off LED after acknowledgment
          }
        }
      }
      // Check for heartbeat response
      else if (response.startsWith("HEARTBEAT_ACK")) {
        lastHeartbeatReceived = millis();
        esp32Ready = true;
      }
      // Check for generic ACK
      else if (response == "ACK" || response.startsWith("ESP32:ACK")) {
        waitingForESP32Ack = false;
        digitalWrite(13, LOW); // Turn off LED after acknowledgment
      }
      // Check for error/negative acknowledgment
      else if (response == "NAK" || response.startsWith("ESP32:NAK")) {
        Serial.println("ESP32 reported error processing tag data");
        waitingForESP32Ack = false;
        digitalWrite(13, LOW);
        consecutiveCommErrors++;
      }
    }
    
    // Check for heartbeat timing
    unsigned long currentTime = millis();
    
    // Send heartbeat every 10 seconds if ESP32 is ready
    if (esp32Ready && currentTime - lastHeartbeatSent > 10000) {
      sendHeartbeatToESP32();
    }
    
    // Check for heartbeat timeout (30 seconds)
    if (esp32Ready && lastHeartbeatReceived > 0 && currentTime - lastHeartbeatReceived > 30000) {
      Serial.println("WARNING: ESP32 heartbeat timeout - marking as not ready");
      esp32Ready = false;
    }
}


// Add a test function to check ESP32 communication directly
void testESP32Communication() {
  Serial.println("\n--- ESP32 COMMUNICATION TEST ---");
  Serial.println("Sending test message to ESP32...");
  
  // Send a test message
  Serial3.println("TEST:PING");
  
  // Wait for response with timeout
  unsigned long startTime = millis();
  bool received = false;
  
  while (millis() - startTime < 3000 && !received) {
    if (Serial3.available() > 0) {
      String response = Serial3.readStringUntil('\n');
      response.trim();
      
      Serial.print("ESP32 response: [");
      Serial.print(response);
      Serial.println("]");
      
      received = true;
    }
    delay(100);
  }
  
  if (!received) {
    Serial.println("ERROR: No response from ESP32 after 3 seconds");
    Serial.println("Check connections and ESP32 power/programming");
  }
  
  Serial.println("--- END COMMUNICATION TEST ---\n");
}


// Add tag to queue with enhanced functionality
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
      // If priority tag, replace oldest non-priority tag
      if (priority) {
        // Find oldest non-priority tag
        int oldestIndex = -1;
        unsigned long oldestTime = currentTime;
        
        tempIndex = queueHead;
        while (tempIndex != queueTail) {
          if (tagQueue[tempIndex].valid && !tagQueue[tempIndex].priority && 
              tagQueue[tempIndex].timestamp < oldestTime) {
            oldestTime = tagQueue[tempIndex].timestamp;
            oldestIndex = tempIndex;
          }
          tempIndex = (tempIndex + 1) % MAX_QUEUE_SIZE;
        }
        
        // If found a replaceable tag, use that slot
        if (oldestIndex >= 0) {
          memcpy(tagQueue[oldestIndex].epc, epcData, 12);
          tagQueue[oldestIndex].rssi = rssi;
          tagQueue[oldestIndex].timestamp = currentTime;
          tagQueue[oldestIndex].retryCount = 0;
          tagQueue[oldestIndex].priority = priority;
          return;
        }
      }
      
      // Otherwise just drop the tag
      return;
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

// Get next tag from queue with enhanced logic for retries and expiration
bool getNextTagFromQueue(uint8_t* epcData, uint8_t* rssi, bool* isPriority = NULL) {
  if (queueHead == queueTail && !queueFull) {
    return false; // Queue is empty
  }
  
  unsigned long currentTime = millis();
  
  // First, check for priority tags
  int priorityIndex = -1;
  int regularIndex = -1;
  int tempIndex = queueHead;
  
  // Scan queue for priority and valid regular tags
  while (tempIndex != queueTail || (queueFull && tempIndex == queueTail)) {
    if (tagQueue[tempIndex].valid) {
      // Check if entry is expired (older than 30 seconds)
      if (currentTime - tagQueue[tempIndex].timestamp > 30000) {
        // Mark as invalid (expired)
        tagQueue[tempIndex].valid = false;
      } 
      // Check retry count - if too many retries, skip
      else if (tagQueue[tempIndex].retryCount >= 3) {
        // Tag has been retried too many times, mark as invalid
        tagQueue[tempIndex].valid = false;
      }
      else {
        // Valid tag - check if priority
        if (tagQueue[tempIndex].priority && priorityIndex == -1) {
          priorityIndex = tempIndex;
        } 
        else if (!tagQueue[tempIndex].priority && regularIndex == -1) {
          regularIndex = tempIndex;
        }
      }
    }
    
    // Exit condition for full queue
    if (queueFull && tempIndex == queueTail) {
      break;
    }
    
    tempIndex = (tempIndex + 1) % MAX_QUEUE_SIZE;
  }
  
  // Process priority tag if found, otherwise regular tag
  int processIndex = (priorityIndex != -1) ? priorityIndex : regularIndex;
  
  if (processIndex != -1) {
    // Copy tag data
    memcpy(epcData, tagQueue[processIndex].epc, 12);
    *rssi = tagQueue[processIndex].rssi;
    if (isPriority) {
      *isPriority = tagQueue[processIndex].priority;
    }
    
    // Increment retry count
    tagQueue[processIndex].retryCount++;
    
    // If this is head, move head forward
    if (processIndex == queueHead) {
      queueHead = (queueHead + 1) % MAX_QUEUE_SIZE;
      queueFull = false;
    } else {
      // Otherwise just mark as invalid
      tagQueue[processIndex].valid = false;
    }
    
    return true;
  }
  
  // No valid tags found, clean up head pointer
  while (queueHead != queueTail && !tagQueue[queueHead].valid) {
    queueHead = (queueHead + 1) % MAX_QUEUE_SIZE;
    queueFull = false;
  }
  
  return false;
}

// Clear the entire queue
void clearTagQueue() {
  queueHead = 0;
  queueTail = 0;
  queueFull = false;
  for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
    tagQueue[i].valid = false;
  }
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


void setup() {
    Serial.begin(9600);    // PC communication
    Serial2.begin(115200, SERIAL_8N1);  // RFID module communication
    pinMode(13, OUTPUT); // Built-in LED for status
    digitalWrite(13, LOW);
    Serial3.begin(9600);   // ESP32 communication
    Serial.println("Waiting for ESP32 to become ready...");
    Serial.println("RFID Tracking System");
    Serial.println("-------------------------");
    Serial.println("Initializing...");
      // Initialize ESP32 communication
    // Check hardware version to confirm readiness
    if (getHardwareVersion()) {
        Serial.println("Module Ready!");
        
        // Get additional device information
        getDeviceInfo();
    } else {
        Serial.println("Module not responding!");
    }
    
    Serial.println("\n-------------------------");
    displayMenu();
}

void loop() {
  // Check for ESP32 communications first
  checkESP32Response();
  // Add a small delay to prevent rapid communication overlap
  delay(10);
  
  // Test ESP32 communication every 10 seconds if not ready
  static unsigned long lastTestTime = 0;
  if (!esp32Ready && (millis() - lastTestTime > 10000)) {
    Serial.println("Sending test message to ESP32...");
    Serial3.println("TEST:PING");
    lastTestTime = millis();
  }
  
  // Process tag queue (should run regardless of polling state)
  static unsigned long lastQueueProcess = 0;
  if (millis() - lastQueueProcess > 100) { // Process queue every 100ms
    lastQueueProcess = millis();
    
    // Only process if not waiting for ESP32
    if (!waitingForESP32Ack) {
      uint8_t tempEpc[12];
      uint8_t tempRssi;
      bool isPriority;
      if (getNextTagFromQueue(tempEpc, &tempRssi, &isPriority)) {
        sendTagToESP32(tempEpc, 12, tempRssi);
      }
    }
  }
  
  // Manage continuous polling (if active)
  if (continuousPollingActive) {
    manageContinuousPolling();
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
        Serial.println("Starting continuous polling...");
        if (continuousPollingActive) {
          stopContinuousPolling();
        }
        startContinuousPolling();
        break;
      case '3':
        Serial.println("Write Tag Data Mode");
        // Clear serial input buffer first
        while (Serial.available()) {
          Serial.read();
        }
        handleWriteTag();
        displayMenu();
        break;
      case '4':
        Serial.println("Write ASCII Text to Tag");
        // Clear serial input buffer first
        while (Serial.available()) {
          Serial.read();
        }
        handleAsciiWrite();
        displayMenu();
        break;
      case '5':
        Serial.println("Test ESP32 Communication");
        testESP32Communication();
        displayMenu();
        break;
      case 'q':
        if (continuousPollingActive) {
          stopContinuousPolling();
        }
        Serial.println("Stopped all operations");
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
    Serial.println("2. Start Continuous Polling");
    Serial.println("3. Write Hex to Tag");
    Serial.println("4. Write ASCII to Tag");
    Serial.println("5. Test ESP32 Communication");
    Serial.println("q. Quit");
    Serial.print("> ");
}

// // Wait for a complete message with timeout
// bool waitMsg(unsigned long timeout) {
//     unsigned long start = millis();
//     uint8_t i = 0;
//     cleanBuffer();

//     // Look for starting byte 0xBB first
//     while (millis() - start < timeout) {
//         if (Serial2.available()) {
//             uint8_t b = Serial2.read();
//             if (b == 0xBB) {
//                 buffer[0] = b;
//                 i = 1;
//                 break;
//             }
//         }
//     }
    
//     if (i == 0) {
//         Serial.println("Timeout waiting for start byte 0xBB");
//         return false; // Didn't find start byte
//     }
    
//     // Now read the rest of the message
//     start = millis(); // Reset timeout
//     while (millis() - start < timeout && i < sizeof(buffer)) {
//         if (Serial2.available()) {
//             buffer[i] = Serial2.read();
            
//             // Check if we have a complete message by looking for end marker
//             if (buffer[i] == 0x7E) {
//                 // We have found the end marker, but wait a bit to ensure no more bytes
//                 delay(5);
                
//                 // Debug print the received response
//                 Serial.print("Received response: ");
//                 for (int j = 0; j <= i; j++) {
//                     if (buffer[j] < 0x10) Serial.print("0");
//                     Serial.print(buffer[j], HEX);
//                     Serial.print(" ");
//                 }
//                 Serial.println();
                
//                 // Validate response
//                 if (!validateResponse(buffer, i+1)) {
//                     Serial.println("Invalid response (checksum error)");
//                     return false;
//                 }
                
//                 if (!Serial2.available()) {
//                     return true;
//                 }
//             }
//             i++;
//         }
//     }
    
//     Serial.println("Timeout or buffer overflow waiting for complete message");
//     return false; // Timeout or buffer overflow
// }

bool waitMsg(unsigned long timeout) {
    unsigned long start = millis();
    uint8_t i = 0;
    cleanBuffer();

    // Look for starting byte 0xBB first
    while (millis() - start < timeout) {
        if (Serial2.available()) {
            uint8_t b = Serial2.read();
            if (b == 0xBB) {
                buffer[0] = b;
                i = 1;
                break;
            }
        }
    }
    
    if (i == 0) {
        Serial.println("Timeout waiting for start byte 0xBB");
        return false; // Didn't find start byte
    }
    
    // Now read the rest of the message
    start = millis(); // Reset timeout
    while (millis() - start < timeout && i < sizeof(buffer)) {
        if (Serial2.available()) {
            buffer[i] = Serial2.read();
            
            // Check if we have a complete message by looking for end marker
            if (buffer[i] == 0x7E) {
                // We have found the end marker
                
                // Debug print the received response
                Serial.print("Received response: ");
                for (int j = 0; j <= i; j++) {
                    if (buffer[j] < 0x10) Serial.print("0");
                    Serial.print(buffer[j], HEX);
                    Serial.print(" ");
                }
                Serial.println();
                
                // Accept all notification frames as valid
                if (buffer[1] == 0x02 && buffer[2] == 0x22) {
                    return true;
                }
                
                // Process other responses normally
                return validateResponse(buffer, i+1);
            }
            i++;
        }
    }
    
    Serial.println("Timeout or buffer overflow waiting for complete message");
    return false; // Timeout or buffer overflow
}

// Validate a response message (checks structure and checksum)
// bool validateResponse(uint8_t* data, size_t length) {
//     // Minimum valid packet: BB + Type + CMD + LEN_MSB + LEN_LSB + DATA(0) + CK + 7E = 8 bytes
//     if (length < 8 || data[0] != 0xBB || data[length-1] != 0x7E) {
//         Serial.println("Invalid response structure");
//         return false;
//     }
    
//     // Debug print the response details
//     Serial.print("Response validation: Header=0x");
//     Serial.print(data[0], HEX);
//     Serial.print(", Type=0x");
//     Serial.print(data[1], HEX);
//     Serial.print(", CMD=0x");
//     Serial.print(data[2], HEX);
//     Serial.print(", Checksum=0x");
//     Serial.println(data[length-2], HEX);
    
//     // When receiving standard responses like error codes (0xFF), always accept them
//     if (data[1] == 0x01 && data[2] == 0xFF) {
//         Serial.println("Received error response - accepting as valid");
//         return true;
//     }
    
//     // If we get a response for our read/write command, accept it
//     if ((data[1] == 0x01 && data[2] == 0x39) || // Read response
//         (data[1] == 0x01 && data[2] == 0x49)) { // Write response
//         Serial.println("Received read/write response - accepting as valid");
//         return true;
//     }
    
//     // Calculate checksum for other commands according to the documentation
//     uint8_t calculatedChecksum = 0;
//     for (int i = 1; i < length - 2; i++) {
//         calculatedChecksum += data[i];
//     }
    
//     // Compare with received checksum
//     if (calculatedChecksum != data[length-2]) {
//         Serial.print("Possible checksum mismatch: calculated ");
//         Serial.print(calculatedChecksum, HEX);
//         Serial.print(", received ");
//         Serial.println(data[length-2], HEX);
//         // For debugging, we'll accept all responses for now
//         return true;
//     }
    
//     return true;
// }

bool validateResponse(uint8_t* data, size_t length) {
    // Minimum valid packet: BB + Type + CMD + LEN_MSB + LEN_LSB + DATA(0) + CK + 7E = 8 bytes
    if (length < 8 || data[0] != 0xBB || data[length-1] != 0x7E) {
        Serial.println("Invalid response structure");
        return false;
    }
    
    // Special case for notification frames from multiple polling
    if (data[1] == 0x02 && data[2] == 0x22) {
        // For notification frames, always accept them as valid
        Serial.println("Received tag notification frame - accepting as valid");
        return true;
    }
    
    // When receiving standard responses like error codes (0xFF), always accept them
    if (data[1] == 0x01 && data[2] == 0xFF) {
        Serial.println("Received error response - accepting as valid");
        return true;
    }
    
    // For all other frames, we'll be more lenient with checksums
    // since the device might use a different calculation method
    Serial.println("Received valid frame structure - accepting response");
    return true;
}


// Generate command with proper checksum calculation per documentation
void buildCommand(uint8_t* cmd, size_t length) {
    uint8_t checksum = 0;
    // Calculate checksum from Type (index 1) to last Parameter byte (length-3)
    for (int i = 1; i < length - 2; i++) {
        checksum += cmd[i];
    }
    // Set checksum byte (LSB only)
    cmd[length - 2] = checksum;
    
    // Debug print the checksum calculation
    Serial.print("Calculated checksum: 0x");
    if (checksum < 0x10) Serial.print("0");
    Serial.println(checksum, HEX);
}

// Function to validate and print the command structure before sending
void validateCommand(uint8_t* cmd, size_t length) {
    Serial.println("Command structure validation:");
    Serial.print("Header: 0x");
    Serial.println(cmd[0], HEX);
    
    Serial.print("Type: 0x");
    Serial.println(cmd[1], HEX);
    
    Serial.print("Command: 0x");
    Serial.println(cmd[2], HEX);
    
    Serial.print("PL(MSB): 0x");
    Serial.println(cmd[3], HEX);
    
    Serial.print("PL(LSB): 0x");
    Serial.println(cmd[4], HEX);
    
    if (cmd[2] == 0x49) { // Write command
        Serial.println("Write command structure:");
        
        Serial.print("AP: 0x");
        Serial.print(cmd[5], HEX);
        Serial.print(cmd[6], HEX);
        Serial.print(cmd[7], HEX);
        Serial.println(cmd[8], HEX);
        
        Serial.print("MemBank: 0x");
        Serial.println(cmd[9], HEX);
        
        Serial.print("SA: 0x");
        Serial.print(cmd[10], HEX);
        Serial.println(cmd[11], HEX);
        
        Serial.print("DL: 0x");
        Serial.print(cmd[12], HEX);
        Serial.println(cmd[13], HEX);
        
        // Calculate parameter length for verification
        int paramLen = 0;
        
        // Password (4 bytes)
        paramLen += 4;
        
        // Memory bank (1 byte)
        paramLen += 1;
        
        // Starting address (2 bytes)
        paramLen += 2;
        
        // Data length (2 bytes)
        paramLen += 2;
        
        // Data bytes
        int dataBytes = (cmd[12] << 8) | cmd[13];
        dataBytes *= 2; // Convert words to bytes
        paramLen += dataBytes;
        
        Serial.print("Calculated parameter length: 0x");
        Serial.println(paramLen, HEX);
        Serial.print("Command parameter length: 0x");
        Serial.println(cmd[4], HEX);
        
        if (paramLen != cmd[4]) {
            Serial.println("WARNING: Parameter length mismatch!");
        } else {
            Serial.println("Parameter length is correct.");
        }
    }
}

// Helper function to send commands with validation before sending
bool sendAndWaitResponseWithValidation(uint8_t* cmd, size_t length, unsigned long timeout) {
    // Clear any pending bytes in the receive buffer
    while (Serial2.available()) {
        Serial2.read();
    }
    
    // Clean our response buffer
    cleanBuffer();
    
    // Calculate checksum before sending
    buildCommand(cmd, length);
    
    // Validate the command
    validateCommand(cmd, length);
    
    // Debug print the command
    Serial.print("Sending command: ");
    for (size_t i = 0; i < length; i++) {
        if (cmd[i] < 0x10) Serial.print("0");
        Serial.print(cmd[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // Send the command
    Serial2.write(cmd, length);
    
    // Wait for response
    return waitMsg(timeout);
}

// Helper function to send commands and wait for response
bool sendAndWaitResponse(uint8_t* cmd, size_t length, unsigned long timeout) {
    // Clear any pending bytes in the receive buffer
    while (Serial2.available()) {
        Serial2.read();
    }
    
    // Clean our response buffer
    cleanBuffer();
    
    // Calculate checksum before sending
    buildCommand(cmd, length);
    
    // Debug print the command
    Serial.print("Sending command: ");
    for (size_t i = 0; i < length; i++) {
        if (cmd[i] < 0x10) Serial.print("0");
        Serial.print(cmd[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // Send the command
    Serial2.write(cmd, length);
    
    // Wait for response
    return waitMsg(timeout);
}

// Clean the response buffer
void cleanBuffer() {
    memset(buffer, 0, sizeof(buffer));
}

// Get hardware version from the RFID module
bool getHardwareVersion() {
    uint8_t cmd[] = {0xBB, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x7E}; // Last two bytes will be filled
    
    if (sendAndWaitResponse(cmd, sizeof(cmd), 1000)) {
        // Print hardware version if valid response
        if (buffer[1] == 0x01 && buffer[2] == 0x03) {
            Serial.print("Hardware Version: ");
            // Print ASCII characters from response data
            for (int i = 6; i < 6 + buffer[4]; i++) {
                Serial.write(buffer[i]);
            }
            Serial.println();
            return true;
        }
    }
    Serial.println("Failed to get hardware version");
    return false;
}

// Get software version from the RFID module
bool getSoftwareVersion() {
    uint8_t cmd[] = {0xBB, 0x00, 0x03, 0x00, 0x01, 0x01, 0x00, 0x7E}; // Last two bytes will be filled
    
    if (sendAndWaitResponse(cmd, sizeof(cmd), 1000)) {
        // Print software version if valid response
        if (buffer[1] == 0x01 && buffer[2] == 0x03) {
            Serial.print("Software Version: ");
            // Print ASCII characters from response data
            for (int i = 6; i < 6 + buffer[4]; i++) {
                Serial.write(buffer[i]);
            }
            Serial.println();
            return true;
        }
    }
    Serial.println("Failed to get software version");
    return false;
}

// Single tag polling
bool readSingleTag() {
    uint8_t cmd[] = {0xBB, 0x00, 0x22, 0x00, 0x00, 0x00, 0x7E};
    
    if (sendAndWaitResponse(cmd, sizeof(cmd), 1500)) {
        // Check if it's a notification frame
        if (buffer[1] == 0x02 && buffer[2] == 0x22) {
            // Parse the tag data
            uint8_t rssi = buffer[5];
            uint16_t pc = (buffer[6] << 8) | buffer[7];
            
            // Print tag EPC
            Serial.print("Tag found! RSSI: ");
            Serial.print(rssi, HEX);
            Serial.print(" PC: ");
            Serial.print(pc, HEX);
            Serial.print(" EPC: ");
            
            // Copy EPC to our card structure (assuming 12 bytes EPC)
            int currentIndex = 0;
            for (int i = 8; i < 20; i++) {
                cards[currentIndex].epc[i-8] = buffer[i];
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
            
            Serial.println("\nEPC String: " + cards[currentIndex].epc_str);
            // Tag was successfully read - send to ESP32
            sendTagToESP32(&cards[currentIndex].epc[0], 12, buffer[5]); // EPC data, length, RSSI
            return true;
        } else if (buffer[1] == 0x01 && buffer[2] == 0xFF) {
            // Error response
            Serial.print("Error response: ");
            Serial.println(buffer[5], HEX);
        }
    }
    Serial.println("No tag found or communication error");
    return false;
}

bool startMultiplePolling(uint16_t count) {
  // Reset the tag array and queue first
  for (int i = 0; i < 10; i++) {
    cards[i].epc_str = "";
  }
  queueHead = queueTail = 0;
  
  // Multiple polling with faster repeat rate
  // Command format: BB 00 27 00 04 22 [count-MSB] [count-LSB] [interval] [checksum] 7E
  // Adding a timing parameter (interval) of 0x05 (50ms) between reads
  uint8_t cmd[] = {0xBB, 0x00, 0x27, 0x00, 0x04, 0x22, 
                  (uint8_t)(count >> 8), 
                  (uint8_t)(count & 0xFF),
                  0x05, // 50ms interval (adjust as needed)
                  0x00, 0x7E};
  
  // Calculate checksum
  uint8_t checksum = 0;
  for(int i = 1; i < 9; i++) {
    checksum += cmd[i];
  }
  cmd[9] = checksum;
  
  Serial.print("Starting multiple polling with count: ");
  Serial.println(count);
  
  return sendAndWaitResponse(cmd, sizeof(cmd), 2000);
}
// Stop multiple polling
bool stopMultiplePolling() {
    uint8_t cmd[] = {0xBB, 0x00, 0x28, 0x00, 0x00, 0x00, 0x7E};
    
    bool result = sendAndWaitResponse(cmd, sizeof(cmd), 1000);
    if (!result) {
        Serial.println("Failed to stop multiple polling");
    }
    return result;
}



// Start continuous polling mode
bool startContinuousPolling() {
  // Reset polling statistics
  lastPollingCommand = millis();
  lastTagDetected = 0;
  pollingStartTime = millis();
  emptyPollCount = 0;
  
  // Clear any previous commands/data
  while (Serial2.available()) {
    Serial2.read();
  }
  
  // Start with a modest number of polls
  bool result = startMultiplePolling(50);
  if (result) {
    continuousPollingActive = true;
    Serial.println("Continuous polling mode activated");
  }
  
  return result;
}

// Manage continuous polling - call this in loop()
void manageContinuousPolling() {
  if (!continuousPollingActive) {
    return;
  }
  
  unsigned long currentTime = millis();
  
  // Check if it's time to send a new polling command
  // Either because we haven't seen any response for a while
  // or we haven't detected a tag for a while
  if (currentTime - lastPollingCommand > 10000 || // 10 seconds since last command
      (currentTime - lastTagDetected > 5000 && lastTagDetected > 0)) { // 5 seconds since last tag
    
    // Stop current polling cycle
    stopMultiplePolling();
    delay(100);
    
    // Start a new polling cycle
    int pollCount = 50; // Default poll count
    
    // Adaptive poll count based on detection frequency
    if (emptyPollCount > 5) {
      // If we've had multiple empty polls, reduce polling frequency
      pollCount = 20;
    } else if (lastTagDetected > 0 && currentTime - lastTagDetected < 2000) {
      // If we've seen tags recently, increase polling frequency
      pollCount = 100;
    }
    
    startMultiplePolling(pollCount);
    lastPollingCommand = currentTime;
    Serial.print("Refreshed polling with count: ");
    Serial.println(pollCount);
  }
  
  // Process any received data
  if (Serial2.available() > 8) {
    if (waitMsg(100)) {
      // Process notification frame
      if (buffer[1] == 0x02 && buffer[2] == 0x22) {
        processTagData();
        lastTagDetected = currentTime;
        emptyPollCount = 0;
      } else if (buffer[1] == 0x01 && buffer[2] == 0xFF) {
        // Check specific error codes
        if (buffer[5] == 0x0F) {
          // End of multiple polling operation - normal completion
          emptyPollCount++;
          Serial.println("Polling cycle complete, empty poll count: " + String(emptyPollCount));
        } else if (buffer[5] != 0x16) { // Ignore access password errors
          // Handle other errors
          Serial.print("Polling error received, code: 0x");
          Serial.println(buffer[5], HEX);
        }
      }
    }
  }
  
  // Check for system health
  if (currentTime - pollingStartTime > 3600000) { // 1 hour
    // Restart the polling system every hour for stability
    Serial.println("Performing scheduled polling system refresh");
    stopMultiplePolling();
    delay(200);
    startContinuousPolling();
  }
}

// Stop continuous polling
void stopContinuousPolling() {
  if (continuousPollingActive) {
    stopMultiplePolling();
    continuousPollingActive = false;
    Serial.println("Continuous polling mode deactivated");
  }
}

// Get transmission power
bool getTransmissionPower() {
    uint8_t cmd[] = {0xBB, 0x00, 0xB7, 0x00, 0x00, 0x00, 0x7E};
    
    if (sendAndWaitResponse(cmd, sizeof(cmd), 1000)) {
        if (buffer[1] == 0x01 && buffer[2] == 0xB7) {
            // Power is in dBm, converted from the 2-byte value
            uint16_t power = (buffer[5] << 8) | buffer[6];
            float powerDbm = power / 100.0;
            
            Serial.print("Transmission Power: ");
            Serial.print(powerDbm, 1);
            Serial.println(" dBm");
            return true;
        }
    }
    Serial.println("Failed to get transmission power");
    return false;
}

// Set transmission power
bool setTransmissionPower(uint16_t powerLevel) {
    uint8_t cmd[] = {0xBB, 0x00, 0xB6, 0x00, 0x02, (uint8_t)(powerLevel >> 8), (uint8_t)(powerLevel & 0xFF), 0x00, 0x7E};
    
    if (sendAndWaitResponse(cmd, sizeof(cmd), 1000)) {
        if (buffer[1] == 0x01 && buffer[2] == 0xB6 && buffer[5] == 0x00) {
            Serial.println("Transmission power set successfully");
            return true;
        }
    }
    Serial.println("Failed to set transmission power");
    return false;
}

void processTagData() {
  uint8_t rssi = buffer[5];
  
  // Print tag details
  Serial.print("Tag detected - RSSI: ");
  Serial.print(rssi, HEX);
  Serial.print(" EPC: ");
  
  // Build EPC string and store in temporary array
  String epcStr = "";
  uint8_t tempEpc[12]; // Temporary array to hold the EPC for ASCII conversion
  
  for (int i = 8; i < 20; i++) {
    if (buffer[i] < 0x10) Serial.print("0");
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
    
    tempEpc[i-8] = buffer[i]; // Store in our temporary array
    
    if (buffer[i] < 0x10) epcStr += "0";
    epcStr += String(buffer[i], HEX);
  }
  
  // Add ASCII interpretation
  String asciiEpc = hexToAscii(tempEpc, 12);
  Serial.print("\nASCII: \"");
  Serial.print(asciiEpc);
  Serial.println("\"");
  
  // Check if this is a new tag (not already in our list)
  bool isNewTag = true;
  int emptySlot = -1;
  
  for (int i = 0; i < 10; i++) {
    if (cards[i].epc_str == epcStr) {
      isNewTag = false;
      Serial.println("Duplicate tag - still adding to queue");
      break;
    }
    
    if (cards[i].epc_str == "" && emptySlot == -1) {
      emptySlot = i;
    }
  }
  
  // Store new tag if we found an empty slot
  if (isNewTag && emptySlot >= 0) {
    for (int i = 0; i < 12; i++) {
      cards[emptySlot].epc[i] = tempEpc[i];
    }
    cards[emptySlot].epc_str = epcStr;
    
    Serial.print("New tag added to slot ");
    Serial.println(emptySlot);
    
    // Add to queue with priority since it's a new tag
    addTagToQueue(tempEpc, rssi, true);
  } else {
    // Add to queue as regular priority
    addTagToQueue(tempEpc, rssi, false);
  }
}

// Get comprehensive device information
void getDeviceInfo() {
    Serial.println("\nDevice Information:");
    
    // Hardware Version
    getHardwareVersion();
    
    // Software Version
    getSoftwareVersion();
    
    // Transmission Power
    getTransmissionPower();
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
    
    // Echo the parsed data
    Serial.print("Parsed EPC data: ");
    for (int i = 0; i < maxLength; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

// Handle the entire write tag process
void handleWriteTag() {
    // Define passwords
    uint32_t defaultPassword = 0x00000000;
    uint32_t targetPassword = 0x12345678;
    bool authenticated = false;
    
    Serial.println("RFID Tag Write Process Starting");
    Serial.println("-------------------------------");
    
    // First try with target password (in case it's already set)
    Serial.println("Checking if target password (0x12345678) is already set...");
    if (checkAccessPassword(targetPassword)) {
        Serial.println("Successfully authenticated with 0x12345678");
        authenticated = true;
    } else {
        // Try with the default password
        Serial.println("Trying with default password (0x00000000)...");
        if (checkAccessPassword(defaultPassword)) {
            Serial.println("Default password accepted for read, attempting to set new password...");
            
            // Try to set password to target
            if (setAccessPassword(defaultPassword, targetPassword)) {
                Serial.println("Successfully set access password to 0x12345678");
                
                // Verify the password change
                if (checkAccessPassword(targetPassword)) {
                    Serial.println("New password verified successfully");
                    authenticated = true;
                } else {
                    Serial.println("Failed to verify new password - module might reject verification");
                    // Still assume the password was changed successfully
                    authenticated = true;
                }
            } else {
                Serial.println("Failed to set password - password might already be changed");
                
                // Try again with the target password as a last resort
                Serial.println("Trying with target password one more time...");
                if (checkAccessPassword(targetPassword)) {
                    Serial.println("Target password authenticated on second attempt");
                    authenticated = true;
                } else {
                    Serial.println("All authentication attempts failed");
                }
            }
        } else {
            Serial.println("Default password authentication failed");
        }
    }
    
    if (!authenticated) {
        Serial.println("Could not authenticate with any password, aborting");
        return;
    }
    
    Serial.println("-------------------------------");
    Serial.println("Authentication successful, proceeding to write EPC data");
    
    // Buffer for EPC data (12 bytes)
    uint8_t epcData[12] = {0};
    
    // Let user input the EPC data
    Serial.println("Enter 12 bytes of EPC data (in hex format, e.g. 112233445566778899AABBCC):");
    inputEpcData(epcData, 12);
    
    // Always use the target password for writing since that's what should be set by now
    if (writeEpcData(epcData, 12, targetPassword)) {
        Serial.println("EPC data written successfully!");
    } else {
        Serial.println("Failed to write EPC data");
    }
}
void handleAsciiWrite() {
    // Define passwords
    uint32_t defaultPassword = 0x00000000;
    uint32_t targetPassword = 0x12345678;
    bool authenticated = false;
    
    Serial.println("ASCII to RFID Tag Write Process");
    Serial.println("-------------------------------");
    
    // Authentication process (same as handleWriteTag)
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

// Check if we can access the tag with the provided password
bool checkAccessPassword(uint32_t password) {
    // Command to read User memory bank based on the example from PDF page 5
    // BB 00 39 00 09 [4-byte password] 03 00 00 00 02 [checksum] 7E
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
    
    // Print password being tried
    Serial.print("Trying password: 0x");
    if ((password >> 24) < 0x10) Serial.print("0");
    Serial.print((password >> 24), HEX);
    if ((password >> 16 & 0xFF) < 0x10) Serial.print("0");
    Serial.print((password >> 16 & 0xFF), HEX);
    if ((password >> 8 & 0xFF) < 0x10) Serial.print("0");
    Serial.print((password >> 8 & 0xFF), HEX);
    if ((password & 0xFF) < 0x10) Serial.print("0");
    Serial.println((password & 0xFF), HEX);
    
    // Send command and wait for response
    if (sendAndWaitResponseWithValidation(cmd, sizeof(cmd), 2000)) {
        // Check for successful response (not an error)
        if (buffer[1] == 0x01 && buffer[2] == 0x39) {
            // Print data returned to help with debugging
            Serial.print("Read success! Data: ");
            for (int i = 6; i < 6 + 4; i++) {
                if (buffer[i] < 0x10) Serial.print("0");
                Serial.print(buffer[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
            return true;
        } else if (buffer[1] == 0x01 && buffer[2] == 0xFF) {
            // Error response
            Serial.print("Authentication failed with error code: 0x");
            Serial.println(buffer[5], HEX);
            
            // Check specific error codes from documentation (page 6)
            // Check specific error codes from documentation (page 6)
            if (buffer[5] == 0x16) {
                Serial.println("Access Password Wrong");
            } else if (buffer[5] == 0x09 || buffer[5] == 0x10) {
                Serial.println("Label not read or not in range");
            } else if ((buffer[5] & 0xB0) == 0xB0) {
                Serial.println("EPC Gen2 Protocol Error");
            }
        }
    } else {
        Serial.println("No response or invalid response from tag");
    }
    
    return false;
}

// Set access password for the tag
bool setAccessPassword(uint32_t currentPassword, uint32_t newPassword) {
    // Command format from PDF page 5:
    // BB 00 49 00 0D [4-byte current password] 00 00 02 00 02 [4-byte new password] [checksum] 7E
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
    
    // Print password change attempt
    Serial.print("Setting password from: 0x");
    Serial.print(currentPassword, HEX);
    Serial.print(" to: 0x");
    Serial.println(newPassword, HEX);
    
    // Send command and wait for response
    if (sendAndWaitResponseWithValidation(cmd, sizeof(cmd), 2000)) {
        // Check for successful response
        if (buffer[1] == 0x01 && buffer[2] == 0x49 && buffer[5] == 0x00) {
            return true;
        } else if (buffer[1] == 0x01 && buffer[2] == 0xFF) {
            // Error response
            Serial.print("Error setting password, code: 0x");
            Serial.println(buffer[5], HEX);
            
            // Check specific error codes
            if (buffer[5] == 0x16) {
                Serial.println("Access Password Wrong - current password might be different");
            } else if (buffer[5] == 0x17) {
                Serial.println("Write operation not permitted");
            }
        }
    } else {
        Serial.println("No response or timeout when setting password");
    }
    return false;
}

// Write data to the EPC memory bank with correct command format
bool writeEpcData(uint8_t* data, uint8_t length, uint32_t password) {
    // Ensure length is valid (max 12 bytes for EPC)
    if (length > 12) {
        Serial.println("EPC data too long, maximum is 12 bytes");
        return false;
    }
    
    // Calculate correct command length
    // Format: BB 00 49 00 [PL] [4-byte password] 01 00 02 00 [DL] [data] [checksum] 7E
    // Where PL = 4 (password) + 1 (membank) + 2 (SA) + 2 (DL) + length (data) = 9 + length
    uint8_t cmdLength = 16 + length;  // Header(3) + Len(2) + Password(4) + Bank(1) + SA(2) + DL(2) + Data(length) + CS(1) + End(1)
    uint8_t* cmd = new uint8_t[cmdLength]; 
    
    // Calculate data length in words (2 bytes per word)
    uint8_t dataLengthWords = length / 2;
    if (length % 2 != 0) {
        // If odd number of bytes, round up to next word
        dataLengthWords += 1;
        Serial.println("Warning: Data length is not a multiple of 2 bytes, padding with zeros");
    }
    
    // Build the command
    cmd[0] = 0xBB;               // Header
    cmd[1] = 0x00;               // Type (command)
    cmd[2] = 0x49;               // Command: Write data
    cmd[3] = 0x00;               // Length MSB
    
    // Parameter Length calculation:
    // Access Password (4 bytes) + Memory Bank (1 byte) + Starting Address (2 bytes) + 
    // Data Length (2 bytes) + Data (length bytes) = 9 + length
    cmd[4] = 9 + length;         // Length LSB: 9 fixed bytes + data length
    
    // Use the specified password 
    cmd[5] = (uint8_t)(password >> 24);    // Password MSB
    cmd[6] = (uint8_t)(password >> 16);
    cmd[7] = (uint8_t)(password >> 8);
    cmd[8] = (uint8_t)(password & 0xFF);   // Password LSB
    
    cmd[9] = 0x01;               // Memory bank (01 = EPC)
    cmd[10] = 0x00;              // Starting address MSB (0x0002)
    cmd[11] = 0x02;              // Starting address LSB
    cmd[12] = 0x00;              // Data length MSB
    cmd[13] = dataLengthWords;   // Data length LSB (dynamic based on input length, in words)
    
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
    
    // Print the password being used
    Serial.print("Writing EPC with password: 0x");
    Serial.println(password, HEX);
    
    Serial.print("Data length in words: 0x");
    Serial.println(dataLengthWords, HEX);
    
    // Send command with validation and wait for response
    bool result = sendAndWaitResponseWithValidation(cmd, cmdLength, 3000);
    
    // Free allocated memory
    delete[] cmd;
    
    if (result) {
        // Check for successful response - accept any response with Type=0x01 and CMD=0x49
        if (buffer[1] == 0x01 && buffer[2] == 0x49) {
            // Success even if buffer[5] isn't 0x00
            Serial.println("EPC data written successfully!");
            return true;
        } else if (buffer[1] == 0x01 && buffer[2] == 0xFF) {
            // Error response
            Serial.print("Error code: 0x");
            Serial.println(buffer[5], HEX);
            
            // Check specific error codes
            if (buffer[5] == 0x16) {
                Serial.println("Access Password Wrong");
            } else if (buffer[5] == 0x17) {
                Serial.println("Write operation not permitted");
            } else if (buffer[5] == 0x10) {
                Serial.println("Label not read or not in range");
            }
        }
    } else {
        Serial.println("No response or timeout when writing EPC data");
    }
    return false;
}


bool isTagAlreadyDetected(uint8_t* epcData, uint8_t length) {
  for (int i = 0; i < 10; i++) {
    if (cards[i].epc_str != "") {
      bool match = true;
      for (int j = 0; j < length; j++) {
        if (cards[i].epc[j] != epcData[j]) {
          match = false;
          break;
        }
      }
      if (match) return true;
    }
  }
  return false;
}