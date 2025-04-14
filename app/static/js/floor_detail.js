// Floor detail page specific JavaScript functionality

document.addEventListener('DOMContentLoaded', function() {
    // Get floor ID from the current page URL
    const floorId = getFloorIdFromUrl();
    
    if (floorId) {
        // Set up real-time updates for floor detail information
        setupFloorDetailUpdates(floorId);
        
        // Set up floor plan visualization if container exists
        const floorPlanContainer = document.querySelector('.floor-plan-container');
        if (floorPlanContainer) {
            initFloorPlan(floorPlanContainer, floorId);
        }
    }
});

// Function to extract floor ID from URL
function getFloorIdFromUrl() {
    // This is a simple implementation - adjust based on your URL structure
    const pathParts = window.location.pathname.split('/');
    const floorIdIndex = pathParts.indexOf('floor') + 1;
    
    if (floorIdIndex > 0 && floorIdIndex < pathParts.length) {
        return pathParts[floorIdIndex];
    }
    
    return null;
}

// Function to periodically update floor detail information
function setupFloorDetailUpdates(floorId) {
    // First update
    updateFloorDetail(floorId);
    
    // Then set interval for future updates (every 30 seconds)
    setInterval(() => updateFloorDetail(floorId), 30000);
}

// Function to fetch and update floor detail information
async function updateFloorDetail(floorId) {
    try {
        const floorData = await fetchAPI(`floor/${floorId}`);
        
        if (floorData.error) {
            console.error('Error fetching floor data:', floorData.error);
            return;
        }
        
        // Update floor overview information
        const consumptionEl = document.querySelector('.floor-consumption');
        if (consumptionEl && floorData.floor && floorData.floor.consumption !== undefined) {
            consumptionEl.textContent = `${floorData.floor.consumption} kWh`;
        }
        
        // Update status
        if (floorData.floor) {
            const status = floorData.floor.status || 'optimal';
            const statusBadge = document.querySelector('.status-badge');
            
            if (statusBadge) {
                // Remove old status classes
                statusBadge.classList.remove('status-optimal', 'status-sub-optimal', 'status-critical');
                // Add new status class
                statusBadge.classList.add(`status-${status}`);
                // Update text
                statusBadge.textContent = status.charAt(0).toUpperCase() + status.slice(1);
            }
        }
        
        // Update rooms if present
        if (Array.isArray(floorData.rooms)) {
            updateRoomsInfo(floorData.rooms);
        }
        
    } catch (error) {
        console.error('Error updating floor detail information:', error);
    }
}

// Function to update rooms information
function updateRoomsInfo(rooms) {
    rooms.forEach(room => {
        const roomCard = document.querySelector(`[data-room-id="${room.id}"]`);
        if (!roomCard) return;
        
        // Update consumption value
        const consumptionEl = roomCard.querySelector('.consumption-value');
        if (consumptionEl && room.consumption !== undefined) {
            consumptionEl.textContent = `${room.consumption} kWh`;
        }
        
        // Update status
        const status = room.status || 'optimal';
        const statusBadge = roomCard.querySelector('.status-badge');
        
        if (statusBadge) {
            // Remove old status classes
            statusBadge.classList.remove('status-optimal', 'status-sub-optimal', 'status-critical');
            // Add new status class
            statusBadge.classList.add(`status-${status}`);
            // Update text
            statusBadge.textContent = status.charAt(0).toUpperCase() + status.slice(1);
        }
        
        // Update border color based on status
        roomCard.classList.remove('floor-optimal', 'floor-sub-optimal', 'floor-critical');
        roomCard.classList.add(`floor-${status}`);
    });
}

// Function to initialize floor plan visualization
function initFloorPlan(container, floorId) {
    // This is a placeholder - in a real app you would implement
    // a proper floor plan visualization, possibly with SVG or Canvas
    
    container.innerHTML = `
        <div class="text-center p-4">
            <div class="text-lg font-medium mb-2">Floor Plan</div>
            <div class="text-sm text-gray-500">Interactive floor plan would appear here</div>
            <div class="mt-4 text-xs text-gray-400">Floor ID: ${floorId}</div>
        </div>
    `;
}