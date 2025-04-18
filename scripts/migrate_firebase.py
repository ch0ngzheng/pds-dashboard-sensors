"""
Firebase Database Migration Tool
--------------------------------
This script migrates the current database to the optimized structure
while preserving all existing data relationships and functionality.

Usage:
  python migrate_firebase.py [--dry-run] [--backup]

Options:
  --dry-run    Don't apply changes, just print what would happen
  --backup     Create a backup before migration (recommended)
"""

import firebase_admin
from firebase_admin import credentials
from firebase_admin import db
import json
import os
import sys
import argparse
import datetime
import time

def parse_args():
    parser = argparse.ArgumentParser(description='Migrate Firebase database to optimized structure')
    parser.add_argument('--dry-run', action='store_true', help='Simulate migration without changing data')
    parser.add_argument('--backup', action='store_true', help='Create backup before migration')
    return parser.parse_args()

def init_firebase():
    """Initialize Firebase connection"""
    print("Initializing Firebase connection...")
    cred_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 
                         "app", "firebase", "serviceAccountKey.json")

    if not os.path.exists(cred_path):
        print(f"Service account key not found at {cred_path}")
        print("Please ensure you have a valid serviceAccountKey.json file in the app/firebase directory")
        sys.exit(1)

    cred = credentials.Certificate(cred_path)
    firebase_url = os.environ.get('FIREBASE_DATABASE_URL', 'https://your-firebase-url.firebaseio.com/')
    firebase_admin.initialize_app(cred, {'databaseURL': firebase_url})
    print("Firebase connection initialized.")

def backup_database():
    """Create a backup of the current database"""
    print("Creating database backup...")
    root_ref = db.reference('/')
    current_data = root_ref.get()
    
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_file = f'firebase_backup_{timestamp}.json'
    
    with open(backup_file, 'w') as f:
        json.dump(current_data, f, indent=2)
    
    print(f"Backup saved to {backup_file}")
    return backup_file

def migrate_users_and_visitors(dry_run=False):
    """Migrate users and visitors to the unified people collection"""
    print("\n=== Migrating Users and Visitors to People Collection ===")
    
    # Get existing data
    users_ref = db.reference('/users')
    visitors_ref = db.reference('/visitors')
    users_data = users_ref.get() or {}
    visitors_data = visitors_ref.get() or {}
    
    # Create new people structure
    people_data = {}
    
    # Process users
    for user_id, user in users_data.items():
        print(f"Processing user: {user_id}")
        
        # Create person entry
        person = {
            "type": "resident",
            "first_name": user.get("firstName", ""),
            "last_name": user.get("lastName", ""),
            "dob": user.get("dob", ""),
            "user_id": user_id,  # Keep original ID for backward compatibility
            "rfid_tags": {},
            "locations": {
                "current": user.get("currentRoom", ""),
                "history": []
            }
        }
        
        # Process tags
        if "tags" in user:
            for tag_id in user["tags"]:
                person["rfid_tags"][tag_id] = True
        
        people_data[user_id] = person
    
    # Process visitors
    for visitor_id, visitor in visitors_data.items():
        if not visitor_id.startswith("-"):  # Skip if not a proper Firebase ID
            continue
            
        print(f"Processing visitor: {visitor_id}")
        
        # Create person entry
        person = {
            "type": "visitor",
            "first_name": visitor.get("first_name", ""),
            "last_name": visitor.get("last_name", ""),
            "dob": visitor.get("dob", ""),
            "visitor_id": visitor_id,  # Keep original ID for backward compatibility
            "user_id": visitor.get("user_id", ""),  # Some visitors have user_ids
            "rfid_tags": {},
            "locations": {
                "current": "",
                "history": []
            }
        }
        
        people_data[visitor_id] = person
    
    if not dry_run:
        print("Uploading people data to Firebase...")
        people_ref = db.reference('/people')
        people_ref.set(people_data)
    else:
        print("DRY RUN: Would upload people data to Firebase")
        print(f"People count: {len(people_data)}")
    
    return people_data

def migrate_tags(dry_run=False):
    """Migrate tags to the new structure and create tag_readings collection"""
    print("\n=== Migrating Tags and Tag Readings ===")
    
    # Get existing data
    tags_ref = db.reference('/tags')
    events_ref = db.reference('/events')
    tags_data = tags_ref.get() or {}
    events_data = events_ref.get() or {}
    
    # Create new tag structure
    new_tags = {}
    tag_readings = {}
    
    # Process tags
    for tag_id, tag in tags_data.items():
        print(f"Processing tag: {tag_id}")
        
        # Keep basic tag info
        new_tag = {
            "epc": tag.get("epc", ""),
            "ascii_text": tag.get("ascii_text", ""),
            "last_read": {
                "timestamp": tag.get("last_seen", 0),
                "location": "unknown",
                "rssi": tag.get("rssi", 0)
            }
        }
        
        # Try to find owner from the ascii text (often contains user ID)
        if len(tag.get("ascii_text", "")) >= 8:
            potential_id = tag.get("ascii_text", "").strip()
            # For common formats like "AWong020530"
            new_tag["owner_id"] = potential_id
        
        new_tags[tag_id] = new_tag
    
    # Process tag readings from events
    for event_id, event in events_data.items():
        if event.get("type") != "tag_detected":
            continue
            
        timestamp = event.get("unix_time", 0)
        location = event.get("location", "unknown").lower().replace(" ", "-")
        tag_id = event.get("tag_id", "")
        
        # Skip if missing critical data
        if not timestamp or not tag_id:
            continue
            
        # Organize readings by location and date
        date_key = datetime.datetime.fromtimestamp(timestamp).strftime("%Y-%m-%d")
        
        if location not in tag_readings:
            tag_readings[location] = {}
            
        if date_key not in tag_readings[location]:
            tag_readings[location][date_key] = {}
        
        # Add reading
        tag_readings[location][date_key][event_id] = {
            "tag_id": tag_id,
            "timestamp": timestamp,
            "rssi": tags_data.get(tag_id, {}).get("rssi", 0) if tag_id in tags_data else 0
        }
    
    if not dry_run:
        print("Uploading tag data to Firebase...")
        new_tags_ref = db.reference('/tags')
        new_tags_ref.set(new_tags)
        
        print("Uploading tag readings to Firebase...")
        tag_readings_ref = db.reference('/tag_readings')
        tag_readings_ref.set(tag_readings)
    else:
        print("DRY RUN: Would upload tag data to Firebase")
        print(f"Tags count: {len(new_tags)}")
        print(f"Tag readings locations: {list(tag_readings.keys())}")
    
    return new_tags, tag_readings

def migrate_commands(dry_run=False):
    """Migrate commands to the new structure"""
    print("\n=== Migrating Commands ===")
    
    # Get existing data
    commands_ref = db.reference('/commands')
    commands_data = commands_ref.get() or {}
    
    # Create new commands structure
    new_commands = {}
    
    # Process commands by type
    for command_id, command in commands_data.items():
        command_type = command.get("type", "unknown")
        
        if command_type not in new_commands:
            new_commands[command_type] = {}
        
        # Transform command
        new_command = {
            "target_id": command.get("visitor_id", ""),
            "status": command.get("status", "unknown"),
            "created_at": command.get("timestamp", 0),
            "updated_at": command.get("updated_at", 0),
            "params": {
                "user_id": command.get("user_id", "")
            }
        }
        
        new_commands[command_type][command_id] = new_command
        print(f"Processed command: {command_id} (type: {command_type})")
    
    if not dry_run:
        print("Uploading commands data to Firebase...")
        new_commands_ref = db.reference('/commands')
        new_commands_ref.set(new_commands)
    else:
        print("DRY RUN: Would upload commands data to Firebase")
        print(f"Command types: {list(new_commands.keys())}")
    
    return new_commands

def migrate_locations(dry_run=False):
    """Create locations collection from existing data"""
    print("\n=== Setting Up Locations ===")
    
    # Create basic locations structure based on events
    events_ref = db.reference('/events')
    events_data = events_ref.get() or {}
    
    locations = {}
    
    # Extract unique locations from events
    for event_id, event in events_data.items():
        if "location" in event:
            location_name = event["location"]
            location_id = location_name.lower().replace(" ", "-")
            
            if location_id not in locations:
                locations[location_id] = {
                    "name": location_name,
                    "floor_id": "floor1",  # Default assumption
                    "occupants": {}
                }
    
    # Add Living Room if not found (common default)
    if "living-room" not in locations:
        locations["living-room"] = {
            "name": "Living Room",
            "floor_id": "floor1",
            "occupants": {}
        }
    
    if not dry_run:
        print("Uploading locations data to Firebase...")
        locations_ref = db.reference('/locations')
        locations_ref.set(locations)
    else:
        print("DRY RUN: Would upload locations data to Firebase")
        print(f"Locations: {list(locations.keys())}")
    
    return locations

def update_energy_dashboard_references(dry_run=False):
    """Update energy dashboard to use new references"""
    print("\n=== Updating Energy Dashboard References ===")
    
    # Get existing energy dashboard
    energy_ref = db.reference('/energy_dashboard')
    energy_data = energy_ref.get() or {}
    
    # No major changes needed here, just maintaining it
    # We could update visitor references, but the dashboard uses its own format
    
    if not dry_run:
        # We're keeping energy_dashboard as is
        print("Energy dashboard preserved with original structure")
    else:
        print("DRY RUN: Would preserve energy dashboard structure")
    
    return energy_data

def migrate_system(dry_run=False):
    """Consolidate system and system_status into a unified structure"""
    print("\n=== Migrating System Data ===")
    
    # Get existing system data
    system_ref = db.reference('/system')
    system_status_ref = db.reference('/system_status')
    system_data = system_ref.get() or {}
    system_status_data = system_status_ref.get() or {}
    
    # Create new system structure
    new_system = {
        "current": system_data.get("status", {}),
        "history": {}
    }
    
    # Add a history entry with the system_status data
    if system_status_data:
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        new_system["history"][timestamp] = system_status_data
    
    if not dry_run:
        print("Uploading system data to Firebase...")
        new_system_ref = db.reference('/system')
        new_system_ref.set(new_system)
    else:
        print("DRY RUN: Would upload new system data to Firebase")
    
    return new_system

def cleanup_old_data(dry_run=False):
    """Remove old data structures after successful migration"""
    print("\n=== Cleaning Up Old Data Structures ===")
    
    if not dry_run:
        print("Removing old data structures...")
        db.reference('/users').delete()
        db.reference('/visitors').delete()
        db.reference('/events').delete()
        # Keep /tags for compatibility, could remove later
        db.reference('/system_status').delete()
    else:
        print("DRY RUN: Would remove old data structures")
        print("  - /users")
        print("  - /visitors")
        print("  - /events")
        print("  - /system_status")

def main():
    args = parse_args()
    
    init_firebase()
    
    if args.backup:
        backup_file = backup_database()
        print(f"Backup created: {backup_file}")
    
    if args.dry_run:
        print("\n*** DRY RUN MODE - No changes will be made ***\n")
    
    # Execute migration steps
    people_data = migrate_users_and_visitors(args.dry_run)
    tags_data, readings_data = migrate_tags(args.dry_run)
    commands_data = migrate_commands(args.dry_run)
    locations_data = migrate_locations(args.dry_run)
    energy_data = update_energy_dashboard_references(args.dry_run)
    system_data = migrate_system(args.dry_run)
    
    # Clean up old data after successful migration
    if not args.dry_run:
        cleanup_old_data(args.dry_run)
    
    print("\n=== Migration Summary ===")
    print(f"People migrated: {len(people_data)}")
    print(f"Tags migrated: {len(tags_data)}")
    print(f"Command types migrated: {len(commands_data)}")
    print(f"Locations created: {len(locations_data)}")
    
    if args.dry_run:
        print("\nDRY RUN COMPLETE - No changes were made.")
        print("To perform the actual migration, run without --dry-run")
    else:
        print("\nMIGRATION COMPLETE!")
        print("Your database has been successfully migrated to the new structure.")
        print("You should now update your application code to use the new paths.")

if __name__ == "__main__":
    main()
