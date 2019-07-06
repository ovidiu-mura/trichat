# trichat - Chat Application


Project Requirements - https://web.cecs.pdx.edu/~markem/ALSP/project-description.pdf


## Client and Server configured and send two messages
### client -> server
### server -> client


# Notes:
- Serialize/deserialize data
- Start one chat per session
- Messages structure (ids of sender/receiver)
- server validates the uniqness of eache client id
- multi-threading (Alex)
- arguments validation server/client (Kamakshi)

Message:
- id sender/receiver
- buffer 
- timestamp (sent time)
- destroy message if fails to be sent
- write all the message to a file (client mainain a file with all received messages of each session)


Client Structure:

Connected Clients:
client1
client2
client3

client1@client3: Test message1
recvd msg from client3: Test message2
client1@client2: Test message3

client2 logged out

new client4 joined chat
~                          

Client attributes:
- unique client id
- get users
- client waits for the server response and prints for every message sent, "sent"

Server attributes:
- one sending message queue
- one receiving message queue
- algorithm sending the packets with msg (FIFO)
- sleep all the threads
- all the active users

Packeges:
- size 1K
- file transfer
- packet number
- client id (sender/receiver)
- packege will have the receiver id (string:name)

Memory Management:
- everytime a client sends a message client allocates new memory
- memory is freed after the message is received by the client
- server queue the message in a new allocated memory, it stores in the queue and after the message is received by the receiver client, the memory will be destroied (freed)
