from flask import Flask, jsonify
from dotenv import load_dotenv
import os
import firebase_admin
from firebase_admin import credentials, db

# Load environment variables
load_dotenv()

# Create Flask app
app = Flask(__name__)

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

@app.route('/')
def home():
    return "Energy Dashboard API is running!"

@app.route('/api/test')
def test():
    try:
        # Try to access the database
        ref = db.reference('/')
        data = ref.get() or {}
        return jsonify({
            "status": "success",
            "message": "Database connection successful",
            "data": data
        })
    except Exception as e:
        return jsonify({
            "status": "error",
            "message": f"Error accessing database: {str(e)}"
        })

if __name__ == '__main__':
    app.run(debug=True)
