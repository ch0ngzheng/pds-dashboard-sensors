from flask import Blueprint, render_template
import traceback
from app.firebase.firebase_client import FirebaseClient

battery = Blueprint('battery', __name__)

@battery.route('/info')
def info():
    """Battery information page"""
    try:
        battery_data = FirebaseClient.get_battery_info()
        
        return render_template('battery.html',
                              page_title="Battery",
                              back_url="/",
                              battery=battery_data)
    except Exception as e:
        print(f"Error in battery info route: {e}")
        traceback.print_exc()
        return render_template('error.html',
                              page_title="Error",
                              error_message=str(e),
                              back_url="/")

@battery.route('/grid')
def grid():
    """Grid/Electricity information page"""
    try:
        grid_data = FirebaseClient.get_grid_info()
        
        # Define backup values
        backup_grid_data = {
            "purchased": 1200,   # kWh
            "sold": 800,         # kWh
            "revenue": 350.75,   # $
            "connected": True
        }
        
        # Use backup values if grid_data is None or missing keys
        if not grid_data:
            grid_data = backup_grid_data
        else:
            # Fill in any missing keys from backup
            for key, value in backup_grid_data.items():
                if key not in grid_data:
                    grid_data[key] = value

        return render_template('grid.html',
                              page_title="Grid",
                              back_url="/battery/info",
                              grid=grid_data)
    except Exception as e:
        print(f"Error in grid route: {e}")
        traceback.print_exc()
        return render_template('error.html',
                              page_title="Error",
                              error_message=str(e),
                              back_url="/battery/info")
