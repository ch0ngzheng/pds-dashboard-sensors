from flask import Flask, render_template_string

app = Flask(__name__, 
            template_folder='app/templates',
            static_folder='app/static')

# Sample battery data that matches what we saw in Firebase
battery_data = {
    "capacity": 13.5,
    "charging_rate": 2.5,
    "current_power": 3.2,
    "discharging_rate": 1.8,
    "health": 92,
    "life": 8,
    "percentage": 75,
    "solar_active": True
}

@app.route('/test')
def test():
    # Print what we're passing to the template
    print("\nBattery data being passed to template:")
    print(battery_data)
    
    # Simple template to test variable access
    template = """
    <!DOCTYPE html>
    <html>
    <head>
        <title>Template Test</title>
    </head>
    <body>
        <h1>Battery Data Test</h1>
        <ul>
            <li>Percentage: {{ battery.percentage|default('NOT FOUND') }}</li>
            <li>Current Power: {{ battery.current_power|default('NOT FOUND') }}</li>
            <li>Charging Rate: {{ battery.charging_rate|default('NOT FOUND') }}</li>
            <li>Discharging Rate: {{ battery.discharging_rate|default('NOT FOUND') }}</li>
        </ul>
        
        <h2>Raw Battery Object:</h2>
        <pre>{{ battery }}</pre>
    </body>
    </html>
    """
    
    return render_template_string(template, battery=battery_data)

if __name__ == '__main__':
    app.run(debug=True, port=5001)
