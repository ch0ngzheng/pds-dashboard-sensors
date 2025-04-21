from flask import Flask, render_template, jsonify, Blueprint
from dotenv import load_dotenv
import os
import firebase_admin
from firebase_admin import credentials, db
import traceback

# Load environment variables
load_dotenv()

# Create Flask app
app = Flask(__name__, 
            template_folder='app/templates',
            static_folder='app/static',
            static_url_path='/static')

# Initialize Firebase
cred_path = os.environ.get('FIREBASE_CREDENTIALS_PATH')
db_url = os.environ.get('FIREBASE_DATABASE_URL')

print(f"Credentials path: {cred_path}")
print(f"Database URL: {db_url}")

try:
    # Check if Firebase is already initialized
    try:
        firebase_admin.get_app()
        print("Firebase already initialized")
    except ValueError:
        # Initialize Firebase
        print("Initializing Firebase...")
        cred = credentials.Certificate(cred_path)
        firebase_admin.initialize_app(cred, {
            'databaseURL': db_url
        })
        print("Firebase initialized successfully")
except Exception as e:
    print(f"Error initializing Firebase: {e}")
    traceback.print_exc()

# Create a Firebase client class
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
                if 'status' not in data:
                    data['status'] = 'charging'
                
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
    def get_visitors():
        """Get visitors information"""
        try:
            # Get reference to the database
            ref = db.reference('/energy_dashboard')
            visitors_ref = ref.child('visitors')
            return visitors_ref.get() or {"count": 100, "trend": "up"}
        except Exception as e:
            print(f"Error getting visitors info: {e}")
            traceback.print_exc()
            return {"count": 100, "trend": "up", "error": str(e)}
    
    @staticmethod
    def get_notifications(limit=10):
        """Get notifications"""
        try:
            # Get reference to the database
            ref = db.reference('/energy_dashboard')
            notifications_ref = ref.child('notifications')
            return notifications_ref.get() or [
                {"id": 1, "message": "Battery level low", "timestamp": 1649123456},
                {"id": 2, "message": "Grid connection restored", "timestamp": 1649123457}
            ]
        except Exception as e:
            print(f"Error getting notifications: {e}")
            traceback.print_exc()
            return [{"id": 1, "message": f"Error: {str(e)}", "timestamp": 1649123456}]
    
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
                            {"id": "room1", "name": "Living Room", "consumption": 50, "status": "optimal"},
                            {"id": "room2", "name": "Kitchen", "consumption": 70, "status": "sub-optimal"},
                            {"id": "room3", "name": "Bedroom", "consumption": 30, "status": "critical"}
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
                    {"id": "room1", "name": "Living Room", "consumption": 50, "status": "optimal"},
                    {"id": "room2", "name": "Kitchen", "consumption": 70, "status": "sub-optimal"},
                    {"id": "room3", "name": "Bedroom", "consumption": 30, "status": "critical"}
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
                    {"id": "room1", "name": "Living Room", "consumption": 50, "status": "optimal"},
                    {"id": "room2", "name": "Kitchen", "consumption": 70, "status": "sub-optimal"},
                    {"id": "room3", "name": "Bedroom", "consumption": 30, "status": "critical"}
                ]
            }

# Create blueprints
main = Blueprint('main', __name__)
battery = Blueprint('battery', __name__)
api = Blueprint('api', __name__)

# Main routes
@main.route('/')
def index():
    """Landing page with battery info and visitor tracking"""
    try:
        battery_info = FirebaseClient.get_battery_info()
        visitors_info = FirebaseClient.get_visitors()
        
        # Get floor data from API
        try:
            # Make a request to our own API endpoint
            import requests
            response = requests.get('http://localhost:5004/api/floors')
            if response.status_code == 200:
                all_floors = response.json()
            else:
                # Fallback to default data if API request fails
                all_floors = [
                    {"id": "floor1", "name": "First Floor", "consumption": 120, "status": "optimal"},
                    {"id": "floor2", "name": "Second Floor", "consumption": 200, "status": "sub-optimal"},
                    {"id": "floor3", "name": "Third Floor", "consumption": 80, "status": "critical"}
                ]
        except Exception as e:
            print(f"Error fetching floor data from API: {e}")
            # Fallback to default data
            all_floors = [
                {"id": "floor1", "name": "First Floor", "consumption": 120, "status": "optimal"},
                {"id": "floor2", "name": "Second Floor", "consumption": 200, "status": "sub-optimal"},
                {"id": "floor3", "name": "Third Floor", "consumption": 80, "status": "critical"}
            ]
        
        # Count floors by status
        floor_status_counts = {
            "optimal": 0,
            "sub-optimal": 0,
            "critical": 0
        }
        
        for floor in all_floors:
            if floor["status"] in floor_status_counts:
                floor_status_counts[floor["status"]] += 1
        
        return render_template('index.html', 
                              page_title="Home",
                              battery=battery_info,
                              visitors=visitors_info,
                              floors=all_floors,
                              floor_status_counts=floor_status_counts)
    except Exception as e:
        print(f"Error in index route: {e}")
        traceback.print_exc()
        return render_template('error.html',
                              page_title="Error",
                              error_message=str(e),
                              back_url="/")

@main.route('/notifications')
def notifications():
    """Notifications/alerts page"""
    try:
        # Use hardcoded mock data instead of Firebase to ensure proper formatting
        notifications_data = [
            {
                "id": "1",
                "title": "Battery Alert",
                "message": "Battery level is low (25%)",
                "timestamp": "2025-04-13 10:30 AM",
                "action": "View Battery Status",
                "action_url": "/battery/info",
                "priority": "high"
            },
            {
                "id": "2",
                "title": "Tip",
                "message": "Sunny weather expected next week! Save electricity by using natural light.",
                "timestamp": "2025-04-13 11:45 AM",
                "action": "",
                "action_url": "",
                "priority": "medium"
            },
            {
                "id": "3",
                "title": "Energy Consumption",
                "message": "Floor 2 is showing higher than normal energy usage",
                "timestamp": "2025-04-13 12:15 PM",
                "action": "View Floor Details",
                "action_url": "/floor/floor2",
                "priority": "low"
            }
        ]
        
        return render_template('notifications.html',
                              page_title="Notifications",
                              back_url="/",
                              notifications=notifications_data)
    except Exception as e:
        print(f"Error in notifications route: {e}")
        traceback.print_exc()
        return render_template('error.html',
                              page_title="Error",
                              error_message=str(e),
                              back_url="/")

@main.route('/floors')
def floors():
    """All floors overview page"""
    try:
        # Directly fetch floor data from Firebase
        all_floors = FirebaseClient.get_floors()
        print(f"Fetched {len(all_floors)} floors for display")
    except Exception as api_error:
        print(f"Error fetching floor data: {api_error}")
        # Fallback to default data if fetching fails
        all_floors = [
            {"id": "floor1", "name": "First Floor", "consumption": 120, "status": "optimal"},
            {"id": "floor2", "name": "Second Floor", "consumption": 200, "status": "sub-optimal"},
            {"id": "floor3", "name": "Third Floor", "consumption": 80, "status": "critical"}
        ]
        
    return render_template('floors.html',
                            page_title="Floors",
                            back_url="/",
                            floors=all_floors)


@main.route('/floor/<floor_id>')
def floor_detail(floor_id):
    """Floor detail page"""
    try:
        # Fetch floor data from API
        import requests
        try:
            # Make a request to our own API endpoint
            response = requests.get(f'http://localhost:5004/api/floor/{floor_id}')
            if response.status_code == 200:
                floor_data = response.json()
                print(f"Fetched floor data for {floor_id} from API for display")
            else:
                # Fallback to default data if API request fails
                print(f"API request failed with status code: {response.status_code}")
                floor_data = {
                    "id": floor_id,
                    "name": f"Floor {floor_id.replace('floor', '')}",
                    "consumption": 150,
                    "status": "optimal",
                    "rooms": [
                        {"id": "room1", "name": "Living Room", "consumption": 50, "status": "optimal"},
                        {"id": "room2", "name": "Kitchen", "consumption": 70, "status": "sub-optimal"},
                        {"id": "room3", "name": "Bedroom", "consumption": 30, "status": "critical"}
                    ]
                }
        except Exception as api_error:
            print(f"Error fetching floor data from API: {api_error}")
            # Fallback to default data
            floor_data = {
                "id": floor_id,
                "name": f"Floor {floor_id.replace('floor', '')}",
                "consumption": 150,
                "status": "optimal",
                "rooms": [
                    {"id": "room1", "name": "Living Room", "consumption": 50, "status": "optimal"},
                    {"id": "room2", "name": "Kitchen", "consumption": 70, "status": "sub-optimal"},
                    {"id": "room3", "name": "Bedroom", "consumption": 30, "status": "critical"}
                ]
            }
        
        return render_template('floor_detail.html',
                            page_title=f"Floor {floor_id.replace('floor', '')}",
                            back_url="/floors",
                            floor=floor_data)
    except Exception as e:
        print(f"Error in floor_detail route: {e}")
        traceback.print_exc()
        return render_template('error.html',
                            page_title="Error",
                            error_message=str(e),
                            back_url="/floors")

@main.route('/rooms')
def rooms():
    """All rooms overview page"""
    try:
        # Mock data for rooms since we don't have real data
        all_rooms = [
            {"id": "room1", "name": "Living Room", "consumption": 120, "status": "active"},
            {"id": "room2", "name": "Kitchen", "consumption": 200, "status": "active"},
            {"id": "room3", "name": "Bedroom", "consumption": 80, "status": "inactive"}
        ]
        
        return render_template('rooms.html',
                              page_title="Rooms",
                              back_url="/",
                              rooms=all_rooms)
    except Exception as e:
        print(f"Error in rooms route: {e}")
        traceback.print_exc()
        return render_template('error.html',
                              page_title="Error",
                              error_message=str(e),
                              back_url="/")

@main.route('/visitors')
def visitors():
    """Visitors tracking page"""
    visitors_info = FirebaseClient.get_visitors()
    
    return render_template('visitors.html',
                          page_title="Visitors",
                          back_url="/",
                          visitors=visitors_info)

# Battery routes
@battery.route('/info')
def info():
    """Battery information page"""
    try:
        battery_data = FirebaseClient.get_battery_info()
        
        return render_template('battery.html',
                              page_title="Battery",
                              back_url="/",
                              battery=battery_data)
    except Exception as e:
        print(f"Error in battery info route: {e}")
        traceback.print_exc()
        return render_template('error.html',
                              page_title="Error",
                              error_message=str(e),
                              back_url="/")

@battery.route('/grid')
def grid():
    """Grid/Electricity information page"""
    try:
        grid_data = FirebaseClient.get_grid_info()
        
        return render_template('grid.html',
                              page_title="Grid",
                              back_url="/battery/info",
                              grid=grid_data)
    except Exception as e:
        print(f"Error in grid route: {e}")
        traceback.print_exc()
        return render_template('error.html',
                              page_title="Error",
                              error_message=str(e),
                              back_url="/battery")

# API routes
@api.route('/battery')
def get_battery():
    """API endpoint for battery information"""
    battery_data = FirebaseClient.get_battery_info()
    return jsonify(battery_data)

@api.route('/grid')
def get_grid():
    """API endpoint for grid information"""
    grid_data = FirebaseClient.get_grid_info()
    return jsonify(grid_data)

@api.route('/notifications')
def get_notifications():
    """API endpoint for notifications"""
    notifications = FirebaseClient.get_notifications()
    return jsonify(notifications)

@api.route('/floors')
def get_floors():
    """API endpoint for floors data"""
    try:
        # Get floors data from Firebase
        floors_data = FirebaseClient.get_floors()
        print(f"API returning {len(floors_data)} floors")
        return jsonify(floors_data)
    except Exception as e:
        print(f"Error in API get_floors: {str(e)}")
        # Return mock data as fallback
        all_floors = [
            {"id": "floor1", "name": "First Floor", "consumption": 120, "status": "optimal"},
            {"id": "floor2", "name": "Second Floor", "consumption": 200, "status": "sub-optimal"},
            {"id": "floor3", "name": "Third Floor", "consumption": 80, "status": "critical"}
        ]
        return jsonify(all_floors)

@api.route('/floor/<floor_id>')
def get_floor(floor_id):
    """API endpoint for specific floor data"""
    try:
        # Get floor data from Firebase
        floor_data = FirebaseClient.get_floor(floor_id)
        print(f"API returning floor data for {floor_id}")
        return jsonify(floor_data)
    except Exception as e:
        print(f"Error in API get_floor: {str(e)}")
        # Determine status based on floor_id as fallback
        status = "optimal"
        if floor_id == "floor2":
            status = "sub-optimal"
        elif floor_id == "floor3":
            status = "critical"
            
        # Mock data for the selected floor
        floor_data = {
            "id": floor_id,
            "name": f"Floor {floor_id.replace('floor', '')}",
            "consumption": 150,
            "status": status,
            "rooms": [
                {"id": "room1", "name": "Living Room", "consumption": 50, "status": "optimal"},
                {"id": "room2", "name": "Kitchen", "consumption": 70, "status": "sub-optimal"},
                {"id": "room3", "name": "Bedroom", "consumption": 30, "status": "critical"}
            ]
        }
        return jsonify(floor_data)

# Register blueprints
app.register_blueprint(main)
app.register_blueprint(battery)
app.register_blueprint(api, url_prefix='/api')

if __name__ == '__main__':
    app.run(debug=True, port=5004)
