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

TrackerInfo trackers[MAX_TRACKERS]; 
int tracker_count = 0;

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
    buffer[bytes_read] = '\0';

    char *line = strtok(buffer, "\n");
    while (line != NULL && tracker_count < MAX_TRACKERS) {
        sscanf(line, "%s %d", trackers[tracker_count].ip, &trackers[tracker_count].port);
        tracker_count++;
        line = strtok(NULL, "\n");
    }

    close(fd);
}

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

    char command[BUFFER_SIZE];
    while (true) {
        cout << "Enter command: ";
        cin.getline(command, sizeof(command));  

        send(sockfd, command, strlen(command), 0);

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE); 
        int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; 
            printf("Received from tracker: %s\n", buffer);
        } else if (bytes_received == 0) {
            printf("Tracker closed the connection\n");
            break;
        }
    }

    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./client <IP>:<PORT> tracker_info.txt\n");
        return -1;
    }

    load_trackers(argv[2]);

    while (1) {
        connect_to_tracker(0);
        sleep(5);  
    }

    return 0;
}
