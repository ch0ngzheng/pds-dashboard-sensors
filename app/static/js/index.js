// Dashboard functionality
document.addEventListener('DOMContentLoaded', function() {
    // Initialize battery level
    const batteryLevel = document.querySelector('.battery-level');
    if (batteryLevel) {
        const level = batteryLevel.getAttribute('data-level') || '0';
        batteryLevel.style.width = `${level}%`;
    }

    // Initialize status colors
    const statusColors = {
        'optimal': '#10B981',     // Green
        'sub-optimal': '#F59E0B', // Yellow
        'critical': '#EF4444'     // Red
    };

    // Apply status colors
    document.querySelectorAll('[data-status]').forEach(element => {
        const status = element.getAttribute('data-status');
        if (status && statusColors[status]) {
            element.style.backgroundColor = statusColors[status];
        }
    });
});
