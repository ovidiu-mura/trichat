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
