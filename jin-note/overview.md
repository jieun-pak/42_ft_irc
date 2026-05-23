# Project
## definition
to create our own IRC server.
can use an actual IRC client to connect to our server and test it.
create a C++ program that...
- listens for incoming connections on a specific port, 
- understands IRC text commands
- routes messages between users

## in practice
the program should run:
```cpp
./ircserv <port> <password>
```
port: the port your server listens to (usually 6667)
pw: the global pw users must provide to connect

## requirements
[] must use non-blocking I/O multiplexing
   - [] do not use multi-threading or fork() to handle multiple users
   - [] should use one of these system calls (`poll/epoll/kqueue`)
      - `poll()`: instead of checking every user connection constantly, it alerts you: "A sent you msg", "B disconnected", etc.

[] support `core actions` using standard IRC text commands
   - [] Authenticaton
   - [] identity(Nickname, username)
   - [] channels
   - [] messaging

[] support channel operator commands for admin
   - [] KICK:	Boot a user out of the channel.
   - [] INVITE:	Allow a specific user into a channel.
   - [] TOPIC:	View or change the channel's description.
   - [] MODE:	Change the rules of the channel (see below).

[] support channel mods
   - []`i`: Make the channel invite-only.
   - []`t`: Only Operators can change the TOPIC.
   - []`k`: Set a channel password.
   - []`o`: Promote a regular user to Operator.
   - []`l`: Set a maximum limit of users in the channel.

[] handle data fragmentation issue of `read()`
   - If a user sends the command `"JOIN #general\n"`, it might arrive at your server in chunks: `"JO"`, `"IN #ge"`,`"neral\n"`
   - handle this (the nc -C test)
   - store incoming text in a buffer for each user and only process it once you detect the end-of-line character (\n or \r\n).



# IRC (Internet Relay Chat)
## definition
protocoal for real-time text messaging btw internet-connected computers in 1988.

grandfather of Slack or Discord

## characteristics
- text-based and real-time chat system
- has two parts
   - client: the app a user opens on their computer with UI. We are NOT building it.
   - server: central hub that all clients connect to. it receives a message from one client and distributes it to the correct recipients. We should build it.


# Questions
Q1. how to connect to correct recipients?
Q2. IRC text commands?
Q3. how IRC is working?