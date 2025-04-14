import os
import shutil

# Files to remove
files_to_remove = [
    'run.py',
    'app/__init__.py',
    'app/config.py',
    'app/firebase/__init__.py',
    'app/firebase/firebase_client.py',
    'app/routes/api_routes.py',
    'app/routes/battery_routes.py',
    'app/routes/floor_routes.py',
    'app/routes/main_routes.py',
    'check_env.py',
    'debug_app.py',
    'debug_flask.py',
    'minimal_app.py'
]

# Directories that might be empty after file removal
directories_to_check = [
    'app/firebase',
    'app/routes'
]

# Convert paths to use OS-specific path separator
files_to_remove = [file.replace('/', os.sep) for file in files_to_remove]
directories_to_check = [directory.replace('/', os.sep) for directory in directories_to_check]

# Function to remove a file if it exists
def remove_file(file_path):
    if os.path.exists(file_path):
        try:
            os.remove(file_path)
            print(f"Removed: {file_path}")
            return True
        except Exception as e:
            print(f"Error removing {file_path}: {e}")
            return False
    else:
        print(f"File not found: {file_path}")
        return False

# Function to remove empty directories
def remove_empty_directory(directory_path):
    if os.path.exists(directory_path) and os.path.isdir(directory_path):
        if not os.listdir(directory_path):  # Check if directory is empty
            try:
                os.rmdir(directory_path)
                print(f"Removed empty directory: {directory_path}")
                return True
            except Exception as e:
                print(f"Error removing directory {directory_path}: {e}")
                return False
    return False

# Main cleanup function
def cleanup():
    print("Starting cleanup...")
    
    # Remove files
    for file_path in files_to_remove:
        remove_file(file_path)
    
    # Check and remove empty directories
    for directory_path in directories_to_check:
        remove_empty_directory(directory_path)
    
    print("Cleanup completed!")

if __name__ == "__main__":
    print("This will remove files from your original application structure that are no longer needed.")
    print("Proceeding with cleanup...")
    cleanup()
