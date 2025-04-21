import firebase_admin
from firebase_admin import credentials, db, initialize_app
import traceback
import os

firebase_app = None

def init_firebase(app):
    global firebase_app

    # try:          # for local dev, uncomment this part 
    #     # Check if Firebase is already initialized
    #     try:
    #         firebase_admin.get_app()
    #         print("Firebase already initialized")
    #     except ValueError:
    #         # Initialize Firebase
    #         print("Initializing Firebase...")
    #         cred_path = app.config['FIREBASE_CREDENTIALS']
    #         db_url = app.config['FIREBASE_DATABASE_URL']
            
    #         print(f"Credentials path: {cred_path}")
    #         print(f"Database URL: {db_url}")
            
    #         if not cred_path or not os.path.exists(cred_path):
    #             print(f"Warning: Firebase credentials file not found at {cred_path}")
    #             print("Using mock data for development")
    #             return
                
    #         if not db_url:
    #             print("Warning: Firebase database URL not set")
    #             print("Using mock data for development")
    #             return
                

    try:            # for production, uncomment this part
        cred_path = app.config.get('FIREBASE_CREDENTIALS_PATH') or os.environ.get('FIREBASE_CREDENTIALS_PATH')
        db_url = app.config.get('FIREBASE_DATABASE_URL') or os.environ.get('FIREBASE_DATABASE_URL')
        
        print(f"Credentials path: {cred_path}")
        print(f"Database URL: {db_url}")

        if not cred_path or not os.path.exists(cred_path):
            print(f"Warning: Firebase credentials file not found at {cred_path}")
            print("Using mock data for development")
            return

        if not db_url:
            print("Warning: Firebase database URL not set")
            print("Using mock data for development")
            return

        # Avoid double-initialization
        if not firebase_admin._apps:
            cred = credentials.Certificate(cred_path)
            # firebase_app = firebase_admin.initialize_app(cred, {  # for local dev, uncomment this
            
            firebase_app = firebase_admin.initialize_app(cred, {   # for production, uncomment this
                'databaseURL': db_url
            })
            print("Firebase initialized successfully")
        else:
            print("Firebase already initialized.")

    except Exception as e:
        print(f"Error initializing Firebase: {e}")
        traceback.print_exc()
        print("Using mock data for development")
