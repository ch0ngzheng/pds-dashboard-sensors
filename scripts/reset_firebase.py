import firebase_admin
from firebase_admin import credentials
from firebase_admin import db
import json
import os
import sys
import datetime

# Initialize Firebase with your service account
# You'll need to have a service account key file
cred_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 
                       "app", "firebase", "serviceAccountKey.json")

if not os.path.exists(cred_path):
    print(f"Service account key not found at {cred_path}")
    print("Please ensure you have a valid serviceAccountKey.json file in the app/firebase directory")
    sys.exit(1)

cred = credentials.Certificate(cred_path)

# Get Firebase URL from environment or use default for development
firebase_url = os.environ.get('FIREBASE_DATABASE_URL', 'https://your-firebase-url.firebaseio.com/')

firebase_admin.initialize_app(cred, {
    'databaseURL': firebase_url
})

# Clean database structure - this maintains the structure but removes all test data
clean_db = {
    "commands": {},
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
    "system_status": {
        "boot_count": 0,
        "free_heap": 0,
        "ip_address": "",
        "last_update": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "start_time": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "status": "online",
        "uptime_seconds": 0,
        "wifi_ssid": "",
        "wifi_strength": 0
    },
    "tags": {},
    "visitors": {}
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

# Main menu
def main():
    print("\n===== Firebase Database Reset Tool =====")
    print("1. Backup current database")
    print("2. Reset database with clean template")
    print("3. Export clean template file (no changes to database)")
    print("4. Exit")
    
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

if __name__ == "__main__":
    main()
