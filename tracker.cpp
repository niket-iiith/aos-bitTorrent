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

typedef struct
{
    char ip[16];
    int port;
} TrackerInfo;

typedef struct
{
    char user_id[20];
    char password[20];
    int logged_in;
} User;

typedef struct
{
    char group_id[20];
    char owner[20];
    char members[MAX_USERS][20];
    int member_count;
} Group;

TrackerInfo trackers[MAX_TRACKERS];
User users[MAX_USERS];
Group groups[MAX_GROUPS];
int tracker_count = 0;
int user_count = 0;
int group_count = 0;
pthread_mutex_t client_lock;

void load_trackers(const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        perror("Unable to open tracker info file");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0)
    {
        perror("Error reading the file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    buffer[bytes_read] = '\0';

    char *line = strtok(buffer, "\n");
    while (line != NULL && tracker_count < MAX_TRACKERS)
    {
        sscanf(line, "%s %d", trackers[tracker_count].ip, &trackers[tracker_count].port);
        tracker_count++;
        line = strtok(NULL, "\n");
    }

    close(fd);
}

typedef struct
{
    char group_id[20];
    char requested_user[20];
} JoinRequest;

JoinRequest join_requests[MAX_GROUPS * MAX_USERS];
int request_count = 0;

void *handle_client(void *arg)
{
    int new_sock = *(int *)arg;
    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(new_sock, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0)
        {
            printf("Client disconnected\n");
            break;
        }
        buffer[bytes_read] = '\0';
        printf("Received command from client: %s\n", buffer);

        char response[BUFFER_SIZE];
        char user_id[20];
        int user_logged_in = 0;
        strcpy(user_id, "");

        for (int i = 0; i < user_count; i++)
        {
            if (users[i].logged_in)
            {
                user_logged_in = 1;
                strcpy(user_id, users[i].user_id);
                break;
            }
        }

        if (strncmp(buffer, "create_user", 11) == 0)
        {
            char new_user_id[20], password[20];
            sscanf(buffer, "create_user %s %s", new_user_id, password);
            for (int i = 0; i < user_count; i++)
            {
                if (strcmp(users[i].user_id, new_user_id) == 0)
                {
                    snprintf(response, sizeof(response), "User %s already exists.", new_user_id);
                    send(new_sock, response, strlen(response), 0);
                    break;
                }
            }
            if (user_count < MAX_USERS)
            {
                strcpy(users[user_count].user_id, new_user_id);
                strcpy(users[user_count].password, password);
                users[user_count].logged_in = 0;
                user_count++;
                snprintf(response, sizeof(response), "User %s created successfully.", new_user_id);
                send(new_sock, response, strlen(response), 0);
            }
            else
            {
                snprintf(response, sizeof(response), "User limit reached.");
                send(new_sock, response, strlen(response), 0);
            }
        }
        else if (strncmp(buffer, "login", 5) == 0)
        {
            char login_user_id[20], password[20];
            sscanf(buffer, "login %s %s", login_user_id, password);
            int found = 0;
            for (int i = 0; i < user_count; i++)
            {
                if (strcmp(users[i].user_id, login_user_id) == 0 && strcmp(users[i].password, password) == 0)
                {
                    snprintf(response, sizeof(response), "User %s logged in successfully.", login_user_id);
                    users[i].logged_in = 1;
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                snprintf(response, sizeof(response), "Invalid user ID or password.");
            }
            send(new_sock, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "create_group", 12) == 0)
        {
            char group_id[20];
            sscanf(buffer, "create_group %s", group_id);
            for (int i = 0; i < group_count; i++)
            {
                if (strcmp(groups[i].group_id, group_id) == 0)
                {
                    snprintf(response, sizeof(response), "Group %s already exists.", group_id);
                    send(new_sock, response, strlen(response), 0);
                    break;
                }
            }
            if (group_count < MAX_GROUPS)
            {
                strcpy(groups[group_count].group_id, group_id);
                strcpy(groups[group_count].owner, user_id);
                groups[group_count].member_count = 0;
                group_count++;
                snprintf(response, sizeof(response), "Group %s created successfully.", group_id);
                send(new_sock, response, strlen(response), 0);
            }
            else
            {
                snprintf(response, sizeof(response), "Group limit reached.");
                send(new_sock, response, strlen(response), 0);
            }
        }
        else if (strncmp(buffer, "join_group", 10) == 0)
        {
            char group_id[20];
            sscanf(buffer, "join_group %s", group_id);
            if (!user_logged_in)
            {
                snprintf(response, sizeof(response), "You must be logged in to join a group.");
            }
            else
            {
                int group_found = 0;
                for (int i = 0; i < group_count; i++)
                {
                    if (strcmp(groups[i].group_id, group_id) == 0)
                    {
                        snprintf(response, sizeof(response), "Request to join group %s sent to %s.", group_id, groups[i].owner);
                        strcpy(join_requests[request_count].group_id, group_id);
                        strcpy(join_requests[request_count].requested_user, user_id);
                        request_count++;
                        group_found = 1;
                        break;
                    }
                }
                if (!group_found)
                {
                    snprintf(response, sizeof(response), "Group %s does not exist.", group_id);
                }
            }
            send(new_sock, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "leave_group", 11) == 0)
        {
            char group_id[20];
            sscanf(buffer, "leave_group %s", group_id);
            if (!user_logged_in)
            {
                snprintf(response, sizeof(response), "You must be logged in to leave a group.");
            }
            else
            {
                int group_found = 0;
                for (int i = 0; i < group_count; i++)
                {
                    if (strcmp(groups[i].group_id, group_id) == 0)
                    {
                        group_found = 1;
                        int is_owner = strcmp(groups[i].owner, user_id) == 0;
                        if (is_owner && groups[i].member_count == 0)
                        {
                            snprintf(response, sizeof(response), "Group %s deleted as you are the only member.", group_id);
                            for (int j = i; j < group_count - 1; j++)
                            {
                                groups[j] = groups[j + 1];
                            }
                            group_count--;
                        }
                        else if (is_owner)
                        {
                            if (groups[i].member_count > 0)
                            {
                                strcpy(groups[i].owner, groups[i].members[0]);
                                snprintf(response, sizeof(response), "You left group %s. New owner is %s.", group_id, groups[i].owner);
                            }
                            else
                            {
                                snprintf(response, sizeof(response), "You left group %s.", group_id);
                            }
                            groups[i].member_count--;
                        }
                        else
                        {
                            snprintf(response, sizeof(response), "You left group %s.", group_id);
                            groups[i].member_count--;
                        }
                        break;
                    }
                }
                if (!group_found)
                {
                    snprintf(response, sizeof(response), "Group %s does not exist.", group_id);
                }
            }
            send(new_sock, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "list_requests", 13) == 0)
        {
            char group_id[20];
            sscanf(buffer, "list_requests %s", group_id);
            snprintf(response, sizeof(response), "Pending join requests for group %s:", group_id);
            for (int i = 0; i < request_count; i++)
            {
                if (strcmp(join_requests[i].group_id, group_id) == 0)
                {
                    strcat(response, "\n");
                    strcat(response, join_requests[i].requested_user);
                }
            }
            send(new_sock, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "accept_request", 14) == 0)
        {
            char group_id[20], requested_user[20];
            sscanf(buffer, "accept_request %s %s", group_id, requested_user);
            snprintf(response, sizeof(response), "User %s has been added to group %s.", requested_user, group_id);
            send(new_sock, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "list_groups", 11) == 0)
        {
            snprintf(response, sizeof(response), "Groups in the network:");
            for (int i = 0; i < group_count; i++)
            {
                strcat(response, "\n");
                strcat(response, groups[i].group_id);
            }
            send(new_sock, response, strlen(response), 0);
        }
        else
        {
            snprintf(response, sizeof(response), "Unknown command.");
            send(new_sock, response, strlen(response), 0);
        }
    }
    close(new_sock);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: ./tracker tracker_info.txt tracker_no\n");
        return -1;
    }

    load_trackers(argv[1]);

    int tracker_no = atoi(argv[2]);

    if (tracker_no < 1 || tracker_no > tracker_count)
    {
        printf("Invalid tracker number. Please enter a number between 1 and %d.\n", tracker_count);
        return -1;
    }

    printf("Tracker %d initialized at IP: %s and Port: %d\n", tracker_no, trackers[tracker_no - 1].ip, trackers[tracker_no - 1].port);

    int server_fd, new_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(trackers[tracker_no - 1].port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Tracker %d listening on port %d\n", tracker_no, trackers[tracker_no - 1].port);

    pthread_t thread_id;

    while (1)
    {
        new_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_sock >= 0)
        {
            printf("Connection accepted\n");
            pthread_create(&thread_id, NULL, handle_client, &new_sock);
        }
        else
        {
            perror("Accept failed");
        }
    }

    close(server_fd);
    return 0;
}
