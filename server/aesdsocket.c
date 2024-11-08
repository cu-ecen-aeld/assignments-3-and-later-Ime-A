#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#define BACKLOG 10
#define MYPORT "9000"
#define BUFFER_LENGTH 1024

bool caught_sigint = false;
bool caught_sigterm = false;

void signal_handler(int signal_number)
{
    if (signal_number == SIGINT || signal_number == SIGTERM)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        caught_sigint = (signal_number == SIGINT);
        caught_sigterm = (signal_number == SIGTERM);
    }
}

int main(int argc, char** argv)
{

    if(argc )
    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction new_action = {0};
    new_action.sa_handler = signal_handler;
    sigaction(SIGTERM, &new_action, NULL);
    sigaction(SIGINT, &new_action, NULL);

    int sockfd, con_sockfd;
    char client_ip[BUFFER_LENGTH];
    char buffer[BUFFER_LENGTH];
    struct addrinfo hints = {0}, *res;
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, MYPORT, &hints, &res) != 0) {
        syslog(LOG_ERR, "Failed to get address info.");
        return -1;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        syslog(LOG_ERR, "Socket creation failed.");
        freeaddrinfo(res);
        return -1;
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        syslog(LOG_ERR, "Socket bind failed.");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);

    while (!caught_sigint && !caught_sigterm) {
        listen(sockfd, BACKLOG);
        syslog(LOG_INFO, "Listening on port %s...", MYPORT);

        con_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (con_sockfd < 0) continue;

        if (getnameinfo((struct sockaddr *)&client_addr, addr_size, client_ip, sizeof(client_ip), NULL, 0, NI_NUMERICHOST) == 0)
            syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        int fd = open("/var/tmp/aesdsocketdata", O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (fd == -1) {
            syslog(LOG_ERR, "File open failed.");
            close(con_sockfd);
            continue;
        }

        ssize_t bytes_received;
        while ((bytes_received = recv(con_sockfd, buffer, BUFFER_LENGTH, 0)) > 0) {
            char *newline_pos = strchr(buffer, '\n');
            ssize_t write_size = newline_pos ? newline_pos - buffer + 1 : bytes_received;
            write(fd, buffer, write_size);
            if (newline_pos) break;
        }
        close(fd);

        fd = open("/var/tmp/aesdsocketdata", O_RDONLY);
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, BUFFER_LENGTH)) > 0)
            send(con_sockfd, buffer, bytes_read, 0);

        close(fd);
        close(con_sockfd);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    close(sockfd);
    remove("/var/tmp/aesdsocketdata");
    closelog();
    return 0;
}
