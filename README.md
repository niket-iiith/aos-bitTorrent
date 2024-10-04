# Peer Management Server

## Description
The Peer Management Server is a C++ application that manages user accounts and groups. It allows clients to connect to the server, create user accounts, log in, create groups, join groups, leave groups, and manage pending join requests. The server runs in a multi-threaded environment, enabling simultaneous connections from multiple clients.

## Features
### Tracker side command
```
./tracker tracker_info.txt
```

### Client side commands
```
./client <IP>:<PORT> tracker_info.txt
```
```
create_user <user_id> <passwd>
```
```
login <user_id> <passwd>
```
```
create_group <group_id> <group_owner>
```
```
join_group <group_id>
```
```
leave_group <group_id>
```
```
list_requests <group_id>
```
```
accept_request <group_id> <user_id>
```
```
list_groups
```

## INFORMATION
tracker_info.txt is the file that contains IP address and Port number of the tracker.