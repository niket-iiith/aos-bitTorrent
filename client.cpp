#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include<fcntl.h>

#define MAX_TRACKERS 5
#define BUFFER_SIZE 1024

using namespace std;

typedef struct {
    char ip[16];
    int port;
} TrackerInfo;

TrackerInfo trackers[MAX_TRACKERS];  // Trackers list
int tracker_count = 0;

// Load tracker info using open() and read() system calls
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

// Connect to the tracker
void connect_to_tracker(int tracker_no) {
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(trackers[tracker_no].port);
    inet_pton(AF_INET, trackers[tracker_no].ip, &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to tracker failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to tracker %d at %s:%d\n", tracker_no, trackers[tracker_no].ip, trackers[tracker_no].port);

    // Infinite loop to send user input as commands
    char command[BUFFER_SIZE];
    while (true) {
        cout << "Enter command: ";
        cin.getline(command, sizeof(command));  // Get user input

        // Send the command to the tracker
        send(sockfd, command, strlen(command), 0);

        // Receive response from the tracker
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE); // Clear the buffer
        int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate the string
            printf("Received from tracker: %s\n", buffer);
        } else if (bytes_received == 0) {
            printf("Tracker closed the connection\n");
            break;
        }
    }

    close(sockfd);
}

// Main client function
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./client tracker_info.txt\n");
        return -1;
    }

    load_trackers(argv[1]);

    // Infinite loop to connect to the tracker
    while (1) {
        // Connect to the first tracker
        connect_to_tracker(0);
        sleep(5);  // Sleep before retrying
    }

    return 0;
}
