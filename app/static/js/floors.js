// Floors page specific JavaScript functionality

document.addEventListener('DOMContentLoaded', function() {
    // Set up real-time updates for floors information
    setupFloorsUpdates();
});

// Function to periodically update floors information
function setupFloorsUpdates() {
    // First update
    updateFloorsInfo();
    
    // Then set interval for future updates (every 60 seconds)
    setInterval(updateFloorsInfo, 60000);
}

// Function to fetch and update floors information
async function updateFloorsInfo() {
    try {
        const floorsData = await fetchAPI('floors');
        
        if (floorsData.error) {
            console.error('Error fetching floors data:', floorsData.error);
            return;
        }
        
        // Check if we have floors data and it's an array
        if (!Array.isArray(floorsData) || floorsData.length === 0) {
            return;
        }
        
        // Update each floor card if it exists
        floorsData.forEach(floor => {
            const floorCard = document.querySelector(`[data-floor-id="${floor.id}"]`);
            if (!floorCard) return;
            
            // Update consumption value
            const consumptionEl = floorCard.querySelector('.consumption-value');
            if (consumptionEl && floor.consumption !== undefined) {
                consumptionEl.textContent = `${floor.consumption} kWh`;
            }
            
            // Update status
            const status = floor.status || 'optimal';
            const statusBadge = floorCard.querySelector('.status-badge');
            
            if (statusBadge) {
                // Remove old status classes
                statusBadge.classList.remove('status-optimal', 'status-sub-optimal', 'status-critical');
                // Add new status class
                statusBadge.classList.add(`status-${status}`);
                // Update text
                statusBadge.textContent = status.charAt(0).toUpperCase() + status.slice(1);
            }
            
            // Update border color based on status
            floorCard.classList.remove('floor-optimal', 'floor-sub-optimal', 'floor-critical');
            floorCard.classList.add(`floor-${status}`);
        });
        
    } catch (error) {
        console.error('Error updating floors information:', error);
    }
}