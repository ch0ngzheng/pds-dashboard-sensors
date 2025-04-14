from flask import Flask
from app.config import Config
from app.firebase import init_firebase
from app.routes.main_routes import main
from app.routes.battery_routes import battery
from app.routes.api_routes import api

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
    app.run(debug=True)
