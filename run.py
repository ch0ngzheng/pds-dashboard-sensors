from flask import Flask, session
from flask_session import Session
from app.config import Config
from app.firebase import init_firebase
from app.routes.main_routes import main
from app.routes.battery_routes import battery
from app.routes.api_routes import api

def create_app(config_class=Config):
    app = Flask(__name__, 
                template_folder='app/templates',
                static_folder='app/static')
    app.secret_key = 'your_secret_key_here_change_in_production'  # Important for sessions
    app.config['SESSION_TYPE'] = 'filesystem'
    app.config['SESSION_PERMANENT'] = False
    app.config['SESSION_USE_SIGNER'] = True
    app.config.from_object(config_class)
    
    # Initialize the Session extension
    Session(app)
    
    # Initialize Firebase
    init_firebase(app)
    
    # Register blueprints
    app.register_blueprint(main)
    app.register_blueprint(battery, url_prefix='/battery')
    app.register_blueprint(api, url_prefix='/api')
    
    return app

app = create_app()

if __name__ == '__main__':
    app.run(debug=True)
