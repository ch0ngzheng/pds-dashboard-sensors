// app/static/js/visitors.js

document.addEventListener('DOMContentLoaded', function() {
    // Set up real-time updates for visitor information
    setupVisitorsUpdates();
});

// Function to periodically update visitor information
function setupVisitorsUpdates() {
    // First update
    updateVisitorsInfo();
    
    // Then set interval for future updates (every 30 seconds)
    setInterval(updateVisitorsInfo, 30000);
}

// Function to fetch and update visitor information
async function updateVisitorsInfo() {
    try {
        const visitorsData = await fetchAPI('visitors');
        
        if (visitorsData.error) {
            console.error('Error fetching visitors data:', visitorsData.error);
            return;
        }
        
        // Update visitor count
        const visitorCountEl = document.querySelector('.visitor-count');
        if (visitorCountEl && visitorsData.count !== undefined) {
            visitorCountEl.textContent = visitorsData.count;
        }

        // Update last activity timestamp
        const lastActivityEl = document.querySelector('.last-activity');
        if (lastActivityEl && visitorsData.last_activity) {
            const date = new Date(visitorsData.last_activity * 1000);
            lastActivityEl.textContent = date.toLocaleString();
        }

        // Update guests list
        const guestsContainer = document.getElementById('guests-container');
        if (guestsContainer && visitorsData.guests) {
            guestsContainer.innerHTML = '';
            Object.entries(visitorsData.guests).forEach(([guestId, guestData]) => {
                const guestElement = document.createElement('div');
                guestElement.className = 'guest-item';
                const status = guestData.currentRoom ? 'present' : 'absent';
                const lastSeen = guestData.lastSeen ? new Date(guestData.lastSeen * 1000).toLocaleString() : 'Never';
                guestElement.innerHTML = `
                    <strong>${guestData.name || 'Guest'}</strong>
                    <span class="guest-status ${status}">${status.toUpperCase()}</span>
                    <div class="guest-details">
                        <span class="guest-tags">Tags: ${Object.keys(guestData.tags || {}).join(', ')}</span>
                        <span class="guest-last-seen">Last seen: ${lastSeen}</span>
                    </div>
                `;
                guestsContainer.appendChild(guestElement);
            });
        }

        // Update rooms data
        const roomsContainer = document.querySelector('.rooms-container');
        if (roomsContainer && visitorsData.rooms) {
            roomsContainer.innerHTML = '';
            Object.entries(visitorsData.rooms).forEach(([room, count]) => {
                const roomId = `room-${room.toLowerCase().replace(/\s+/g, '-')}`;
                // count is now an integer directly from backend

                // Create room row
                const roomRow = document.createElement('div');
                roomRow.className = 'border rounded-lg p-3 flex justify-between items-center';

                const leftDiv = document.createElement('div');
                leftDiv.className = 'flex items-center';
                leftDiv.innerHTML = `
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 mr-2 text-blue-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M16 7a4 4 0 11-8 0 4 4 0 018 0zM12 14a7 7 0 00-7 7h14a7 7 0 00-7-7z" />
                    </svg>
                    <span>${room}</span>
                `;

                // Right: visitor count only
                const rightSpan = document.createElement('span');
                rightSpan.className = `font-medium ${roomId}`;
                rightSpan.textContent = `${count} visitors`;

                roomRow.appendChild(leftDiv);
                roomRow.appendChild(rightSpan);
                roomsContainer.appendChild(roomRow);
            });
        }
        
    } catch (error) {
        console.error('Error updating visitors information:', error);
    }
}