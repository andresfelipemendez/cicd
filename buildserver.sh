#!/bin/bash
echo "buildserver.sh starting. Initial CWD: $(pwd)"
echo "Script location according to $0: $0"
# Define the server directory
SERVER_DIR="/home/andres/development/cserver"
SERVER_EXECUTABLE_NAME="personalserver" # The name of your compiled C server executable
SERVER_SOURCE_FILE="server.c"

# --- Navigate to the server directory ---
echo "INFO: Changing directory to ${SERVER_DIR}..."
cd "${SERVER_DIR}" || { echo "ERROR: Could not navigate to ${SERVER_DIR}. Aborting."; exit 1; }

echo "INFO: Attempting to stop any existing '${SERVER_EXECUTABLE_NAME}' process..."

# --- Kill previous server process ---
echo "INFO: Attempting to stop any existing '${SERVER_EXECUTABLE_NAME}' process..."
# Using pkill to find and kill the process by its command line.
# The -f flag matches against the full command line.
# We use "./" to be more specific to the executable in the current directory.
if pkill -f "./${SERVER_EXECUTABLE_NAME}"; then
    echo "INFO: Sent SIGTERM to running './${SERVER_EXECUTABLE_NAME}' process(es)."
    echo "INFO: Waiting a couple of seconds for the old process to stop..."
    sleep 2 # Give it a moment to shut down gracefully and release resources (like network ports)
else
    echo "INFO: No './${SERVER_EXECUTABLE_NAME}' process was found running."
fi

git pull

# --- Compile the server ---
echo "INFO: Compiling ${SERVER_SOURCE_FILE} into ./${SERVER_EXECUTABLE_NAME}..."
gcc "${SERVER_SOURCE_FILE}" -o "${SERVER_EXECUTABLE_NAME}"
# Check if compilation was successful
if [ $? -ne 0 ]; then
  echo "ERROR: Compilation of ${SERVER_SOURCE_FILE} failed. Aborting."
  exit 1
fi
echo "INFO: Compilation successful."

# --- Run the new server ---
echo "INFO: Starting the new './${SERVER_EXECUTABLE_NAME}' process in the background..."
# Run the server in the background so the script can exit while the server keeps running.
# In buildserver.sh, for the last line:
LOG_DIR="${SERVER_DIR}/logs" # Example log directory
mkdir -p "${LOG_DIR}"
nohup ./${SERVER_EXECUTABLE_NAME} > "${LOG_DIR}/server_stdout.log" 2> "${LOG_DIR}/server_stderr.log" &
# Capture the Process ID (PID) of the new background server process
NEW_SERVER_PID=$!
echo "INFO: New server started with PID: ${NEW_SERVER_PID}."

# Optionally, you can save this PID to a file if you need to manage the process later
# (e.g., for a dedicated stop script or status check)
# echo ${NEW_SERVER_PID} > server.pid

echo "INFO: Deployment script finished."
exit 0