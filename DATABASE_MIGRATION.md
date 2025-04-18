# Firebase Database Migration Guide

This guide will help you update your application code to work with the new optimized database structure. The new structure reduces redundancy, improves performance, and makes future maintenance easier.

## Table of Contents
- [Overview of Changes](#overview-of-changes)
- [Migration Steps](#migration-steps)
- [Code Update Guide](#code-update-guide)
  - [FirebaseClient Updates](#firebaseclient-updates)
  - [Web App Updates](#web-app-updates)
  - [ESP32 Code Updates](#esp32-code-updates)
  - [MEGA Code Updates](#mega-code-updates)
  - [UNO Code Updates](#uno-code-updates)

## Overview of Changes

### Before: Original Structure
```
/users/{user_id}
/visitors/{visitor_id}
/commands/{command_id}
/tags/{tag_id}
/events/{event_id}
/energy_dashboard/...
/system/status/...
/system_status/...
```

### After: Optimized Structure
```
/people/{person_id}
/tags/{tag_id}
/tag_readings/{location_id}/{date}/{reading_id}
/commands/{command_type}/{command_id}
/locations/{location_id}
/energy_dashboard/...
/system/current/...
/system/history/...
```

## Migration Steps

1. **Backup Your Current Database**
   ```
   python scripts/migrate_firebase.py --backup
   ```

2. **Test Migration in Dry Run Mode**
   ```
   python scripts/migrate_firebase.py --dry-run
   ```

3. **Perform Actual Migration**
   ```
   python scripts/migrate_firebase.py
   ```

4. **Update Application Code** (see below)

5. **Verify Everything Works**
   - Test all RFID tag reading/writing
   - Test visitor registration
   - Test location tracking
   - Test dashboard functionality

## Code Update Guide

### FirebaseClient Updates

In `app/firebase/firebase_client.py`, update the client initialization:

```python
def __init__(self):
    if not FirebaseClient._initialized:
        self._db = db
        self._grid_ref = self._db.reference('grid')
        self._battery_ref = self._db.reference('battery')
        self._notifications_ref = self._db.reference('notifications')
        self._floors_ref = self._db.reference('floors')
        
        # Updated references
        self._people_ref = self._db.reference('people')
        self._commands_ref = self._db.reference('commands')
        self._locations_ref = self._db.reference('locations')
        self._tags_ref = self._db.reference('tags')
        
        # Legacy references (to maintain backward compatibility)
        self._visitors_ref = self._people_ref
        
        FirebaseClient._initialized = True
```

Update methods to work with new paths:

```python
@classmethod
def add_visitor(cls, visitor_data):
    """Add a new visitor to Firebase"""
    instance = cls.get_instance()
    
    # Convert to new people format
    person_data = {
        "type": "visitor",
        "first_name": visitor_data.get("first_name", ""),
        "last_name": visitor_data.get("last_name", ""),
        "dob": visitor_data.get("dob", ""),
        "user_id": visitor_data.get("user_id", ""),
        "rfid_tags": {},
        "locations": {
            "current": "",
            "history": []
        }
    }
    
    # Add backward compatibility fields
    for key, value in visitor_data.items():
        if key not in person_data:
            person_data[key] = value
    
    # Add to people collection
    new_person_ref = instance._people_ref.push()
    person_data['id'] = new_person_ref.key
    new_person_ref.set(person_data)
    
    return person_data['id']

@classmethod
def add_command(cls, command_data):
    """Add a new command to Firebase"""
    instance = cls.get_instance()
    
    # Get command type, default to "write_rfid" for compatibility
    command_type = command_data.get("type", "write_rfid")
    
    # Create command under its type
    new_command_ref = instance._commands_ref.child(command_type).push()
    command_data['id'] = new_command_ref.key
    
    # Transform to new format
    new_command = {
        "target_id": command_data.get("visitor_id", ""),
        "status": command_data.get("status", "pending"),
        "created_at": command_data.get("timestamp", int(time.time())),
        "updated_at": command_data.get("updated_at", int(time.time())),
        "params": {
            "user_id": command_data.get("user_id", "")
        },
        "id": command_data["id"]
    }
    
    new_command_ref.set(new_command)
    return new_command['id']

@classmethod
def get_visitors(cls):
    """Get all visitors from Firebase"""
    instance = cls.get_instance()
    people = instance._people_ref.get()
    
    # Filter to only include visitors
    visitors = {}
    if people:
        for id, person in people.items():
            if person.get("type") == "visitor":
                visitors[id] = person
    
    return visitors if visitors else {}
```

### Web App Updates

In `app/routes/main_routes.py`, update references:

```python
@main_routes.route('/onboarding/register', methods=['POST'])
def onboarding_register():
    # ... existing code ...
    
    # Create visitor record in Firebase
    visitor_id = FirebaseClient.add_visitor(visitor_data)
    
    # ... existing code ...
```

For RFID tag writing, update the command creation:

```python
@main_routes.route('/onboarding/submit', methods=['POST'])
def onboarding_submit():
    # ... existing code ...
    
    # Send command to Firebase for RFID writing
    command_data = {
        'type': 'write_rfid',
        'status': 'pending',
        'timestamp': int(time.time()),
        'updated_at': int(time.time()),
        'user_id': visitor_data.get('user_id', ''),
        'visitor_id': visitor_id
    }
    
    command_id = FirebaseClient.add_command(command_data)
    
    # ... existing code ...
```

### ESP32 Code Updates

In `ESP32FIREBASE2.ino`, update your Firebase paths:

```cpp
// Updated Firebase paths
#define COMMANDS_PATH "/commands/write_rfid"
#define TAGS_PATH "/tags"
#define PEOPLE_PATH "/people"

bool processWriteCommand(FirebaseJson &commandJson) {
  String commandId, visitorId, userId;
  
  commandJson.get(jsonObj, "id");
  commandId = jsonObj.stringValue;
  
  commandJson.get(jsonObj, "target_id");  // New field name
  visitorId = jsonObj.stringValue;
  
  commandJson.get(jsonObj, "params/user_id");  // New nested field
  userId = jsonObj.stringValue;
  
  // ... existing code ...
}

void updatePendingWriteCommands() {
  if (Firebase.RTDB.getJSON(&fbdo, COMMANDS_PATH)) {
    // ... existing code ...
  }
}

bool uploadTagToFirebase(const String& epcHex, const String& epcAscii, int rssi) {
  // Upload tag data to new structure
  String tagPath = TAGS_PATH "/" + epcHex;
  
  // ... existing code to upload tag data ...
  
  // Update person location if tag is registered
  if (epcAscii.length() > 0) {
    // Find person by tag
    FirebaseJson queryJson;
    
    if (Firebase.RTDB.getJSON(&fbdo, PEOPLE_PATH)) {
      FirebaseJson* json = fbdo.jsonObjectPtr();
      if (json && json->size() > 0) {
        // Iterate through people to find matching tag
        // ... implementation ...
        
        // Update location when found
        String personPath = PEOPLE_PATH "/" + personId + "/locations/current";
        Firebase.RTDB.setString(&fbdo, personPath.c_str(), currentLocation);
      }
    }
  }
}
```

### MEGA Code Updates

For the MEGA code, the changes should be minimal since it primarily communicates with the ESP32. You mainly need to ensure the tag data format remains compatible:

```cpp
void sendTagToESP(uint8_t* epcData, int length, uint8_t rssi) {
  // ... existing code ...
  
  // Make sure the tag format remains compatible with ESP32 expectations
  // ... existing code ...
}
```

### UNO Code Updates

Similarly, the UNO code mostly interfaces with the ESP32, so minimal changes are needed:

```cpp
bool writeAsciiToTag(const char* ascii, int maxRetries) {
  // ... existing code ...
  
  // This function remains largely unchanged as it deals with
  // direct hardware control, not database structure
  // ... existing code ...
}
```

## Conclusion

This migration modernizes your database structure while maintaining compatibility with your existing code. The updated structure is more efficient, has less redundancy, and will be easier to maintain and extend going forward.

If you encounter any issues during migration, you can always restore from the backup created during the migration process.
