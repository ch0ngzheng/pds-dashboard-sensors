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
        
        // Update total visitors
        const totalVisitorsEl = document.querySelector('.visitors-total');
        if (totalVisitorsEl && visitorsData.total !== undefined) {
            totalVisitorsEl.textContent = visitorsData.total;
        }
        
        // Update visitor list if DOM is updated
        if (visitorsData.rooms) {
            // This would update the visitor counts if we had a more dynamic implementation
            // For now, just log the updated data
            console.log('Visitors data updated:', visitorsData.rooms);
        }
        
    } catch (error) {
        console.error('Error updating visitors information:', error);
    }
}