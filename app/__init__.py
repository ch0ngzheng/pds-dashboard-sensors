from flask import Flask
from flask_session import Session
import os

def create_app():
    app = Flask(__name__)
    app.config['SECRET_KEY'] = 'dev'  # Change this in production
    app.config['SESSION_TYPE'] = 'filesystem'
    
    # Initialize session
    Session(app)

    # Register blueprints
    from app.routes.main_routes import main
    from app.routes.api_routes import api
    from app.routes.battery_routes import battery
    
    app.register_blueprint(main)
    app.register_blueprint(api, url_prefix='/api')
    app.register_blueprint(battery, url_prefix='/battery')

    return app
