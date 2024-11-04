#include <stdio.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <syslog.h>

#define BACKLOG 10
#define MYPORT 9000
#define BUFFER_LENGTH 1024
/*
Create a socket based program with name aesdsocket in the “server” directory which:

     a. Is compiled by the “all” and “default” target of a Makefile in the “server” directory and supports cross compilation, placing the executable file in the “server” directory and named aesdsocket.

     b. Opens a stream socket bound to port 9000, failing and returning -1 if any of the socket connection steps fail.

     c. Listens for and accepts a connection

     d. Logs message to the syslog “Accepted connection from xxx” where XXXX is the IP address of the connected client. 
*/

int main(int argc, char *argv[])
{
    int sockfd = 0;
    int status = 0;
    int con_sockfd = 0;
    char client_ip[BUFFER_LENGTH];

    struct addrinfo hints;
    struct addrinfo* res = NULL;
    struct sockaddr_storage client_addr;
    socklen_t addr_size;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, "9000", &hints, &res);

    if(status != 0)
    {
        perror("Failed to get addres.");
        return -1;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sockfd < 0)
    {
        perror("Failed to obtain a socket file descriptor.");
        close(sockfd);
        return -1;
    }

    status = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if(status != 0)
    {
        perror("Failed to get bind the socket.");
        close(sockfd);
        return -1;
    }

    status = listen(sockfd, BACKLOG);
    if(status != 0)
    {
        perror("Failed to get find a client.");
        close(sockfd);
        return -1;
    }

    printf("Listening on port 9000...\n");

    con_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
    if(con_sockfd < -1)
    {
        perror("Failed to get find a client.");
        close(sockfd);
        return -1;
    }


    status = getnameinfo( (struct sockaddr *)&client_addr, addr_size, client_ip, sizeof(client_ip), NULL, 0, NI_NUMERICHOST);
    if (status != 0)
    {
        perror("Failed to get IP address.");
        close(sockfd);
        close(con_sockfd);
        return -1;
    }

    syslog(LOG_INFO, "Accepted connection from %s",client_ip);

    

    int fd = 0;

    fd = open("/var/tmp/aesdsocketdata", O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        perror("Failed to open or create /var/tmp/aesdsocketdata");
        return;
    }


    close(sockfd);
    close(con_sockfd);
    return 0;
}
