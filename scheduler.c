#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)

typedef struct {
    int process_id;
    int arrival_time;
    int burst_time;
    int remaining_time;
} Process;

#define MAX_CLIENTS 100

Process client_processes[MAX_CLIENTS];
int client_count = 0;

void add_client_process(pid_t pid) {
    client_processes[client_count].process_id = pid;
    client_processes[client_count].arrival_time = time(NULL); // 도착 시간
    client_processes[client_count].burst_time = rand() % 10 + 1; // 임의의 실행 시간
    client_processes[client_count].remaining_time = client_processes[client_count].burst_time;
    client_count++;
}

void round_robin(Process p[], int n, int time_quantum) {
    int all_done=0;
    int time = 0;

    while (!all_done) {
        if (kill(p[time].process_id, SIGCONT) == 0) {
            printf("프로세스 %d 재개\n", p[time].process_id);
        } else {
            perror("프로세스를 재개할 수 없습니다.");
        }   
        sleep(time_quantum);
        printf("프로세스 %d 중지\n", p[time].process_id);
        kill(p[time].process_id, SIGSTOP);
        if(time==0){ time=1;}
        else{time=0;}

    }
}

int main() {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, "6080", &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    fd_set master;
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    SOCKET max_socket = socket_listen;

    printf("Waiting for connections...\n");

    while (1) {
        fd_set reads;
        reads = master;
        if (select(max_socket + 1, &reads, 0, 0, 0) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

        SOCKET i;
        for (i = 1; i <= max_socket; ++i) {
            if (FD_ISSET(i, &reads)) {
                if (i == socket_listen) {
                    struct sockaddr_storage client_address;
                    socklen_t client_len = sizeof(client_address);
                    SOCKET socket_client = accept(socket_listen, (struct sockaddr *) &client_address, &client_len);
                    if (!ISVALIDSOCKET(socket_client)) {
                        fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
                        return 1;
                    }

                    char address_buffer[100];
                    
                    getnameinfo((struct sockaddr *) &client_address, client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
                    printf("New connection from %s\n", address_buffer);
                    
                    char pid[1024];
                    read(socket_client, pid, 1024);
                    add_client_process((pid_t)atoi(pid));
                    printf("클라이언트의 PID: %s\n", pid);
                } else {
                    char read[1024];
                    int bytes_received = recv(i, read, 1024, 0);
                    
                    
                    if (bytes_received < 1) {
                        FD_CLR(i, &master);
                        CLOSESOCKET(i);
                        continue;
                    }
                    
                    SOCKET j;
                    for (j = 1; j <= max_socket; ++j) {
                        if (FD_ISSET(j, &master)) {
                            if (j == socket_listen || j == i) continue;
                            else send(j, read, bytes_received, 0);
                        }
                    }
                }
            }
            if (client_count > 1) {
                printf("스케줄러 동작\n");
                round_robin(client_processes, client_count, 2); 
            }
        }
    }

    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);

    printf("Finished.\n");

    return 0;
}