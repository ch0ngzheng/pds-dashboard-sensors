import firebase_admin
from firebase_admin import credentials
from app.firebase.firebase_client import FirebaseClient

# Initialize Firebase Admin SDK
cred = credentials.Certificate('firebase-credentials.json')
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://your-project.firebaseio.com'
})

# Initialize rooms
FirebaseClient.initialize_rooms()
print("Rooms initialized in Firebase")
