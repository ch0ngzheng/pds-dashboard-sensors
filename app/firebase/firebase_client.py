from firebase_admin import db
import json
import time

class FirebaseClient:
    _instance = None
    _initialized = False

    @staticmethod
    def get_instance():
        if FirebaseClient._instance is None:
            FirebaseClient._instance = FirebaseClient()
        return FirebaseClient._instance

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

    def __init__(self):
        if not FirebaseClient._initialized:
            self._db = db
            # Energy dashboard references remain the same
            self._grid_ref = self._db.reference('grid')
            self._battery_ref = self._db.reference('battery')
            self._notifications_ref = self._db.reference('notifications')
            self._floors_ref = self._db.reference('floors')
            
            # New optimized structure references
            self._people_ref = self._db.reference('people')
            self._tags_ref = self._db.reference('tags')
            self._tag_readings_ref = self._db.reference('tag_readings')
            self._locations_ref = self._db.reference('locations')
            
            # Command reference is now the parent, types are children
            self._commands_ref = self._db.reference('commands')
            
            # Legacy references for backward compatibility
            self._visitors_ref = self._people_ref  # Point to new people collection
            self._rooms_ref = self._db.reference('rooms')  # Keep for legacy code
            
            FirebaseClient._initialized = True
            
    @classmethod
    def initialize_locations(cls):
        """Initialize locations for the demo"""
        instance = cls.get_instance()
        locations = {
            'living-room': {
                'name': 'Living Room',
                'description': 'Main living area with RFID sensor',
                'floor_id': 'floor1',
                'occupants': {},
                'last_update': int(time.time()),
                'status': 'active'
            },
            'kitchen': {
                'name': 'Kitchen',
                'description': 'Cooking and dining area',
                'floor_id': 'floor1',
                'occupants': {},
                'last_update': int(time.time()),
                'status': 'active'
            },
            'studio': {
                'name': 'Studio',
                'description': 'Creative workspace',
                'floor_id': 'floor2',
                'occupants': {},
                'last_update': int(time.time()),
                'status': 'active'
            }
        }
        
        # Add locations to Firebase
        for location_id, location_data in locations.items():
            instance._locations_ref.child(location_id).set(location_data)
            
        # For backward compatibility, also add to rooms
        for room_id, room_data in locations.items():
            # Convert to old room format
            old_room_data = {
                'name': room_data['name'],
                'description': room_data['description'],
                'active_users': 0,
                'last_update': room_data['last_update'],
                'status': room_data['status']
            }
            instance._rooms_ref.child(room_id).set(old_room_data)

    @classmethod
    def debug_firebase_data(cls):
        """Debug function to inspect Firebase data structure"""
        instance = cls.get_instance()
        return {"message": "Firebase connection active"}

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
    def get_visitors(cls):
        """Get all visitors from Firebase"""
        instance = cls.get_instance()
        people = instance._people_ref.get() or {}
        
        # Filter to only include visitors
        visitors = {}
        for id, person in people.items():
            if person.get("type", "") == "visitor":
                visitors[id] = person
        
        return visitors if visitors else {}
        
    @classmethod
    def get_people(cls, person_type=None):
        """Get people from Firebase, optionally filtered by type"""
        instance = cls.get_instance()
        people = instance._people_ref.get() or {}
        
        if person_type:
            filtered_people = {}
            for id, person in people.items():
                if person.get("type", "") == person_type:
                    filtered_people[id] = person
            return filtered_people
        
        return people
        
    @classmethod
    def add_tag_reading(cls, tag_id, location, rssi=0):
        """Record a tag reading at a specific location"""
        instance = cls.get_instance()
        
        # Create timestamp and date key
        timestamp = int(time.time())
        date_key = time.strftime("%Y-%m-%d", time.localtime(timestamp))
        
        # Get safe location ID (convert spaces to hyphens, lowercase)
        location_id = location.lower().replace(" ", "-")
        
        # Create reading data
        reading = {
            "tag_id": tag_id,
            "timestamp": timestamp,
            "rssi": rssi
        }
        
        # Add to tag_readings collection
        reading_ref = instance._tag_readings_ref.child(location_id).child(date_key).push()
        reading["id"] = reading_ref.key
        reading_ref.set(reading)
        
        # Update tag's last_read info
        instance._tags_ref.child(tag_id).child("last_read").update({
            "timestamp": timestamp,
            "location": location_id,
            "rssi": rssi
        })
        
        # Try to find associated person and update their location
        tag_data = instance._tags_ref.child(tag_id).get()
        if tag_data and "owner_id" in tag_data:
            owner_id = tag_data["owner_id"]
            # Update person's current location
            instance._people_ref.child(owner_id).child("locations").update({
                "current": location_id
            })
            # Add to location history (optional)
            history_ref = instance._people_ref.child(owner_id).child("locations").child("history").push()
            history_ref.set({
                "location": location_id,
                "timestamp": timestamp
            })
        
        return reading

    @staticmethod
    def get_battery_info():
        """Get battery information from Firebase"""
        try:
            ref = db.reference('/energy_dashboard')
            battery_data = ref.child('battery').get()
            if battery_data:
                return battery_data
            return {
                "level": 75,
                "status": "charging",
                "time_remaining": "4h 30m"
            }
        except Exception as e:
            print(f"Error getting battery info: {e}")
            return {
                "level": 75,
                "status": "charging",
                "time_remaining": "4h 30m"
            }

    @staticmethod
    def get_grid_info():
        """Get grid information from Firebase"""
        try:
            ref = db.reference('/energy_dashboard')
            grid_data = ref.child('grid').get()
            if grid_data:
                return grid_data
            return {
                "status": "connected",
                "power_draw": 2.5,
                "frequency": 50
            }
        except Exception as e:
            print(f"Error getting grid info: {e}")
            return {
                "status": "connected",
                "power_draw": 2.5,
                "frequency": 50
            }

    @staticmethod
    def get_notifications():
        """Get notifications from Firebase"""
        try:
            ref = db.reference('/notifications')
            notifications = ref.get()
            if notifications:
                notifications_list = []
                for notif_id, notif_data in notifications.items():
                    notif_data['id'] = notif_id
                    notifications_list.append(notif_data)
                return notifications_list
            return []
        except Exception as e:
            print(f"Error getting notifications: {e}")
            return []

    @staticmethod
    def get_floors():
        """Get all floors data from Firebase"""
        try:
            ref = db.reference('/floors')
            floors = ref.get()
            if floors:
                floors_list = []
                for floor_id, floor_data in floors.items():
                    floor_data['id'] = floor_id
                    floors_list.append(floor_data)
                return floors_list
            return []
        except Exception as e:
            print(f"Error getting floors: {e}")
            return []

    @staticmethod
    def get_floor(floor_id):
        """Get specific floor data from Firebase"""
        try:
            ref = db.reference(f'/floors/{floor_id}')
            floor = ref.get()
            if floor:
                floor['id'] = floor_id
                return floor
            return None
        except Exception as e:
            print(f"Error getting floor: {e}")
            return None
