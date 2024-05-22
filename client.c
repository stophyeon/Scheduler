#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#define BUFSIZE 1024
int main(int argc, char *argv[]) {

    pid_t pid = getpid();
    char pid_str[10];


    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    if (argc < 3) {
        fprintf(stderr, "usage: tcp_client hostname port\n");
        return 1;
    }

    printf("Configuring remote address...\n");
    struct addrinfo hints;
    struct addrinfo hints_sch;
    memset(&hints, 0, sizeof(hints));
    memset(&hints_sch, 0, sizeof(hints_sch));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *peer_address;
    struct addrinfo *sch_address;
    
    if (getaddrinfo(argv[1], argv[2], &hints, &peer_address) || getaddrinfo(argv[1], argv[3], &hints_sch, &sch_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    
    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
   
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
            address_buffer, sizeof(address_buffer),
            service_buffer, sizeof(service_buffer),
            NI_NUMERICHOST);
    
    printf("%s %s\n", address_buffer, service_buffer);
    
    printf("Scheduler address is: ");
    char address_buffer_sch[100];
    char service_buffer_sch[100];
    getnameinfo(sch_address->ai_addr, sch_address->ai_addrlen,
            address_buffer_sch, sizeof(address_buffer_sch),
            service_buffer_sch, sizeof(service_buffer_sch),
            NI_NUMERICHOST);
    printf("%s %s\n", address_buffer_sch, service_buffer_sch);


    printf("Creating socket...\n");
    SOCKET socket_peer;
    SOCKET socket_scheduler;
    socket_peer = socket(peer_address->ai_family,
            peer_address->ai_socktype, peer_address->ai_protocol);
    socket_scheduler = socket(sch_address->ai_family,
            sch_address->ai_socktype, sch_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_peer) || !ISVALIDSOCKET(socket_scheduler)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    
    printf("Connecting...\n");
    if (connect(socket_peer,
                peer_address->ai_addr, peer_address->ai_addrlen)||connect(socket_scheduler,
                sch_address->ai_addr, sch_address->ai_addrlen)) {
        fprintf(stderr, "server connect() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
   

    freeaddrinfo(peer_address);
    freeaddrinfo(sch_address);
    
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    send(socket_scheduler, pid_str, strlen(pid_str), 0);
    printf("pid : %s\n",pid_str);
    printf("Connected.\n");
    
    
    while(1) {

        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_peer, &reads);
        FD_SET(0, &reads);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if (select(socket_peer+1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }
        
    } 
    
    printf("Closing socket...\n");
    CLOSESOCKET(socket_peer);

    printf("Finished.\n");
    return 0;
}
