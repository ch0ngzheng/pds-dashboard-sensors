/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "../app/templates/**/*.html", // all HTML templates
    "../app/static/js/**/*.js",   // if you use JS files with Tailwind classes
  ],
  theme: {
    extend: {
      colors: {
        optimal: '#22c55e',        // green-500 or your preferred color
        'sub-optimal': '#f59e42',  // orange or your preferred color
        critical: '#ef4444',       // red-500 or your preferred color
      },
      borderColor: {
        optimal: '#22c55e',
        'sub-optimal': '#f59e42',
        critical: '#ef4444',
      },
    },
  },
  plugins: [],
}

