#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 4000
#define MAX_CLIENTS 32
#define BUF_SIZE 2048
#define NAME_LEN 32

typedef struct {
    char name[NAME_LEN];
    int fd;
    struct sockaddr_in addr;
    int active;
}Client;

static Client clients[MAX_CLIENTS];
static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;
static int server_fd;

static void timestamp(char* buf, size_t len){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, len, "%H:%M:%S", t);
}

static void send_msg(int fd, const char* msg){
    send(fd, msg, strlen(msg), 0);
}

static void broadcast(const char* msg, int skip_fd){
    pthread_mutex_lock(&registry_lock);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i].active && clients[i].fd != skip_fd){
            send_msg(clients[i].fd, msg);
        }
    }
    pthread_mutex_unlock(&registry_lock);
}

static int send_private_msg(const char* target_name, const char *msg){
    pthread_mutex_lock(&registry_lock);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i].active && strcmp(clients[i].name, target_name) == 0){
            send_msg(clients[i].fd, msg);
            pthread_mutex_unlock(&registry_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&registry_lock);
    return 0;
}

static int name_taken(const char* name){
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i].active && strcmp(clients[i].name, name) == 0){
            return 1;
        }
    }
    return 0;
}

static int find_free_slot(void){
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(!clients[i].active){
            return i;
        }
    }
    return -1;
}

static void list_users(char *buf, size_t len){
    snprintf(buf, len, "[SERVER] Online users: ");
    pthread_mutex_lock(&registry_lock);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i].active){
            strncat(buf, clients[i].name, len - strlen(buf) - 1);
            strncat(buf, " ", len - strlen(buf) - 1);
        }
    }
    pthread_mutex_unlock(&registry_lock);
    strncat(buf, "\n", len - strlen(buf) - 1);
}

static void *client_handler(void *arg){
    int slot = *(int *)arg;
    free(arg);

    int fd = clients[slot].fd;
    char ts[16];
    char buf[BUF_SIZE];
    char out[BUF_SIZE + NAME_LEN + 32];

    // user name
    send_msg(fd, "[SERVER] Welcome! Enter your username: ");

    while(1){
        memset(buf, 0, sizeof(buf));
        int n = recv(fd, buf, NAME_LEN - 1, 0);
        if(n <= 0){
            close(fd);
            clients[slot].active = 0;
            return NULL;
        }

        buf[strcspn(buf, "\r\n")] = '\0';
        if(strlen(buf) == 0){
            send_msg(fd, "[SERVER] Username cannot be empty. Try again: ");
            continue;
        }

        if (strchr(buf, ' ') != NULL || strchr(buf, '\t') != NULL) {
            send_msg(fd, "[WARN]   Username must be a single word with no spaces. Try again: ");
            continue;
        }

        if(strlen(buf) >= NAME_LEN){
            send_msg(fd, "[SERVER] Username entered is too big. Try again: ");
            continue;
        }

        pthread_mutex_lock(&registry_lock);
        if(name_taken(buf)){
            pthread_mutex_unlock(&registry_lock);
            send_msg(fd, "[SERVER] Username taken. Choose another: ");
            continue;
        }
        strncpy(clients[slot].name, buf, NAME_LEN - 1);
        pthread_mutex_unlock(&registry_lock);
        break;
    }

    timestamp(ts, sizeof(ts));
    snprintf(out, sizeof(out), "[%s] *** %s has joined the chat ***\n", ts, clients[slot].name);
    printf("%s", out);
    broadcast(out, fd);

    send_msg(fd, "[SERVER] Commands:\n  /msg <user> <text>  - private message\n  /list               - list online users\n  /quit               - disconnect\n  <text>              - broadcast to all\n\n");

    // messages
    while(1){
        memset(buf, 0, sizeof(buf));
        int n = recv(fd, buf, BUF_SIZE - 1, 0);
        if(n <= 0) break;

        buf[strcspn(buf, "\r\n")] = '\0';
        if(strlen(buf) == 0) continue;

        timestamp(ts, sizeof(ts));

        if(strcmp(buf, "/quit") == 0) break;

        else if(strcmp(buf, "/list") == 0){
            list_users(out, sizeof(out));
            send_msg(fd, out);
        }

        else if(strncmp(buf, "/msg ", 5) == 0){
            char target[NAME_LEN], text[BUF_SIZE];
            if(sscanf(buf + 5, "%31s %[^\n]", target, text) == 2){
                snprintf(out, sizeof(out), "[%s] [PM from %s] %s\n", ts, clients[slot].name, text);
                if(!send_private_msg(target, out)){
                    snprintf(out, sizeof(out), "[SERVER] User \"%s\" not found.\n", target);
                    send_msg(fd, out);
                }
                else {
                    snprintf(out, sizeof(out), "[%s] [PM to %s] %s\n", ts, target, text);
                    send_msg(fd, out);
                }
            } else {
                send_msg(fd, "[SERVER] Usage: /msg <user> <text>\n");
            }
        }

        else {
            snprintf(out, sizeof(out), "[%s] %s: %s\n", ts, clients[slot].name, buf);
            printf("%s", out);
            broadcast(out, fd);
            send_msg(fd, out);
        }
    }

    timestamp(ts, sizeof(ts));
    snprintf(out, sizeof(out), "[%s] *** %s has left the chat ***\n", ts, clients[slot].name);
    printf("%s", out);
    broadcast(out, fd);

    close(fd);
    pthread_mutex_lock(&registry_lock);
    clients[slot].active = 0;
    pthread_mutex_unlock(&registry_lock);

    return NULL;
}

static void handle_sigint(int sig){
    (void)sig;
    printf("\n[SERVER] Shutting down...\n");
    broadcast("[SERVER] Server is shutting down. Goodbye!\n", -1);
    close(server_fd);
    exit(0);
}

int main(){
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof client_addr;

    memset(clients, 0, sizeof clients);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    memset(&server_addr, 0, sizeof server_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0){
        perror("bind");
        exit(1);
    }

    if(listen(server_fd, 10) < 0){
        perror("listen");
        exit(1);
    }

    signal(SIGINT, handle_sigint);

    printf("[SERVER] Chat server listening on port %d\n", PORT);
    printf("[SERVER] Press Ctrl+C to shut down.\n\n");


    while(1){
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {perror("accept"); continue;}

        pthread_mutex_lock(&registry_lock);
        int slot = find_free_slot();

        if(slot == -1){
            pthread_mutex_unlock(&registry_lock);
            send_msg(client_fd, "[SERVER] Server is full. Try again later.\n");
            close(client_fd);
            continue;
        }

        clients[slot].fd = client_fd;
        clients[slot].addr = client_addr;
        clients[slot].active = 1;
        clients[slot].name[0] = '\0';
        pthread_mutex_unlock(&registry_lock);

        printf("[SERVER] New connection from %s:%d (slot %d)\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), slot);

        pthread_t tid;
        int *slot_ptr = malloc(sizeof(int));
        *slot_ptr = slot;
        if(pthread_create(&tid, NULL, client_handler, slot_ptr) != 0){
            perror("pthread_create");
            free(slot_ptr);
            close(client_fd);
            clients[slot].active = 0;
            continue;
        }
        pthread_detach(tid);
    }
    return 0;
}