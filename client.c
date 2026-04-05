#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 4000
#define BUF_SIZE 2048

static int sock_fd = -1;

static void *recv_thread(void* arg){
    (void)arg;
    char buf[BUF_SIZE];

    while(1){
        memset(buf, 0, sizeof(buf));
        int n = recv(sock_fd, buf, sizeof(buf) - 1,  0);
        if(n <= 0){
            printf("\n[CLIENT] Disconnected from server.\n");
            exit(0);
        }

        printf("\r%s> ", buf);
        fflush(stdout);
    }
    return NULL;
}

static void handle_sigint(int sig){
    (void)sig;
    printf("\n[CLIENT] Caught Ctrl+C - sending /quit to server...\n");
    if(sock_fd >= 0){
        send(sock_fd, "/quit\n", 6, 0);
    }
    close(sock_fd);
    exit(0);
}

int main(int argc, char* argv[]){
    const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;

    if(argc >= 2) host = argv[1];
    if(argc >= 3) port = atoi(argv[2]);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0){
        fprintf(stderr, "[CLIENT] Invalid address: %s\n", host);
        exit(1);
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0) {
        perror("connect");
        exit(1);
    }

    printf("[CLIENT] Connected to %s:%d\n", host, port);
    printf("[CLIENT] Commands: /msg <user> <text> | /list | /quit\n\n");

    signal(SIGINT, handle_sigint);

    pthread_t rtid;
    if (pthread_create(&rtid, NULL, recv_thread, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }
    pthread_detach(rtid);

    char buf[BUF_SIZE];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(buf, sizeof buf, stdin) == NULL) {
            send(sock_fd, "/quit\n", 6, 0);
            break;
        }

        buf[strcspn(buf, "\n")] = '\n';

        int n = send(sock_fd, buf, strlen(buf), 0);
        if (n < 0) {
            perror("send");
            break;
        }

        if (strncmp(buf, "/quit", 5) == 0) break;
    }
    close(sock_fd);
    return 0;
}