# Local Network Chat Server (TCP Sockets, CLI)

## Files
- `server.c` - TCP server; accepts multiple clients with `select()`.
- `client.c` - terminal chat client.
- `chat_history.log` - created automatically by the server when messages/events are logged.

## Requirements
A C compiler (GCC/Clang) and a POSIX operating system such as Linux, macOS, or WSL.
The code uses BSD/POSIX socket APIs and is not a native Windows Winsock program.

## Compile
Open a terminal in this folder:

```bash
gcc -Wall -Wextra -std=c11 server.c -o server
gcc -Wall -Wextra -std=c11 client.c -o client
```

## Run locally
Terminal 1:
```bash
./server 5000
```

Terminal 2:
```bash
./client 127.0.0.1 5000 Ayush
```

Terminal 3 (another client):
```bash
./client 127.0.0.1 5000 Rahul
```

## Run across a LAN
1. Connect both computers to the same trusted Wi-Fi/LAN.
2. Start the server on one computer: `./server 5000`.
3. Find that computer's private LAN IPv4 address (for example, `192.168.1.5`).
4. On each other computer run: `./client 192.168.1.5 5000 UserName`.
5. Allow TCP port 5000 through the host firewall if prompted. Use only on a trusted local network.

## Client commands
- `/nick name` - change your nickname.
- `/msg username message` - send a private message.
- `/list` - show connected users.
- `/quit` - leave the chat.
- Any other line is broadcast to all connected clients.

## Server administrator commands
Type these in the server terminal:
- `/clients` - show connected nicknames.
- `/logs` - display the saved chat history.
- `/shutdown` - stop the server and disconnect clients.

## Notes
- The server maintains a dynamically resized array of client structures using `realloc()`.
- `select()` monitors the listening socket, client sockets, and server console.
- TCP is a byte stream, so the server buffers incoming bytes and processes newline-terminated commands.
- The chat log is stored in `chat_history.log` in the server's current working directory.
- This is an educational plain-text chat program. It does not encrypt messages or authenticate users.
