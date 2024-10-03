#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLIENTS 100
#define MAX_TRACKERS 5
#define BUFFER_SIZE 1024
#define MAX_USERS 100
#define MAX_GROUPS 10

typedef struct {
    char ip[16];
    int port;
} TrackerInfo;

typedef struct {
    char user_id[20];
    char password[20];
} User;

typedef struct {
    char group_id[20];
    char members[MAX_USERS][20]; // Store members
    int member_count; // Count of members in the group
} Group;

TrackerInfo trackers[MAX_TRACKERS];  // Trackers list
User users[MAX_USERS];                // User accounts
Group groups[MAX_GROUPS];             // Groups
int tracker_count = 0;
int user_count = 0;                   // Total number of users
int group_count = 0;                  // Total number of groups
pthread_mutex_t client_lock;

// Function to load tracker information using open() and read()
void load_trackers(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Unable to open tracker info file");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        perror("Error reading the file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    buffer[bytes_read] = '\0'; // Null-terminate the string

    // Tokenize the content by lines and load IP and port
    char *line = strtok(buffer, "\n");
    while (line != NULL && tracker_count < MAX_TRACKERS) {
        sscanf(line, "%s %d", trackers[tracker_count].ip, &trackers[tracker_count].port);
        tracker_count++;
        line = strtok(NULL, "\n");
    }

    close(fd);
}

// Handle client commands
void *handle_client(void *arg) {
    int new_sock = *(int *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE); // Clear the buffer
        int bytes_read = read(new_sock, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            printf("Client disconnected\n");
            break;
        }
        buffer[bytes_read] = '\0'; // Null-terminate the string
        printf("Received command from client: %s\n", buffer);

        // Command processing
        char response[BUFFER_SIZE];
        if (strncmp(buffer, "create_user", 11) == 0) {
            char user_id[20], password[20];
            sscanf(buffer, "create_user %s %s", user_id, password);
            // Check if user already exists
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].user_id, user_id) == 0) {
                    snprintf(response, sizeof(response), "User %s already exists.", user_id);
                    send(new_sock, response, strlen(response), 0);
                    break;
                }
            }
            // Create new user
            if (user_count < MAX_USERS) {
                strcpy(users[user_count].user_id, user_id);
                strcpy(users[user_count].password, password);
                user_count++;
                snprintf(response, sizeof(response), "User %s created successfully.", user_id);
                send(new_sock, response, strlen(response), 0);
            } else {
                snprintf(response, sizeof(response), "User limit reached.");
                send(new_sock, response, strlen(response), 0);
            }
        } else if (strncmp(buffer, "login", 5) == 0) {
            char user_id[20], password[20];
            sscanf(buffer, "login %s %s", user_id, password);
            // Check user credentials
            int found = 0;
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].user_id, user_id) == 0 && strcmp(users[i].password, password) == 0) {
                    snprintf(response, sizeof(response), "User %s logged in successfully.", user_id);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                snprintf(response, sizeof(response), "Invalid user ID or password.");
            }
            send(new_sock, response, strlen(response), 0);
        } else if (strncmp(buffer, "create_group", 12) == 0) {
            char group_id[20];
            sscanf(buffer, "create_group %s", group_id);
            // Check if group already exists
            for (int i = 0; i < group_count; i++) {
                if (strcmp(groups[i].group_id, group_id) == 0) {
                    snprintf(response, sizeof(response), "Group %s already exists.", group_id);
                    send(new_sock, response, strlen(response), 0);
                    break;
                }
            }
            // Create new group
            if (group_count < MAX_GROUPS) {
                strcpy(groups[group_count].group_id, group_id);
                groups[group_count].member_count = 0;
                group_count++;
                snprintf(response, sizeof(response), "Group %s created successfully.", group_id);
                send(new_sock, response, strlen(response), 0);
            } else {
                snprintf(response, sizeof(response), "Group limit reached.");
                send(new_sock, response, strlen(response), 0);
            }
        } else {
            snprintf(response, sizeof(response), "Unknown command.");
            send(new_sock, response, strlen(response), 0);
        }
    }
    close(new_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: ./tracker tracker_info.txt tracker_no\n");
        return -1;
    }

    load_trackers(argv[1]);

    int tracker_no = atoi(argv[2]);
    
    // Subtract 1 to convert tracker number to 0-based index
    if (tracker_no < 1 || tracker_no > tracker_count) {
        printf("Invalid tracker number. Please enter a number between 1 and %d.\n", tracker_count);
        return -1;
    }

    printf("Tracker %d initialized at IP: %s and Port: %d\n", tracker_no, trackers[tracker_no - 1].ip, trackers[tracker_no - 1].port);

    int server_fd, new_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(trackers[tracker_no - 1].port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Tracker %d listening on port %d\n", tracker_no, trackers[tracker_no - 1].port);

    pthread_t thread_id;

    // Infinite loop to accept connections from clients
    while (1) {
        new_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_sock >= 0) {
            printf("Connection accepted\n");
            pthread_create(&thread_id, NULL, handle_client, &new_sock);
        } else {
            perror("Accept failed");
        }
    }

    close(server_fd);
    return 0;
}
