import firebase_admin
from firebase_admin import credentials
from firebase_admin import db
import json
import os
import sys
import datetime

# Initialize Firebase using environment variables
try:
    # First try with environment variables (preferred method)
    from dotenv import load_dotenv
    load_dotenv()  # Load environment variables from .env file if present
    
    # Get credentials from environment variables
    cred_path = os.environ.get('FIREBASE_CREDENTIALS_PATH')
    firebase_url = os.environ.get('FIREBASE_DATABASE_URL')
    
    if cred_path and os.path.exists(cred_path):
        # Use credentials file from environment variable
        cred = credentials.Certificate(cred_path)
        print(f"Using Firebase credentials from environment: {cred_path}")
    elif 'GOOGLE_APPLICATION_CREDENTIALS' in os.environ:
        # Use default credentials mechanism
        cred = None  # Use application default credentials
        print("Using application default credentials")
    else:
        # Fallback to local file
        local_cred_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 
                            "app", "firebase", "serviceAccountKey.json")
        
        if os.path.exists(local_cred_path):
            cred = credentials.Certificate(local_cred_path)
            print(f"Using local credentials file: {local_cred_path}")
        else:
            print("No Firebase credentials found. Please set FIREBASE_CREDENTIALS_PATH environment variable")
            print("or place serviceAccountKey.json in the app/firebase directory.")
            sys.exit(1)
    
    # Get the database URL
    if not firebase_url:
        firebase_url = 'https://pds-studio-default-rtdb.asia-southeast1.firebasedatabase.app'  # Default URL
        print(f"No database URL specified in environment. Using default: {firebase_url}")
    else:
        print(f"Using database URL from environment: {firebase_url}")
        
except Exception as e:
    print(f"Error initializing Firebase: {e}")
    sys.exit(1)

firebase_admin.initialize_app(cred, {
    'databaseURL': firebase_url
})

# Clean database structure - this maintains the structure but removes all test data
# Use the optimized structure for new clean database
clean_db = {
    "commands": {
        "write_rfid": {},
        "read_rfid": {}
    },
    "people": {},
    "tags": {},
    "tag_readings": {},
    "locations": {
        "living-room": {
            "name": "Living Room",
            "description": "Main living area with RFID sensor",
            "floor_id": "floor1",
            "occupants": {},
            "last_update": int(datetime.datetime.now().timestamp()),
            "status": "active"
        },
        "kitchen": {
            "name": "Kitchen",
            "description": "Cooking and dining area",
            "floor_id": "floor1",
            "occupants": {},
            "last_update": int(datetime.datetime.now().timestamp()),
            "status": "active"
        },
        "studio": {
            "name": "Studio",
            "description": "Creative workspace",
            "floor_id": "floor2",
            "occupants": {},
            "last_update": int(datetime.datetime.now().timestamp()),
            "status": "active"
        }
    },
    "energy_dashboard": {
        "appliances": {
            "app1": {
                "consumption": 0.12,
                "name": "TV",
                "room_id": "room1",
                "state": "on",
                "type": "electronics"
            }
        },
        "battery": {
            "capacity": 20,
            "charging_rate": 22.5,
            "current_power": -6.7,
            "discharging_rate": 29.2,
            "health": 92,
            "life": 8,
            "percentage": 75,
            "solar_active": True
        },
        "floors": {
            "floor1": {
                "consumption": 12,
                "name": "1st Floor",
                "status": "optimal"
            },
            "floor2": {
                "consumption": 15,
                "name": "2nd Floor",
                "status": "sub-optimal"
            },
            "floor3": {
                "consumption": 2.2,
                "name": "3rd Floor",
                "status": "critical"
            }
        },
        "grid": {
            "connected": True,
            "purchased": 120,
            "revenue": 25.5,
            "sold": 45
        },
        "notifications": {
            "notif1": {
                "action": "Check Battery",
                "action_url": "/battery",
                "message": "Battery level below 20%",
                "priority": "high",
                "timestamp": "2025-04-09T14:30:00Z",
                "title": "Battery Low"
            },
            "notif2": {
                "action": "Tip",
                "message": "It will be sunny the next week! Save electricity by utilizing natural light!",
                "timestamp": "2025-04-11T14:30:00Z",
                "title": "Save Energy"
            }
        },
        "rooms": {
            "room1": {
                "avg_consumption": 12.3,
                "consumption": 1.5,
                "efficiency": 78,
                "floor_id": "floor1",
                "name": "Living Room"
            }
        },
        "visitors": {
            "rooms": {},
            "total": 0
        }
    },
    "events": {},
    "system": {
        "status": {
            "free_heap": 0,
            "ip_address": "",
            "last_update": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "status": "online",
            "uptime_seconds": 0,
            "wifi_ssid": "",
            "wifi_strength": 0
        }
    },
    "system": {
        "current": {
            "boot_count": 0,
            "free_heap": 0,
            "ip_address": "",
            "last_update": int(datetime.datetime.now().timestamp()),
            "start_time": int(datetime.datetime.now().timestamp()),
            "status": "online",
            "uptime_seconds": 0,
            "wifi_ssid": "",
            "wifi_strength": 0
        },
        "history": {}
    },
    "rooms": {}  # Keep for backward compatibility
}

# Option to save current database as backup
def backup_database():
    print("Backing up current database...")
    root_ref = db.reference('/')
    current_data = root_ref.get()
    
    with open('firebase_backup.json', 'w') as f:
        json.dump(current_data, f, indent=2)
    
    print(f"Backup saved to firebase_backup.json")

# Reset database with clean template
def reset_database():
    print("Resetting database with clean template...")
    root_ref = db.reference('/')
    root_ref.set(clean_db)
    print("Database reset complete!")

# Export clean structure to file
def export_clean_template():
    print("Exporting clean database template...")
    with open('clean_firebase_template.json', 'w') as f:
        json.dump(clean_db, f, indent=2)
    print(f"Clean template saved to clean_firebase_template.json")
    
    # Export optimized template separately (for documentation)
    with open('optimized_firebase_template.json', 'w') as f:
        clean_optimized = {
            "commands": clean_db["commands"],
            "people": clean_db["people"],
            "tags": clean_db["tags"],
            "tag_readings": clean_db["tag_readings"],
            "locations": clean_db["locations"],
            "system": clean_db["system"]
        }
        json.dump(clean_optimized, f, indent=2)
    print(f"Optimized template saved to optimized_firebase_template.json")

# Main menu
def main():
    # Check if command line arguments were provided
    if len(sys.argv) > 1:
        choice = sys.argv[1]
        if choice == '1' or choice == 'backup':
            backup_database()
        elif choice == '2' or choice == 'reset':
            # Automatically confirm when using CLI mode
            print("Performing database reset...")
            backup_database()  # Always backup before reset
            reset_database()
        elif choice == '3' or choice == 'export':
            export_clean_template()
        else:
            print("Invalid option. Valid options are:")
            print("1 or backup: Backup current database")
            print("2 or reset: Reset database with clean template")
            print("3 or export: Export clean template file (no changes to database)")
        return
    
    # Interactive mode
    print("\n===== Firebase Database Reset Tool =====")
    print("1. Backup current database")
    print("2. Reset database with clean template")
    print("3. Export clean template file (no changes to database)")
    print("4. Exit")
    
    try:
        choice = input("\nSelect an option (1-4): ")
        
        if choice == '1':
            backup_database()
            main()
        elif choice == '2':
            confirm = input("Are you sure you want to reset the database? This cannot be undone! (y/n): ")
            if confirm.lower() == 'y':
                backup_database()  # Always backup before reset
                reset_database()
            main()
        elif choice == '3':
            export_clean_template()
            main()
        elif choice == '4':
            print("Exiting...")
            sys.exit(0)
        else:
            print("Invalid option, please try again.")
            main()
    except EOFError:
        print("\nNo input detected. Try using command line arguments:")
        print("python reset_firebase.py 2" + " (to reset database)")
        sys.exit(1)

if __name__ == "__main__":
    main()
