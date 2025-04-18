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
        new_command_ref = instance._commands_ref.push()
        command_data['id'] = new_command_ref.key
        command_data['timestamp'] = int(time.time())
        new_command_ref.set(command_data)
        return command_data['id']

    def __init__(self):
        if not FirebaseClient._initialized:
            self._db = db
            self._grid_ref = self._db.reference('grid')
            self._battery_ref = self._db.reference('battery')
            self._notifications_ref = self._db.reference('notifications')
            self._floors_ref = self._db.reference('floors')
            self._visitors_ref = self._db.reference('visitors')
            self._commands_ref = self._db.reference('commands')
            self._rooms_ref = self._db.reference('rooms')
            FirebaseClient._initialized = True
            
    @classmethod
    def initialize_rooms(cls):
        """Initialize rooms for the demo"""
        instance = cls.get_instance()
        rooms = {
            'living-room': {
                'name': 'Living Room',
                'description': 'Main living area with RFID sensor',
                'active_users': 0,
                'last_update': int(time.time()),
                'status': 'active'
            },
            'kitchen': {
                'name': 'Kitchen',
                'description': 'Cooking and dining area',
                'active_users': 0,
                'last_update': int(time.time()),
                'status': 'active'
            },
            'studio': {
                'name': 'Studio',
                'description': 'Creative workspace',
                'active_users': 0,
                'last_update': int(time.time()),
                'status': 'active'
            }
        }
        
        # Add rooms to Firebase
        for room_id, room_data in rooms.items():
            instance._rooms_ref.child(room_id).set(room_data)

    @classmethod
    def debug_firebase_data(cls):
        """Debug function to inspect Firebase data structure"""
        instance = cls.get_instance()
        return {"message": "Firebase connection active"}

    @classmethod
    def add_visitor(cls, visitor_data):
        """Add a new visitor to Firebase"""
        instance = cls.get_instance()
        new_visitor_ref = instance._visitors_ref.push()
        visitor_data['id'] = new_visitor_ref.key
        new_visitor_ref.set(visitor_data)
        return visitor_data['id']

    @classmethod
    def get_visitors(cls):
        """Get all visitors from Firebase"""
        instance = cls.get_instance()
        visitors = instance._visitors_ref.get()
        return visitors if visitors else {}

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
