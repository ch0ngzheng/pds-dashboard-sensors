from flask import Blueprint, jsonify
from app.firebase.firebase_client import FirebaseClient

api = Blueprint('api', __name__)

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

@api.route('/visitors')
def get_visitors():
    """API endpoint for visitors information"""
    visitors_data = FirebaseClient.get_visitors()
    return jsonify(visitors_data)
