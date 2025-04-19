from flask import Blueprint, render_template, jsonify, make_response, request, flash, redirect, url_for, session
import traceback
import json
import time
from datetime import datetime
from flask import Blueprint, render_template, request, redirect, url_for, flash, session
from app.firebase.firebase_client import FirebaseClient
from app.utils.onboarding_utils import validate_name, validate_date, format_name, format_firebase_data

main = Blueprint('main', __name__)

# --- Onboarding routes ---
@main.route('/onboarding/', methods=['GET'])
def onboarding_index():
    return render_template('onboarding/index.html')

@main.route('/onboarding/register', methods=['POST'])
def onboarding_register():
    if request.method == 'POST':
        first_name = request.form.get('firstName', '').strip()
        last_name = request.form.get('lastName', '').strip()
        date_of_birth = request.form.get('dateOfBirth', '').strip()
        
        # Validate first name
        is_valid, message = validate_name(first_name)
        if not is_valid:
            flash(f'First name error: {message}', 'error')
            return redirect(url_for('main.onboarding_index'))
            
        # Validate last name
        is_valid, message = validate_name(last_name)
        if not is_valid:
            flash(f'Last name error: {message}', 'error')
            return redirect(url_for('main.onboarding_index'))
            
        # Validate date of birth
        is_valid, message, dob_obj = validate_date(date_of_birth)
        if not is_valid:
            flash(message, 'error')
            return redirect(url_for('main.onboarding_index'))
            
        # Format names properly
        first_name_formatted = format_name(first_name)
        last_name_formatted = format_name(last_name)
        
        # Create visitor data dictionary
        visitor_data = {
            'firstName': first_name_formatted,
            'lastName': last_name_formatted,
            'dob': dob_obj.strftime('%Y-%m-%d')
        }
        
        try:
            # Store in session
            session['visitor'] = visitor_data
            
            return redirect(url_for('main.onboarding_rfid_instructions'))
            
        except ValueError as e:
            flash(f'Error processing data: {str(e)}', 'error')
            return redirect(url_for('main.onboarding_index'))
    
    return redirect(url_for('main.onboarding_index'))

@main.route('/onboarding/rfid-instructions', methods=['GET'])
def onboarding_rfid_instructions():
    # Check if visitor info exists in session
    visitor = session.get('visitor')
    if not visitor:
        flash('Please complete the registration form first.', 'error')
        return redirect(url_for('main.onboarding_index'))
    
    return render_template('onboarding/rfid_instructions.html', visitor=visitor)

@main.route('/onboarding/submit', methods=['POST'])
def onboarding_submit():
    # Get form data
    first_name = request.form.get('first_name')
    last_name = request.form.get('last_name')
    dob = request.form.get('dob')

    # Basic validation
    if not all([first_name, last_name, dob]):
        flash('Please fill in all fields', 'error')
        return redirect(url_for('main.onboarding_index'))

    try:
        # Generate user ID from name and DOB
        # Format: First letter of first name + Last name + YYMMDD
        dob_date = datetime.strptime(dob, '%Y-%m-%d')
        user_id = f"{first_name[0]}{last_name}{dob_date.strftime('%y%m%d')}"
        
        # Print debug information
        print(f"Generated user_id: {user_id}")

        # Create visitor data using updated structure
        visitor_data = {
            'first_name': first_name,
            'last_name': last_name,
            'dob': dob,
            'name': f'{first_name} {last_name}',  # For backward compatibility
            'user_id': user_id,
            'registration_date': datetime.now().strftime('%Y-%m-%d')
        }

        # Add visitor to Firebase (now goes to people collection)
        visitor_id = FirebaseClient.add_visitor(visitor_data)
        print(f"Added visitor with ID: {visitor_id}")

        # Create command data with explicit user_id
        command_data = {
            'type': 'write_rfid',
            'visitor_id': visitor_id,
            'user_id': user_id,  # Include user_id at top level for backward compatibility
            'status': 'pending',
            'timestamp': int(datetime.now().timestamp()),
            'params': {
                'user_id': user_id  # Also include in params for new structure
            }
        }
        
        # Print command data for debugging
        print(f"Command data being sent to Firebase: {command_data}")
        
        # Add RFID write command
        command_id = FirebaseClient.add_command(command_data)
        print(f"Added command with ID: {command_id}")

        # Store in session for confirmation page
        session['registered_visitor'] = {
            'id': visitor_id,
            'first_name': first_name,
            'last_name': last_name,
            'user_id': user_id,
            'command_id': command_id
        }

        # Flash success message
        flash('Registration successful! Your RFID card is being prepared.', 'success')

        # Redirect to success page
        return redirect(url_for('main.onboarding_success'))

    except Exception as e:
        print(f"Error in onboarding_submit: {str(e)}")
        traceback.print_exc()
        flash(f'Error during registration: {str(e)}', 'error')
        return redirect(url_for('main.onboarding_index'))

@main.route('/onboarding/success', methods=['GET'])
def onboarding_success():
    return render_template('onboarding/success.html')
# --- End onboarding routes ---

@main.route('/debug')
def debug():
    """Debug route to show raw data"""
    try:
        battery_info = FirebaseClient.get_battery_info()
        visitors_info = FirebaseClient.get_visitors()
        
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
        visitors_info = FirebaseClient.get_visitors()
        
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
        visitors_info = FirebaseClient.get_visitors()
        
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
                    {"id": "floor1", "name": "First Floor", "consumption": 120, "status": "optimal"},
                    {"id": "floor2", "name": "Second Floor", "consumption": 200, "status": "sub-optimal"},
                    {"id": "floor3", "name": "Third Floor", "consumption": 80, "status": "critical"}
                ]
        except Exception as api_error:
            print(f"Error fetching floor data from API: {api_error}")
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
        # Get notifications from Firebase
        notifications_data = FirebaseClient.get_notifications()
        
        # Debug logging
        print("\nNotifications data:")
        print(f"Type: {type(notifications_data)}")
        print(f"Content: {notifications_data}")
        
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
                    {"id": "floor1", "name": "First Floor", "consumption": 120, "status": "optimal"},
                    {"id": "floor2", "name": "Second Floor", "consumption": 200, "status": "sub-optimal"},
                    {"id": "floor3", "name": "Third Floor", "consumption": 80, "status": "critical"}
                ]
        except Exception as api_error:
            print(f"Error fetching floor data from API: {api_error}")
            # Fallback to default data
            all_floors = [
                {"id": "floor1", "name": "First Floor", "consumption": 120, "status": "optimal"},
                {"id": "floor2", "name": "Second Floor", "consumption": 200, "status": "sub-optimal"},
                {"id": "floor3", "name": "Third Floor", "consumption": 80, "status": "critical"}
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
                    "consumption": 150,
                    "status": status,
                    "rooms": [
                        {"id": "room1", "name": "Living Room", "consumption": 50, "status": "optimal"},
                        {"id": "room2", "name": "Kitchen", "consumption": 70, "status": "sub-optimal"},
                        {"id": "room3", "name": "Bedroom", "consumption": 30, "status": "critical"}
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
                "consumption": 150,
                "status": status,
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
                    {"id": "room1", "name": "Living Room", "consumption": 120, "status": "optimal", "floor_id": "floor1", "floor_name": "First Floor"},
                    {"id": "room2", "name": "Kitchen", "consumption": 200, "status": "sub-optimal", "floor_id": "floor1", "floor_name": "First Floor"},
                    {"id": "room3", "name": "Bedroom", "consumption": 80, "status": "critical", "floor_id": "floor2", "floor_name": "Second Floor"}
                ]
        except Exception as api_error:
            print(f"Error fetching room data from API: {api_error}")
            # Fallback to default data
            all_rooms = [
                {"id": "room1", "name": "Living Room", "consumption": 120, "status": "optimal", "floor_id": "floor1", "floor_name": "First Floor"},
                {"id": "room2", "name": "Kitchen", "consumption": 200, "status": "sub-optimal", "floor_id": "floor1", "floor_name": "First Floor"},
                {"id": "room3", "name": "Bedroom", "consumption": 80, "status": "critical", "floor_id": "floor2", "floor_name": "Second Floor"}
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
    # Using new method to get only people of type 'visitor'
    visitors_info = FirebaseClient.get_visitors()
    
    # Transform data for template if needed
    formatted_visitors = {}
    for visitor_id, visitor in visitors_info.items():
        # Add any needed transformations for backward compatibility with templates
        # For example, ensure firstName/lastName are available if templates expect them
        if 'first_name' in visitor and 'firstName' not in visitor:
            visitor['firstName'] = visitor['first_name']
        if 'last_name' in visitor and 'lastName' not in visitor:
            visitor['lastName'] = visitor['last_name']
        formatted_visitors[visitor_id] = visitor
    
    return render_template('visitors.html',
                          page_title="Visitors",
                          back_url="/",
                          visitors=formatted_visitors)
