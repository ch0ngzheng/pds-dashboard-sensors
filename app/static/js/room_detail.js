// Room detail page specific JavaScript functionality

document.addEventListener('DOMContentLoaded', function() {
    // Get room ID from the current page URL
    const roomId = getRoomIdFromUrl();
    
    if (roomId) {
        // Set up real-time updates for room detail information
        setupRoomDetailUpdates(roomId);
        
        // Initialize consumption visualizations
        initConsumptionVisualizations();
        
        // Set up appliance toggles
        setupApplianceToggles();
    }
});

// Function to extract room ID from URL
function getRoomIdFromUrl() {
    // This is a simple implementation - adjust based on your URL structure
    const pathParts = window.location.pathname.split('/');
    const roomIdIndex = pathParts.indexOf('room') + 1;
    
    if (roomIdIndex > 0 && roomIdIndex < pathParts.length) {
        return pathParts[roomIdIndex];
    }
    
    return null;
}

// Function to periodically update room detail information
function setupRoomDetailUpdates(roomId) {
    // First update
    updateRoomDetail(roomId);
    
    // Then set interval for future updates (every 30 seconds)
    setInterval(() => updateRoomDetail(roomId), 30000);
}

// Function to fetch and update room detail information
async function updateRoomDetail(roomId) {
    try {
        const roomData = await fetchAPI(`room/${roomId}`);
        
        if (roomData.error) {
            console.error('Error fetching room data:', roomData.error);
            return;
        }
        
        // Update room overview information
        if (roomData.room) {
            updateRoomOverview(roomData.room);
        }
        
        // Update appliances if present
        if (Array.isArray(roomData.appliances)) {
            updateAppliancesInfo(roomData.appliances);
        }
        
        // Update visualizations with new data
        updateVisualizations(roomData.room);
        
    } catch (error) {
        console.error('Error updating room detail information:', error);
    }
}

// Function to update room overview section
function updateRoomOverview(room) {
    // Update consumption value
    const consumptionEl = document.querySelector('.room-consumption');
    if (consumptionEl && room.consumption !== undefined) {
        consumptionEl.textContent = `${room.consumption} kWh`;
    }
    
    // Update status
    const status = room.status || 'optimal';
    const statusBadge = document.querySelector('.status-badge');
    
    if (statusBadge) {
        // Remove old status classes
        statusBadge.classList.remove('status-optimal', 'status-sub-optimal', 'status-critical');
        // Add new status class
        statusBadge.classList.add(`status-${status}`);
        // Update text
        statusBadge.textContent = status.charAt(0).toUpperCase() + status.slice(1);
    }
    
    // Update room stats if they exist
    updateElementText('.avg-consumption', `${room.avg_consumption || '--'} kWh/day`);
    updateElementText('.peak-time', room.peak_time || '--');
    updateElementText('.efficiency-score', `${room.efficiency || '--'}/100`);
}

// Function to update appliances information
function updateAppliancesInfo(appliances) {
    appliances.forEach(appliance => {
        const applianceEl = document.querySelector(`[data-id="${appliance.id}"]`);
        if (!applianceEl) return;
        
        // Update state
        const state = appliance.state || 'off';
        applianceEl.setAttribute('data-state', state.toLowerCase());
        
        // Update state text
        const stateEl = applianceEl.querySelector('.state');
        if (stateEl) {
            stateEl.textContent = state.toUpperCase();
            stateEl.className = `state ${state.toLowerCase() === 'on' ? 'text-green-600' : 'text-gray-600'}`;
        }
    });
}

// Function to initialize consumption visualizations
function initConsumptionVisualizations() {
    // This is a placeholder - in a real app you would use a charting library
    // like Chart.js to render proper charts and visualizations
    
    // Circle chart placeholder
    const circleChartContainer = document.querySelector('.circle-chart-container');
    if (circleChartContainer) {
        circleChartContainer.innerHTML = `
            <div class="text-center p-4">
                <div class="text-lg font-medium mb-2">Energy Distribution</div>
                <div class="text-sm text-gray-500">Circle chart visualization would appear here</div>
            </div>
        `;
    }
    
    // History graph placeholder
    const historyGraphContainer = document.querySelector('.history-graph-container');
    if (historyGraphContainer) {
        historyGraphContainer.innerHTML = `
            <div class="text-center p-4">
                <div class="text-lg font-medium mb-2">Consumption History</div>
                <div class="text-sm text-gray-500">Line graph visualization would appear here</div>
            </div>
        `;
    }
}

// Function to update visualizations with new data
function updateVisualizations(roomData) {
    // This function would update the charts with new data
    // In a real app, you would use chart library methods to update the visualizations
    console.log('Updating visualizations with new data:', roomData);
}

// Function to set up appliance toggle functionality
function setupApplianceToggles() {
    document.querySelectorAll('.appliance-toggle').forEach(appliance => {
        appliance.addEventListener('click', function() {
            const id = this.getAttribute('data-id');
            const currentState = this.getAttribute('data-state');
            const newState = currentState === 'on' ? 'off' : 'on';
            
            // Call API to toggle state
            fetchAPI('toggle-appliance', {
                method: 'POST',
                body: JSON.stringify({
                    appliance_id: id,
                    state: newState
                })
            }).then(data => {
                if (!data.error) {
                    // Update UI
                    this.setAttribute('data-state', newState);
                    const stateElement = this.querySelector('span');
                    if (stateElement) {
                        stateElement.textContent = newState.toUpperCase();
                        stateElement.className = newState === 'on' ? 'text-green-600' : 'text-gray-600';
                    }
                    
                    showNotification(`Device ${newState === 'on' ? 'turned on' : 'turned off'}`, 'success');
                } else {
                    showNotification('Failed to toggle device', 'error');
                }
            }).catch(error => {
                showNotification('Failed to toggle device: ' + error, 'error');
            });
        });
    });
}