from firebase_admin import db
import firebase_admin
from firebase_admin import credentials
import os
from dotenv import load_dotenv
import json

# Load environment variables
load_dotenv()

# Initialize Firebase
cred_path = os.environ.get('FIREBASE_CREDENTIALS_PATH')
db_url = os.environ.get('FIREBASE_DATABASE_URL')

print(f"Credentials path: {cred_path}")
print(f"Database URL: {db_url}")

try:
    # Initialize Firebase if not already initialized
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
    
    # Get battery data
    ref = db.reference('/energy_dashboard')
    battery_ref = ref.child('battery')
    battery_data = battery_ref.get()
    
    print("\nBattery Data:")
    print(json.dumps(battery_data, indent=2))
    
    # Check for specific fields used in templates
    print("\nChecking template fields:")
    template_fields = ['percentage', 'current_power', 'charging_rate', 'discharging_rate']
    for field in template_fields:
        value = battery_data.get(field, 'NOT FOUND')
        print(f"battery.{field}: {value}")
    
    # Get visitors data
    visitors_ref = ref.child('visitors')
    visitors_data = visitors_ref.get()
    
    print("\nVisitors Data:")
    print(json.dumps(visitors_data, indent=2))
    
except Exception as e:
    print(f"Error: {e}")
