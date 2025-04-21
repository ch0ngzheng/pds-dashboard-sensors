from flask import Flask
from app.config import Config
from app.firebase import init_firebase
from app.routes.main_routes import main
from app.routes.battery_routes import battery
from app.routes.api_routes import api

import os

# Check for FIREBASE_CREDENTIALS_JSON in env (for Railway/production)
if os.environ.get("FIREBASE_CREDENTIALS_JSON"):
    firebase_json_path = "/tmp/firebase_adminsdk.json"  # or another safe path
    with open(firebase_json_path, "w") as f:
        f.write(os.environ["FIREBASE_CREDENTIALS_JSON"])
    os.environ["FIREBASE_CREDENTIALS_PATH"] = firebase_json_path
# else: FIREBASE_CREDENTIALS_PATH should already be set for local dev

def create_app(config_class=Config):
    app = Flask(__name__, 
                template_folder='app/templates',
                static_folder='app/static')
    app.config.from_object(config_class)
    
    # Initialize Firebase
    init_firebase(app)
    
    # Register blueprints
    app.register_blueprint(main)
    app.register_blueprint(battery, url_prefix='/battery')
    app.register_blueprint(api, url_prefix='/api')
    
    return app

app = create_app()

if __name__ == '__main__':
    port = int(os.environ.get('PORT', 5000))
    app.run(debug=True, host='0.0.0.0', port=port)
