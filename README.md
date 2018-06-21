
Client
======
Running the client: ./client username [host] [port]

Takes a mandatory username (maximum 31 characters). The host and port are
optional, and they default to localhost and port 2000.

Once connected, the client should receive a greeting from the server.
The client can then send and receive messages.

To quit, either send EOF with control-D, or type "/q".

Server
======
Running the server: ./server [port]

The port is optional and defaults to 2000.

