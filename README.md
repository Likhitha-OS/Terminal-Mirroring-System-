# Terminal Mirroring System

## Overview
This project implements a **Terminal Mirroring System** using message queues, allowing multiple terminals (clients) to interact and mirror commands and outputs. The system consists of a **server** and multiple **clients**, which communicate via a single message queue. The server maintains a list of coupled terminals and broadcasts commands and outputs entered by one client to all other coupled clients.

## Features
- **Server**:
  - Maintains the message queue.
  - Keeps track of coupled client terminals.
  - Broadcasts commands and their outputs to all coupled clients.
- **Client**:
  - Provides an interactive shell interface.
  - Executes single-line commands using a bash subprocess.
  - Mirrors commands and outputs from other coupled clients in real time.
- **Joining and Leaving Protocol**:
  - Clients can join the coupling group by typing `couple`.
  - Clients can leave the coupling group by typing `uncouple`.
  - Upon joining, the server assigns a unique ID to the client, displayed in the terminal.
- **Broadcasting**:
  - Commands and outputs entered on one client are broadcasted to all other coupled clients.
  - Displays the ID of the origin terminal along with the broadcasted command and output.

## Files
### 1. `server.c`
The server program:
- Creates and maintains a single message queue.
- Manages a list of coupled clients.
- Receives messages (commands and outputs) from clients.
- Broadcasts received messages to all other coupled clients.

### 2. `client.c` 
The client program:
- Provides a shell-like interface.
- Allows users to join (`couple`) or leave (`uncouple`) the coupling group.
- Sends commands and outputs to the server for broadcasting.
- Displays mirrored commands and outputs from other coupled clients.

## How It Works
### Server
1. Start the server first to initialize the message queue and prepare for client connections.
2. The server runs in the background, maintaining the coupling group and broadcasting messages.
3. Displays an updated list of coupled clients whenever a client joins or leaves.

### Client
1. Start the client, which connects to the server through the message queue.
2. By default, the client starts in an **uncoupled** state.
3. Type `couple` to join the coupling group. The server assigns an ID, displayed in the terminal.
4. Type shell commands to execute them locally. The command and its output are also broadcasted to all coupled clients.
5. Type `uncouple` to leave the coupling group.
6. Type `exit` to terminate the client program.

### Joining and Broadcasting Example
#### Terminal 1:
```
/usr/home: couple
ID: 1
/usr/home: ls
File1.c
File2.txt
```
#### Terminal 2:
```
/usr/home: couple
ID: 2
/usr/home: mkdir test
/usr/home: cd test
/usr/home/test: ls
```
Output on Terminal 1:
```
Terminal 2: mkdir test
Terminal 2: cd test
Terminal 2: ls
```

## Usage
### Compilation
Use the following commands to compile the programs:
```bash
gcc -o server server.c -lrt
gcc -o client client.c -lrt
```

### Execution
1. Start the server:
   ```bash
   ./server
   ```
2. Start one or more clients in separate terminals:
   ```bash
   ./client
   ```
3. Use the `couple`, `uncouple`, and shell commands as described.

### Commands
- **couple**: Join the coupling group and receive an ID from the server.
- **uncouple**: Leave the coupling group.
- **exit**: Terminate the client program.
- Any valid shell command: Execute the command locally and broadcast it to all coupled clients.

## Implementation Details
- **Message Queue**:
  - A single message queue is shared by all programs.
  - Messages include the client ID, command, and output.
  - Priority features of message queues ensure proper sequencing.
- **Client Execution**:
  - Commands are executed in a forked subprocess using `bash`.
  - Outputs are captured and sent to the server for broadcasting.
- **Server Functionality**:
  - Maintains a list of coupled clients.
  - Forwards received messages to all other clients in the group.

## Dependencies
- GCC compiler.
- Linux-based operating system.
- System V message queue support.

## Example Outputs
### Terminal 1 (Before coupling):
```
/usr/home: ls
File1.c
File2.txt
```
### Terminal 2:
```
/usr/home: couple
ID: 2
/usr/home: mkdir test
/usr/home: cd test
/usr/home/test: ls
```
### Terminal 1 (After coupling):
```
Terminal 2: mkdir test
Terminal 2: cd test
Terminal 2: ls
```

