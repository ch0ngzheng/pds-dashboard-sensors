/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "../app/templates/**/*.html", // all HTML templates
    "../app/static/js/**/*.js",   // if you use JS files with Tailwind classes
  ],
  theme: {
    extend: {},
  },
  plugins: [],
}

