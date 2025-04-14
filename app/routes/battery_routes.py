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
