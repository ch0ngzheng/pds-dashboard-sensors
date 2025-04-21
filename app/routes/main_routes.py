from flask import Blueprint, render_template, jsonify, make_response
import traceback
import json
from app.firebase.firebase_client import FirebaseClient

main = Blueprint('main', __name__)

from flask import redirect, url_for

@main.route('/grid')
def grid_redirect():
    """Redirect /grid to /battery/grid for compatibility with back button and direct URL."""
    return redirect(url_for('battery.grid'))

@main.route('/debug')
def debug():
    """Debug route to show raw data"""
    try:
        battery_info = FirebaseClient.get_battery_info()
        # Use people-by-location for room summary
        location_dict = FirebaseClient.get_people_by_location()
        visitors_info = {
            'rooms': {loc: len(users) for loc, users in location_dict.items()},
            'total': sum(len(users) for users in location_dict.values())
        }
        
        debug_data = {
            "battery": battery_info,
            "visitors": visitors_info
        }
        
        return jsonify(debug_data)
    except Exception as e:
        return jsonify({"error": str(e)})

@main.route('/ajax')
def ajax_index():
    """AJAX-based landing page that loads data dynamically"""
    try:
        response = make_response(render_template('ajax_index.html', 
                              page_title="AJAX Home"))
        
        # Add no-cache headers
        response.headers['Cache-Control'] = 'no-store, no-cache, must-revalidate, max-age=0'
        response.headers['Pragma'] = 'no-cache'
        response.headers['Expires'] = '0'
        
        return response
    except Exception as e:
        print(f"Error in AJAX index route: {e}")
        traceback.print_exc()
        return jsonify({"error": str(e)})

@main.route('/simple')
def simple_index():
    """Simplified landing page with battery info"""
    try:
        battery_info = FirebaseClient.get_battery_info()
        # Use people-by-location for room summary
        location_dict = FirebaseClient.get_people_by_location()
        visitors_info = {
            'rooms': {loc: len(users) for loc, users in location_dict.items()},
            'total': sum(len(users) for users in location_dict.values())
        }
        
        # Print debug info
        print("\nSimple Index - Battery Info:")
        print(battery_info)
        
        # Ensure we have all required fields with default values
        if 'percentage' not in battery_info:
            battery_info['percentage'] = 75
        if 'current_power' not in battery_info:
            battery_info['current_power'] = 3.2
        if 'charging_rate' not in battery_info:
            battery_info['charging_rate'] = 2.5
        if 'discharging_rate' not in battery_info:
            battery_info['discharging_rate'] = 1.8
        
        response = make_response(render_template('simple_index.html', 
                              page_title="Simple Home",
                              battery=battery_info,
                              visitors=visitors_info))
        
        # Add no-cache headers
        response.headers['Cache-Control'] = 'no-store, no-cache, must-revalidate, max-age=0'
        response.headers['Pragma'] = 'no-cache'
        response.headers['Expires'] = '0'
        
        return response
    except Exception as e:
        print(f"Error in simple index route: {e}")
        traceback.print_exc()
        return jsonify({"error": str(e)})

@main.route('/data')
def get_data():
    """Direct data endpoint for AJAX requests"""
    try:
        battery_info = FirebaseClient.get_battery_info()
        
        # Ensure we have all required fields
        if 'percentage' not in battery_info:
            battery_info['percentage'] = 75
        if 'current_power' not in battery_info:
            battery_info['current_power'] = 3.2
        if 'charging_rate' not in battery_info:
            battery_info['charging_rate'] = 2.5
        if 'discharging_rate' not in battery_info:
            battery_info['discharging_rate'] = 1.8
            
        return jsonify({
            "success": True,
            "battery": battery_info
        })
    except Exception as e:
        return jsonify({
            "success": False,
            "error": str(e)
        })

@main.route('/')
def index():
    """Landing page with battery info and visitor tracking"""
    try:
        battery_info = FirebaseClient.get_battery_info()
        # Use people-by-location for room summary
        location_dict = FirebaseClient.get_people_by_location()
        visitors_info = {
            'rooms': {loc: len(users) for loc, users in location_dict.items()},
            'total': sum(len(users) for users in location_dict.values())
        }
        
        # Debug logging
        print("\nBattery Info in index route:")
        print(f"Type: {type(battery_info)}")
        print(f"Content: {battery_info}")
        print(f"Keys: {battery_info.keys() if isinstance(battery_info, dict) else 'Not a dict'}")
        
        # Add fallback values if needed
        if isinstance(battery_info, dict) and 'percentage' not in battery_info:
            print("Adding percentage to battery_info")
            battery_info['percentage'] = 75
        
        if isinstance(battery_info, dict) and 'current_power' not in battery_info:
            print("Adding current_power to battery_info")
            battery_info['current_power'] = 3.2
            
        if isinstance(battery_info, dict) and 'charging_rate' not in battery_info:
            print("Adding charging_rate to battery_info")
            battery_info['charging_rate'] = 2.5
            
        if isinstance(battery_info, dict) and 'discharging_rate' not in battery_info:
            print("Adding discharging_rate to battery_info")
            battery_info['discharging_rate'] = 1.8
        
        print("\nUpdated Battery Info:")
        print(battery_info)
        
        print("\nVisitors Info in index route:")
        print(visitors_info)
        
        # Add console logging for debugging
        print("\nRendering index.html with the following data:")
        print(f"Battery: {battery_info}")
        print(f"Visitors: {visitors_info}")
        
        # Get floor data from API
        import requests
        try:
            # Make a request to our own API endpoint
            api_response = requests.get('http://localhost:5004/api/floors')
            if api_response.status_code == 200:
                all_floors = api_response.json()
            else:
                # Fallback to default data if API request fails
                print(f"API request failed with status code: {api_response.status_code}")
                all_floors = [
                    {"id": "floor1", "name": "First Floor", "consumption": 12, "status": "optimal"},
                    {"id": "floor2", "name": "Second Floor", "consumption": 15, "status": "sub-optimal"},
                    {"id": "floor3", "name": "Third Floor", "consumption": 2.2, "status": "critical"}
                ]
        except Exception as api_error:
            print(f"Error fetching floor data from API: {api_error}")
            # Fallback to default data
            all_floors = [
                {"id": "floor1", "name": "First Floor", "consumption": 12, "status": "optimal"},
                {"id": "floor2", "name": "Second Floor", "consumption": 15, "status": "sub-optimal"},
                {"id": "floor3", "name": "Third Floor", "consumption": 2.2, "status": "critical"}
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
                
        print("\nFloor data:")
        print(f"Floors: {all_floors}")
        print(f"Status counts: {floor_status_counts}")
        
        # Force the browser to not cache this page
        from flask import make_response
        response = make_response(render_template('index.html', 
                              page_title="Home",
                              battery=battery_info,
                              visitors=visitors_info,
                              floors=all_floors,
                              floor_status_counts=floor_status_counts))
        
        # Add no-cache headers to prevent caching
        response.headers['Cache-Control'] = 'no-store, no-cache, must-revalidate, max-age=0'
        response.headers['Pragma'] = 'no-cache'
        response.headers['Expires'] = '0'
        
        return response
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
        # Fetch floor data from API
        import requests
        try:
            # Make a request to our own API endpoint
            response = requests.get('http://localhost:5004/api/floors')
            if response.status_code == 200:
                all_floors = response.json()
            else:
                # Fallback to default data if API request fails
                print(f"API request failed with status code: {response.status_code}")
                all_floors = [
                    {"id": "floor1", "name": "First Floor", "consumption": 12, "status": "optimal"},
                    {"id": "floor2", "name": "Second Floor", "consumption": 15, "status": "sub-optimal"},
                    {"id": "floor3", "name": "Third Floor", "consumption": 2.2, "status": "critical"}
                ]
        except Exception as api_error:
            print(f"Error fetching floor data from API: {api_error}")
            # Fallback to default data
            all_floors = [
                {"id": "floor1", "name": "First Floor", "consumption": 12, "status": "optimal"},
                {"id": "floor2", "name": "Second Floor", "consumption": 15, "status": "sub-optimal"},
                {"id": "floor3", "name": "Third Floor", "consumption": 2.2, "status": "critical"}
            ]
        
        return render_template('floors.html',
                              page_title="Floors",
                              back_url="/",
                              floors=all_floors)
    except Exception as e:
        print(f"Error in floors route: {e}")
        traceback.print_exc()
        return render_template('error.html',
                              page_title="Error",
                              error_message=str(e),
                              back_url="/")

@main.route('/floor/<floor_id>')
def floor_detail(floor_id):
    """Floor detail page"""
    try:
        # Fetch floor data from API
        import requests
        try:
            # Make a request to our own API endpoint
            response = requests.get(f'http://localhost:5004/api/floor/{floor_id}')
            rooms = floor_data.get('rooms', [])
            if response.status_code == 200:
                floor_data = response.json()
            else:
                # Fallback to default data if API request fails
                print(f"API request failed with status code: {response.status_code}")
                # Determine status based on floor_id
                status = "optimal"
                if floor_id == "floor2":
                    status = "sub-optimal"
                elif floor_id == "floor3":
                    status = "critical"
                
                floor_data = {
                    "id": floor_id,
                    "name": f"Floor {floor_id.replace('floor', '')}",
                    "consumption": 12,
                    "status": status,
                    "rooms": [
                        {"id": "room1", "name": "Living Room", "consumption": 2125, "status": "optimal"},
                        {"id": "room2", "name": "Kitchen", "consumption": 3000, "status": "sub-optimal"},
                        {"id": "room3", "name": "Office", "consumption": 2000, "status": "critical"}
                    ]
                }
        except Exception as api_error:
            print(f"Error fetching floor data from API: {api_error}")
            # Fallback to default data
            status = "optimal"
            if floor_id == "floor2":
                status = "sub-optimal"
            elif floor_id == "floor3":
                status = "critical"
            
            floor_data = {
                "id": floor_id,
                "name": f"Floor {floor_id.replace('floor', '')}",
                "consumption": 12,
                "status": status,
                "rooms": [
                    {"id": "room1", "name": "Living Room", "consumption": 2125, "status": "optimal"},
                    {"id": "room2", "name": "Kitchen", "consumption": 3000, "status": "sub-optimal"},
                    {"id": "room3", "name": "Office", "consumption": 2000, "status": "critical"}
                ]
            }
        
        rooms = floor_data.get('rooms', [])
        return render_template('floor_detail.html',
                            page_title=f"Floor {floor_id.replace('floor', '')}",
                            back_url="/floors",
                            floor=floor_data,
                            rooms=rooms,
                            floor_plan_image=f"images/floor_{floor_data['id']}_plan.png")
    except Exception as e:
        print(f"Error in floor_detail route: {e}")
        traceback.print_exc()
        return render_template('error.html',
                            page_title="Error",
                            error_message=str(e),
                            back_url="/floors")

@main.route('/room/<room_id>')
def room_detail(room_id):
    # Example: static room data (replace with DB or session lookup as needed)
    all_rooms = [
        {"id": "room1", "name": "Living Room", "consumption": 2125, "status": "optimal"},
        {"id": "room2", "name": "Kitchen", "consumption": 3000, "status": "sub-optimal"},
        {"id": "room3", "name": "Office", "consumption": 2000, "status": "critical"}
    ]
    room = next((r for r in all_rooms if r["id"] == room_id), None)
    if not room:
        return render_template('error.html', error_message="Room not found", back_url="/floors")
    return render_template('room_detail.html', room=room)

@main.route('/rooms')
def rooms():
    """All rooms overview page"""
    try:
        # Collect all rooms from all floors via the API
        import requests
        try:
            # First get all floors
            response = requests.get('http://localhost:5004/api/floors')
            if response.status_code == 200:
                all_floors = response.json()
                
                # Now get rooms from each floor
                all_rooms = []
                for floor in all_floors:
                    floor_response = requests.get(f'http://localhost:5004/api/floor/{floor["id"]}')
                    if floor_response.status_code == 200:
                        floor_data = floor_response.json()
                        if 'rooms' in floor_data:
                            # Add floor info to each room
                            for room in floor_data['rooms']:
                                room['floor_id'] = floor['id']
                                room['floor_name'] = floor['name']
                                all_rooms.append(room)
            else:
                # Fallback to default data if API request fails
                all_rooms = [
                    {"id": "room1", "name": "Living Room", "consumption": 2125, "status": "optimal", "floor_id": "floor1", "floor_name": "First Floor"},
                    {"id": "room2", "name": "Kitchen", "consumption": 3000, "status": "sub-optimal", "floor_id": "floor1", "floor_name": "First Floor"},
                    {"id": "room3", "name": "Office", "consumption": 2000, "status": "critical", "floor_id": "floor2", "floor_name": "Second Floor"}
                ]
        except Exception as api_error:
            print(f"Error fetching room data from API: {api_error}")
            # Fallback to default data
            all_rooms = [
                {"id": "room1", "name": "Living Room", "consumption": 2125, "status": "optimal", "floor_id": "floor1", "floor_name": "First Floor"},
                {"id": "room2", "name": "Kitchen", "consumption": 3000, "status": "sub-optimal", "floor_id": "floor1", "floor_name": "First Floor"},
                {"id": "room3", "name": "Office", "consumption": 2000, "status": "critical", "floor_id": "floor2", "floor_name": "Second Floor"}
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
    # Get people by location
    location_dict = FirebaseClient.get_people_by_location()
    # Prepare data for template: {location: {count: int, people: [str, ...]}}
    visitors_info = {
        'locations': {
            loc: {'count': len(users), 'people': users}
            for loc, users in location_dict.items()
        },
        'total': sum(len(users) for users in location_dict.values())
    }
    return render_template('visitors.html',
                          page_title="Visitors",
                          back_url="/",
                          visitors=visitors_info)
