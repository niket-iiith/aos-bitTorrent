#include<stdio.h>
#include<string>
#include<pthread.h>

using namespace std;

struct Tracker{
    __uint32_t ip;
    int port;
};

struct Client{
    string id;
    string password;
};

struct File{
    string fileName;
    string sha1;
    int groupId;
};

Tracker tracker[2];
Client client[100];
File file[100];


int trackerCount = 0;
int clientCount = 0;
pthread_mutex_t lockClient;


