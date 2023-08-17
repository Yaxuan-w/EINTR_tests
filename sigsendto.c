#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#define PORT 9995

pthread_barrier_t barrier;
static volatile sig_atomic_t sigusr1_received = 0;

static void sig_usr(int signum) {
    if (signum == SIGUSR1) {
        printf("[Server] Received signal %d\n", signum);
        sigusr1_received = 1;
    } else {
        printf("[Client] Received signal %d\n", signum);
    }
}

void* client(void* v) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char *hello = "[Server] Hello from client";
    char buffer[1024] = {0};
    long opt = 1;
    sock = socket(AF_INET, SOCK_DGRAM, 0); // Use SOCK_DGRAM for UDP
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    struct sigaction sa_usr;
    sa_usr.sa_flags = 0;
    sa_usr.sa_handler = sig_usr;

    sigaction(SIGUSR2, &sa_usr, NULL);

    pthread_barrier_wait(&barrier);

    sendto(sock, hello, strlen(hello), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    printf("[Client] Hello message sent\n");

    while (1) {
        struct sockaddr_in sender_addr;
        socklen_t sender_addrlen = sizeof(sender_addr);
        ssize_t send_result;
        while ((send_result = sendto(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender_addr, sender_addrlen)) == -1 && errno == EINTR) {
            if (sigusr1_received) {
                printf("Sending interrupted.\n");
                sigusr1_received = 0;
            }
        }

        if (send_result == -1) {
            perror("sendto");
            return NULL;
        } else {
            printf("Message sent successfully.\n");
        }

        sleep(1); // Sleep to prevent busy loop
    }

    close(sock);
    return NULL;
}

void* server(void* v) {
    int server_fd, new_socket, valread; 
    struct sockaddr_in address; 
    long opt = 1;
    int addrlen = sizeof(address); 
    char buffer[1024] = {0}; 
    char *hello = "[Client] Hello from server"; 
       
    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt"); 
        exit(EXIT_FAILURE); 
    } 
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(PORT); 
       
    // Forcefully attaching socket to the port
    if (bind(server_fd, (struct sockaddr *)&address,  
                                 sizeof(address))<0) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    if (listen(server_fd, 3) < 0) 
    { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    } 

    pthread_barrier_wait(&barrier);

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
                       (socklen_t*)&addrlen))<0) { 
        perror("accept"); 
        exit(EXIT_FAILURE); 
    } 

    // Set signal handler
    struct sigaction sa_usr;
    sa_usr.sa_flags = 0;
    sa_usr.sa_handler = sig_usr;   

    sigaction(SIGUSR1, &sa_usr, NULL);

    valread = recv(new_socket, buffer, 1024, 0);
    printf("%s\n",buffer); 

    /* Continue after SIGUSR2 interruption of recv() */
    sleep(10);
    while(!sigusr1_received) {
        int ret = send(new_socket, hello, strlen(hello), 0);
        if(ret < 0){
            perror("send");
            printf("[!] send %d", ret);
            break;
        } 
    }
     
    printf("[Server] Hello message sent\n"); 
    return NULL;
} 

int main() {
    pthread_t serverthread, clientthread;
    pthread_barrier_init(&barrier, NULL, 2);
    pthread_create(&serverthread, NULL, server, NULL);
    pthread_create(&clientthread, NULL, client, NULL);
    sleep(5);
    pthread_kill(clientthread, SIGUSR2);
    sleep(15);
    pthread_kill(serverthread, SIGUSR1);
    pthread_join(clientthread, NULL);
    pthread_join(serverthread, NULL);
    pthread_barrier_destroy(&barrier);
    return 0;
}
