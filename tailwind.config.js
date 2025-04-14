/** @type {import('tailwindcss').Config} */
module.exports = {
    content: [
      "./app/templates/**/*.html",
      "./app/static/js/**/*.js"
    ],
    theme: {
      extend: {
        colors: {
          // Custom colors based on your design
          'optimal': '#38c172',      // Green for optimal status
          'sub-optimal': '#f6993f',  // Orange for sub-optimal status
          'critical': '#e3342f',     // Red for critical status
        },
      },
    },
    plugins: [],
  }