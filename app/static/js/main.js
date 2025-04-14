// Main JavaScript functionality

// Helper function for making API requests
async function fetchAPI(endpoint, options = {}) {
    try {
        const response = await fetch(`/api/${endpoint}`, {
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json'
            },
            ...options
        });
        
        if (!response.ok) {
            throw new Error(`API error: ${response.status}`);
        }
        
        return await response.json();
    } catch (error) {
        console.error(`Error fetching ${endpoint}:`, error);
        return { error: error.message };
    }
}

// Format number with comma separators
function formatNumber(num) {
    return num.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
}

// Format date to readable format
function formatDate(dateString) {
    const date = new Date(dateString);
    return date.toLocaleString();
}

// Function to show notifications
function showNotification(message, type = 'info') {
    // Create notification element
    const notif = document.createElement('div');
    notif.className = `fixed top-4 right-4 p-4 rounded-lg shadow-lg transition-all transform translate-x-full opacity-0
                      ${type === 'error' ? 'bg-red-500 text-white' : 
                      type === 'success' ? 'bg-green-500 text-white' : 
                      'bg-blue-500 text-white'}`;
    notif.textContent = message;
    
    // Add to DOM
    document.body.appendChild(notif);
    
    // Trigger animation
    setTimeout(() => {
        notif.classList.remove('translate-x-full', 'opacity-0');
    }, 10);
    
    // Auto remove
    setTimeout(() => {
        notif.classList.add('translate-x-full', 'opacity-0');
        setTimeout(() => {
            notif.remove();
        }, 300);
    }, 3000);
}

// Initialize any page-specific functionality
document.addEventListener('DOMContentLoaded', function() {
    // Initialize common elements
    
    // Setup notification button in header if it exists
    const notificationBtn = document.querySelector('header button');
    if (notificationBtn) {
        notificationBtn.addEventListener('click', function() {
            window.location.href = '/notifications';
        });
    }
    
    // Initialize any interactive elements with data-action attributes
    document.querySelectorAll('[data-action]').forEach(element => {
        element.addEventListener('click', function() {
            const action = this.getAttribute('data-action');
            const id = this.getAttribute('data-id');
            
            // Handle different actions
            switch(action) {
                case 'toggle':
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
                            const stateElement = this.querySelector('.state');
                            if (stateElement) {
                                stateElement.textContent = newState.toUpperCase();
                                stateElement.className = `state ${newState === 'on' ? 'text-green-600' : 'text-gray-600'}`;
                            }
                            
                            showNotification(`Device ${newState === 'on' ? 'turned on' : 'turned off'}`, 'success');
                        } else {
                            showNotification('Failed to toggle device', 'error');
                        }
                    });
                    break;
                    
                default:
                    console.log(`Unknown action: ${action}`);
            }
        });
    });
});