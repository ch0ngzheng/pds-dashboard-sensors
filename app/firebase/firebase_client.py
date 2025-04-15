from firebase_admin import db
import traceback
import time

class FirebaseClient:
    @staticmethod
    def get_battery_info():
        """Get battery information from Realtime Database"""
        try:
            # Get reference to the database
            ref = db.reference('/energy_dashboard')
            battery_ref = ref.child('battery')
            data = battery_ref.get()
            
            # If we have data, make sure it has the right field names
            if data:
                # If we have 'level' but not 'percentage', map it
                if 'level' in data and 'percentage' not in data:
                    data['percentage'] = data['level']
                
                # Make sure we have all required fields
                if 'percentage' not in data:
                    data['percentage'] = 75
                if 'current_power' not in data:
                    data['current_power'] = 3.2
                if 'charging_rate' not in data:
                    data['charging_rate'] = 2.5
                if 'discharging_rate' not in data:
                    data['discharging_rate'] = 1.8
                
                return data
            else:
                # Return default data with the correct field names
                return {
                    "percentage": 75,
                    "current_power": 3.2,
                    "charging_rate": 2.5,
                    "discharging_rate": 1.8,
                    "status": "charging"
                }
        except Exception as e:
            print(f"Error getting battery info: {e}")
            traceback.print_exc()
            return {
                "percentage": 75,
                "current_power": 3.2,
                "charging_rate": 2.5,
                "discharging_rate": 1.8,
                "status": "charging",
                "error": str(e)
            }
    
    @staticmethod
    def cleanup_inactive_tags():
        """Remove inactive tags and update guest locations"""
        try:
            current_time = int(time.time())
            room_ref = db.reference('/rooms/Living Room')
            room_data = room_ref.get() or {}
            
            # Get all readers' data
            readers_data = room_data.get('readers', {})
            active_tags = set()
            inactive_tags = set()
            
            # Check each reader's in-range tags
            for reader_id, reader_data in readers_data.items():
                in_range = reader_data.get('inRange', {}) or {}
                for tag_id, last_seen in in_range.items():
                    if current_time - last_seen < 300:  # 5 minutes threshold
                        active_tags.add(tag_id)
                    else:
                        inactive_tags.add(tag_id)
                        # Remove inactive tag from reader's range
                        db.reference(f'/rooms/Living Room/readers/{reader_id}/inRange/{tag_id}').delete()
            
            # Update guest locations for inactive tags
            for tag_id in inactive_tags:
                tag_ref = db.reference(f'/tags/{tag_id}')
                tag_data = tag_ref.get()
                if tag_data and 'guestId' in tag_data:
                    guest_ref = db.reference(f'/guests/{tag_data["guestId"]}')
                    guest_ref.update({
                        'currentRoom': None,
                        'lastSeen': current_time
                    })
                # Update tag data
                tag_ref.update({
                    'currentRoom': None,
                    'lastSeen': current_time
                })
            
            return True
        except Exception as e:
            print(f"Error cleaning up inactive tags: {e}")
            return False

    @staticmethod
    def get_visitors():
        """Get visitors information including guest details"""
        try:
            # Get data from /energy_dashboard/visitors/rooms where ESP32 writes user locations
            ref = db.reference('/energy_dashboard/visitors/rooms')
            rooms_data = ref.get()
            
            if not rooms_data:
                return {"count": 0, "trend": "down", "last_activity": 0, "rooms": {}, "guests": {}}
            
            rooms = {}
            guests = {}
            total = 0
            last_activity = 0
            current_time = int(time.time())
            
            # Process each room
            for room_name, room_info in rooms_data.items():
                if not room_info or 'users' not in room_info:
                    continue
                    
                active_users = []
                # Process each user in the room
                for user_id, user_info in room_info['users'].items():
                    if not user_info:
                        continue
                        
                    last_seen = user_info.get('last_seen', 0)
                    # Only include users seen in last 5 minutes
                    if current_time - last_seen < 300:
                        user_data = {
                            'name': user_id,  # Use user_id as name for now
                            'currentRoom': room_name,
                            'lastSeen': last_seen,
                            'present': user_info.get('present', False)
                        }
                        active_users.append(user_data)
                        # Fetch additional user details from /users/<user_id>
                        try:
                            user_ref = db.reference(f'/users/{user_id}')
                            user_details = user_ref.get() or {}
                            # Merge additional fields if available
                            for key in ['firstName', 'lastName', 'dob', 'name', 'tags', 'userId', 'currentRoom', 'lastSeen']:
                                if key in user_details:
                                    user_data[key] = user_details[key]
                        except Exception as enrich_e:
                            print(f'Error enriching user {user_id}:', enrich_e)
                        guests[user_id] = user_data
                        last_activity = max(last_activity, last_seen)
                        total += 1
                
                if active_users:
                    rooms[room_name] = len(active_users)
            
            result = {
                "count": total,
                "trend": "up" if total > 0 else "down",
                "last_activity": last_activity,
                "rooms": rooms,
                "guests": guests
            }
            
            return result
        except Exception as e:
            print(f"Error getting visitors: {str(e)}")
            return {"count": 0, "trend": "down", "error": str(e)}
        except Exception as e:
            print(f"Error getting visitors info: {e}")
            traceback.print_exc()
            return {"count": 0, "trend": "down", "error": str(e)}
    
    @staticmethod
    def get_notifications():
        """Get notifications from Realtime Database"""
        try:
            # Get reference to the notifications in the database
            # Print the database structure to help debug
            root_ref = db.reference('/')
            print("\nFirebase Database Structure:")
            print(root_ref.get())
            
            # Try different paths to find notifications
            possible_paths = [
                '/energy_dashboard/notifications',
                '/notifications',
                '/alerts'
            ]
            
            notifications_data = None
            used_path = None
            
            for path in possible_paths:
                try:
                    ref = db.reference(path)
                    data = ref.get()
                    if data:
                        notifications_data = data
                        used_path = path
                        print(f"\nFound notifications at path: {path}")
                        break
                except Exception as path_error:
                    print(f"Error checking path {path}: {str(path_error)}")
            
            if notifications_data is None:
                print("\nWARNING: No notifications found in Firebase at any of the checked paths!")
                print("Please check your Firebase database structure and ensure notifications exist.")
                return []
                
            print(f"\nRaw notifications data from {used_path}:")
            print(notifications_data)
            
            # Process notifications
            processed_notifications = []
            
            # Handle different data structures: dict or list
            if isinstance(notifications_data, dict):
                for key, notification in notifications_data.items():
                    if not isinstance(notification, dict):
                        print(f"Warning: Notification {key} is not a dictionary: {notification}")
                        continue
                        
                    # Ensure each notification has the required fields
                    processed_notification = {
                        'id': str(key),
                        'title': str(notification.get('title', 'Notification')),
                        'message': str(notification.get('message', 'No details provided')),
                        'timestamp': str(notification.get('timestamp', '')),
                        'action': str(notification.get('action', '')),
                        'action_url': str(notification.get('action_url', '#')),
                        'priority': str(notification.get('priority', 'info')).lower()
                    }
                    processed_notifications.append(processed_notification)
            
            elif isinstance(notifications_data, list):
                # If it's already a list, process each notification
                for i, notification in enumerate(notifications_data):
                    if not isinstance(notification, dict):
                        print(f"Warning: Notification at index {i} is not a dictionary: {notification}")
                        continue
                        
                    processed_notification = {
                        'id': str(i),
                        'title': str(notification.get('title', 'Notification')),
                        'message': str(notification.get('message', 'No details provided')),
                        'timestamp': str(notification.get('timestamp', '')),
                        'action': str(notification.get('action', '')),
                        'action_url': str(notification.get('action_url', '#')),
                        'priority': str(notification.get('priority', 'info')).lower()
                    }
                    processed_notifications.append(processed_notification)
            else:
                print(f"Warning: Notifications data is neither a dict nor a list: {type(notifications_data)}")
            
            # Print debug information
            print(f"\nFetched {len(processed_notifications)} notifications from Firebase")
            print("Processed Notifications:")
            for notification in processed_notifications:
                print(f"  - {notification['title']}: {notification['message']}")
            
            return processed_notifications
        
        except Exception as e:
            print(f"Error fetching notifications: {str(e)}")
            traceback.print_exc()
            # Return empty list instead of default data
            return []
    
    @staticmethod
    def debug_firebase_data():
        """Debug function to inspect Firebase data structure"""
        try:
            # Get root reference
            root_ref = db.reference('/')
            data = root_ref.get()
            
            print("\n=== Firebase Data Structure ===\n")
            for key, value in data.items():
                print(f"\n{key}:")
                print("-" * 40)
                print(value)
                print("-" * 40)
            
            return data
        except Exception as e:
            print(f"Error inspecting Firebase data: {e}")
            traceback.print_exc()
            return {"error": str(e)}

    @staticmethod
    def get_grid_info():
        """Get grid information"""
        try:
            # Get reference to the database
            ref = db.reference('/energy_dashboard')
            grid_ref = ref.child('grid')
            return grid_ref.get() or {"status": "connected", "load": 80}
        except Exception as e:
            print(f"Error getting grid info: {e}")
            traceback.print_exc()
            return {"status": "connected", "load": 80, "error": str(e)}
    
    @staticmethod
    def get_floors():
        """Get floors data from Realtime Database"""
        try:
            # Print the database structure to help debug
            root_ref = db.reference('/')
            print("\nFirebase Database Structure:")
            print(root_ref.get())
            
            # Try different paths to find floors data
            possible_paths = [
                '/energy_dashboard/floors',
                '/floors',
                '/building/floors'
            ]
            
            floors_data = None
            used_path = None
            
            for path in possible_paths:
                try:
                    ref = db.reference(path)
                    data = ref.get()
                    if data:
                        floors_data = data
                        used_path = path
                        print(f"\nFound floors data at path: {path}")
                        break
                except Exception as path_error:
                    print(f"Error checking path {path}: {str(path_error)}")
            
            if floors_data is None:
                print("\nWARNING: No floors data found in Firebase at any of the checked paths!")
                print("Falling back to default floor data")
                # Return default data if nothing found in Firebase
                return [
                    {"id": "floor1", "name": "First Floor", "consumption": 120, "status": "optimal"},
                    {"id": "floor2", "name": "Second Floor", "consumption": 200, "status": "sub-optimal"},
                    {"id": "floor3", "name": "Third Floor", "consumption": 80, "status": "critical"}
                ]
                
            print(f"\nRaw floors data from {used_path}:")
            print(floors_data)
            
            # Process floors data
            processed_floors = []
            
            # Handle different data structures: dict or list
            if isinstance(floors_data, dict):
                for key, floor in floors_data.items():
                    if not isinstance(floor, dict):
                        print(f"Warning: Floor {key} is not a dictionary: {floor}")
                        continue
                        
                    # Ensure each floor has the required fields
                    processed_floor = {
                        'id': str(key),
                        'name': str(floor.get('name', f'Floor {key}')),
                        'consumption': floor.get('consumption', 0),
                        'status': str(floor.get('status', 'optimal')).lower()
                    }
                    processed_floors.append(processed_floor)
            
            elif isinstance(floors_data, list):
                # If it's already a list, process each floor
                for i, floor in enumerate(floors_data):
                    if not isinstance(floor, dict):
                        print(f"Warning: Floor at index {i} is not a dictionary: {floor}")
                        continue
                        
                    # Make sure it has an ID
                    if 'id' not in floor:
                        floor['id'] = f"floor{i+1}"
                        
                    processed_floor = {
                        'id': str(floor.get('id')),
                        'name': str(floor.get('name', f'Floor {i+1}')),
                        'consumption': floor.get('consumption', 0),
                        'status': str(floor.get('status', 'optimal')).lower()
                    }
                    processed_floors.append(processed_floor)
            
            # Print debug information
            print(f"\nProcessed {len(processed_floors)} floors from Firebase")
            for floor in processed_floors:
                print(f"  - {floor['name']} (ID: {floor['id']}): {floor['status']}, {floor['consumption']} kWh")
            
            return processed_floors
        
        except Exception as e:
            print(f"Error fetching floors: {str(e)}")
            traceback.print_exc()
            # Return default data in case of error
            return [
                {"id": "floor1", "name": "First Floor", "consumption": 120, "status": "optimal"},
                {"id": "floor2", "name": "Second Floor", "consumption": 200, "status": "sub-optimal"},
                {"id": "floor3", "name": "Third Floor", "consumption": 80, "status": "critical"}
            ]
            
    @staticmethod
    def get_floor(floor_id):
        """Get specific floor data from Realtime Database"""
        try:
            # Get all floors
            all_floors = FirebaseClient.get_floors()
            
            # Find the specific floor
            for floor in all_floors:
                if str(floor['id']) == str(floor_id):
                    # Add rooms data
                    floor['rooms'] = []
                    
                    # Try to get rooms data from Firebase
                    try:
                        # Try different paths to find rooms data
                        possible_paths = [
                            f'/energy_dashboard/floors/{floor_id}/rooms',
                            f'/floors/{floor_id}/rooms',
                            f'/building/floors/{floor_id}/rooms'
                        ]
                        
                        rooms_data = None
                        
                        for path in possible_paths:
                            try:
                                ref = db.reference(path)
                                data = ref.get()
                                if data:
                                    rooms_data = data
                                    print(f"\nFound rooms data at path: {path}")
                                    break
                            except Exception as path_error:
                                print(f"Error checking path {path}: {str(path_error)}")
                        
                        if rooms_data:
                            # Process rooms data
                            if isinstance(rooms_data, dict):
                                for room_id, room in rooms_data.items():
                                    if isinstance(room, dict):
                                        room['id'] = room_id
                                        floor['rooms'].append(room)
                            elif isinstance(rooms_data, list):
                                floor['rooms'] = rooms_data
                    except Exception as rooms_error:
                        print(f"Error fetching rooms: {str(rooms_error)}")
                    
                    # If no rooms were found, add some default rooms
                    if not floor['rooms']:
                        floor['rooms'] = [
                            {"id": "room1", "name": "Conference Room", "consumption": 45},
                            {"id": "room2", "name": "Office Space", "consumption": 65},
                            {"id": "room3", "name": "Kitchen", "consumption": 40}
                        ]
                    
                    return floor
            
            # If floor not found, return default data
            status = "optimal"
            if floor_id == "floor2":
                status = "sub-optimal"
            elif floor_id == "floor3":
                status = "critical"
                
            return {
                "id": floor_id,
                "name": f"Floor {floor_id.replace('floor', '')}",
                "consumption": 150,
                "status": status,
                "rooms": [
                    {"id": "room1", "name": "Conference Room", "consumption": 45},
                    {"id": "room2", "name": "Office Space", "consumption": 65},
                    {"id": "room3", "name": "Kitchen", "consumption": 40}
                ]
            }
            
        except Exception as e:
            print(f"Error fetching floor {floor_id}: {str(e)}")
            traceback.print_exc()
            
            # Return default data in case of error
            status = "optimal"
            if floor_id == "floor2":
                status = "sub-optimal"
            elif floor_id == "floor3":
                status = "critical"
                
            return {
                "id": floor_id,
                "name": f"Floor {floor_id.replace('floor', '')}",
                "consumption": 150,
                "status": status,
                "rooms": [
                    {"id": "room1", "name": "Conference Room", "consumption": 45},
                    {"id": "room2", "name": "Office Space", "consumption": 65},
                    {"id": "room3", "name": "Kitchen", "consumption": 40}
                ]
            }
