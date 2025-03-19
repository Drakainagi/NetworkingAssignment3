REFER TO SAMPLE ZIP FILE FOR STRUCTURE IN BUILD FOLDER
# Server Configuration Parameters (SELECTIVE REPEAT)

# CHUNK_SIZE:
#   - Lower value helps avoid IP fragmentation on unstable networks.
# Recommended Range: 1024 - 32768 bytes.
CHUNK_SIZE = 32768

# ACK_TIMEOUT:
#   - Timeout in milliseconds; adjust if you experience high latency.
# Recommended Range: 200 - 1500 ms.
ACK_TIMEOUT = 1000

# MAX_RETRIES:
#   - Fewer retries to prevent long delays if the network is poor.
# Recommended Range: 5 - infinity.
MAX_RETRIES = 5

# WINDOW_SIZE:
#   - Smaller window minimizes congestion in unreliable networks.
# Recommended Range: 4 - 128.
WINDOW_SIZE = 128

***********************************************************************************************************************************************************************
## Supported Commands

### /q
- **Usage:**  
  Simply type `/q` and press Enter.
- **Function:**  
  - Sends a quit command (CMD_REQ_QUIT) to the server.
  - Initiates a graceful shutdown of the client.
- **When to Use:**  
  - Use this command when you want to disconnect from the server and exit the client application.

---

### /l
- **Usage:**  
  Simply type `/l` and press Enter.
- **Function:**  
  - Sends a file list request (CMD_REQ_LISTFILES) to the server.
  - Receives and displays the list of available files on the server.
- **When to Use:**  
  - Use this command when you want to view the available files on the server before initiating a download.

---

### /d \<client_ip:udpPort> \<filename>
- **Usage:**  
  Type `/d` followed by two parameters:  
  1. The client (your local IP and UDP port) in the format `ip:udpPort`  
  2. The name of the file you wish to download  
  **Example:**  
  `/d 192.168.1.100:5000 example.txt`
- **Function:**  
  - Sends a download request (CMD_REQ_DOWNLOAD) to the server.
  - The request includes:
    - Your UDP IP address and UDP port (to indicate where to send the file data).
    - The filename you want to download.
  - Upon a valid response from the server, the client starts receiving file data via UDP.
- **When to Use:**  
  - Use this command when you want to download a specific file from the server.
  - Ensure the destination information is accurate and that the requested file exists on the server.

***********************************************************************************************************************************************************************

## Additional Notes
- **Building Project:**  
  Edit the #if close to the top of the respective .cpp files to determine which file to build to generate the repective .exe file.
- **Unknown Commands:**  
  If you enter a command that is not recognized (anything other than `/q`, `/l`, or `/d`), the client will display a message indicating that the command is unknown along with a reminder of the supported commands.
- **Command Input:**  
  Commands are entered via the console. Ensure there are no leading or trailing spaces that might affect parsing.
- **Error Handling:**  
  In the event of an invalid command format (for example, an incorrectly formatted destination for `/d`), the client will notify you with an appropriate error message.

---