/**
 * Notifications page JavaScript
 * Handles notification interactions and updates
 */

document.addEventListener('DOMContentLoaded', function() {
    console.log('Notifications page loaded');
    
    // Get all notification cards
    const notificationCards = document.querySelectorAll('.card');
    
    // Add click event to notification cards for expandable view
    notificationCards.forEach(card => {
        card.addEventListener('click', function(e) {
            // Don't trigger if clicking on action link
            if (e.target.tagName === 'A') return;
            
            // Toggle expanded class
            this.classList.toggle('expanded');
        });
    });
    
    // Function to format notification timestamps
    function formatTimestamps() {
        const timestamps = document.querySelectorAll('.notification-time');
        timestamps.forEach(timestamp => {
            const time = timestamp.textContent.trim();
            if (time) {
                try {
                    // Try to parse the timestamp and format it
                    const date = new Date(time);
                    if (!isNaN(date.getTime())) {
                        timestamp.textContent = formatTimeAgo(date);
                    }
                } catch (e) {
                    console.log('Error formatting timestamp:', e);
                }
            }
        });
    }
    
    // Format relative time (e.g., "2 hours ago")
    function formatTimeAgo(date) {
        const now = new Date();
        const diffMs = now - date;
        const diffSec = Math.round(diffMs / 1000);
        const diffMin = Math.round(diffSec / 60);
        const diffHour = Math.round(diffMin / 60);
        const diffDay = Math.round(diffHour / 24);
        
        if (diffSec < 60) {
            return 'just now';
        } else if (diffMin < 60) {
            return `${diffMin} minute${diffMin > 1 ? 's' : ''} ago`;
        } else if (diffHour < 24) {
            return `${diffHour} hour${diffHour > 1 ? 's' : ''} ago`;
        } else if (diffDay < 7) {
            return `${diffDay} day${diffDay > 1 ? 's' : ''} ago`;
        } else {
            return date.toLocaleDateString();
        }
    }
    
    // Run timestamp formatting
    formatTimestamps();
});
