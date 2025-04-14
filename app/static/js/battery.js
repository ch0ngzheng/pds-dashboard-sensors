// Battery page specific JavaScript functionality

document.addEventListener('DOMContentLoaded', function() {
    // Set up real-time updates for battery information
    setupBatteryUpdates();
    
    // Set up chart if container exists
    const chartContainer = document.querySelector('.bg-gray-200');
    if (chartContainer) {
        initBatteryChart(chartContainer);
    }
});

// Function to periodically update battery information
function setupBatteryUpdates() {
    // First update
    updateBatteryInfo();
    
    // Then set interval for future updates (every 30 seconds)
    setInterval(updateBatteryInfo, 30000);
}

// Function to fetch and update battery information
async function updateBatteryInfo() {
    try {
        const batteryData = await fetchAPI('battery');
        
        if (batteryData.error) {
            console.error('Error fetching battery data:', batteryData.error);
            return;
        }
        
        // Update battery percentage display
        const batteryLevelEl = document.querySelector('.battery-level');
        const batteryPercentEl = document.querySelector('.battery-percent');
        
        if (batteryLevelEl && batteryPercentEl) {
            const percentage = batteryData.percentage || 0;
            
            // Update height of battery level visualization
            batteryLevelEl.style.height = `${percentage}%`;
            
            // Update percentage text
            batteryPercentEl.textContent = `${percentage}%`;
            
            // Update color based on level
            if (percentage < 20) {
                batteryLevelEl.style.backgroundColor = '#e74c3c';
            } else if (percentage < 50) {
                batteryLevelEl.style.backgroundColor = '#f39c12';
            } else {
                batteryLevelEl.style.backgroundColor = '#2ecc71';
            }
            
            // Update text color
            if (percentage > 50) {
                batteryPercentEl.classList.add('text-white');
                batteryPercentEl.classList.remove('text-gray-800');
            } else {
                batteryPercentEl.classList.add('text-gray-800');
                batteryPercentEl.classList.remove('text-white');
            }
        }
        
        // Update other battery info fields
        updateElementText('.capacity-value', batteryData.capacity);
        updateElementText('.life-value', batteryData.life);
        updateElementText('.health-value', `${batteryData.health}%`);
        updateElementText('.charging-rate-value', batteryData.charging_rate);
        updateElementText('.discharging-rate-value', batteryData.discharging_rate);
        
        // Update solar panel status
        const solarStatusEl = document.querySelector('.solar-status');
        if (solarStatusEl && batteryData.solar_active !== undefined) {
            solarStatusEl.textContent = batteryData.solar_active ? 'Active - Generating' : 'Inactive';
            solarStatusEl.className = `solar-status ${batteryData.solar_active ? 'text-green-600' : 'text-gray-600'}`;
        }
        
    } catch (error) {
        console.error('Error updating battery information:', error);
    }
}

// Helper function to update element text if element exists
function updateElementText(selector, value) {
    const element = document.querySelector(selector);
    if (element && value !== undefined) {
        element.textContent = value;
    }
}

// Function to initialize battery charging wattage chart
function initBatteryChart(container) {
    // This is a placeholder - in a real app you would use a charting library
    // like Chart.js to render a proper chart
    
    container.innerHTML = `
        <div class="text-center p-4">
            <div class="text-lg font-medium mb-2">Battery Charging Wattage</div>
            <div class="text-sm text-gray-500">Chart visualization would appear here</div>
            <div class="mt-4 text-xs text-gray-400">Data updates every 30 seconds</div>
        </div>
    `;
}