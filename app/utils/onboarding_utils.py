from datetime import datetime
import re

def validate_name(name):
    if not name:
        return False, "Name cannot be empty."
    if not name.isalpha():
        return False, "Name must contain only letters."
    return True, ""

def validate_date(date_str):
    try:
        dob = datetime.strptime(date_str, '%Y-%m-%d')
        if dob > datetime.now():
            return False, "Date of birth cannot be in the future.", None
        return True, "", dob
    except Exception:
        return False, "Invalid date format. Use YYYY-MM-DD.", None

def format_name(name):
    return name.strip().title()

def format_firebase_data(data):
    # Placeholder for any formatting needed for Firebase
    return data
