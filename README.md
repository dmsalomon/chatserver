
Client
======
Running the client: ./client username [host] [port]

Takes a mandatory username (maximum 31 characters). The host and port are
optional, and they default to localhost (127.0.0.1) and port 2000.

Once connected, the client should receive a greeting from the server.
The client can then send and receive messages.

To quit, either send EOF with control-D, or type "/q".

This client code is slightly different from last assignment. Because the
messages can be from different clients, the name must be included
along with each message, instead of just once at the beginning of the
conversation.

Server
======
Running the server: ./server [port]

Nothing special it can take an optional port number to bind on. By default
the port is 2000.


