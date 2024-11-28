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
#include <sys/stat.h>
#include <sys/queue.h>
#include <pthread.h>
#include <time.h>

#define BACKLOG 10
#define MYPORT "9000"
#define BUFFER_LENGTH 1024

volatile sig_atomic_t caught_signal = false;
pthread_mutex_t mutex_file = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_list = PTHREAD_MUTEX_INITIALIZER;

struct Node 
{
    int con_sockfd;
    pthread_t thread_id;
    SLIST_ENTRY(Node) entries;

};


SLIST_HEAD(slisthead, Node); // Define the head type
struct slisthead head = SLIST_HEAD_INITIALIZER(head); // Declare and initialize the list head

void signal_handler(int signal_number)
{
    if (signal_number == SIGINT || signal_number == SIGTERM)
    {
        caught_signal = true;

    }
}

void *handle_time(void *args)
{
    char buffer[156];
    time_t rawtime;
    struct tm *timeinfo;
    char* timestamp = "timestamp:";

    strncpy(buffer, timestamp, strlen(timestamp));
    buffer[strlen(timestamp)] = '\0';
    int start_point = strlen(timestamp);

    if (start_point >= sizeof(buffer)) {
        syslog(LOG_ERR, "Buffer size too small for timestamp prefix.");
        return NULL;
    }


    while(!caught_signal)
    {
        for (int i = 0; i < 100; i++) 
        { // 100 x 100ms = 10 seconds
            if (caught_signal) return NULL;
                usleep(100000); // Sleep for 100ms
        }   

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        // Format the date and time into the buffer
        if (strftime(&buffer[start_point], sizeof(buffer) - start_point, "%a, %d %b %Y %H:%M:%S %z", timeinfo) == 0) {
            syslog(LOG_ERR,"Buffer size is too small.\n");
            return NULL;
        }
        int str_len = strlen(buffer);
        

        int fd = open("/var/tmp/aesdsocketdata", O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (fd == -1) {
            syslog(LOG_ERR, "File open failed.");         
            return NULL;
        }

        pthread_mutex_lock(&mutex_file);
        if (write(fd, buffer, str_len) == -1 || write(fd, "\n", 1) == -1) {
            syslog(LOG_ERR, "Write to file failed.");
            pthread_mutex_unlock(&mutex_file);
            close(fd);
            return NULL;
        }
        pthread_mutex_unlock(&mutex_file);
        close(fd);

    }

    return NULL;
}

void * handle_connection(void* args)
{
    char buffer[BUFFER_LENGTH];
    struct Node *client = (struct Node *)args;
    int con_sockfd = client->con_sockfd;
    
    int fd = open("/var/tmp/aesdsocketdata", O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "File open failed.");
        close(con_sockfd);
        
        return NULL;
    }

    pthread_mutex_lock(&mutex_file);
    ssize_t bytes_received;
    while ((bytes_received = recv(con_sockfd, buffer, BUFFER_LENGTH, 0)) > 0) {
        char *newline_pos = strchr(buffer, '\n');
        ssize_t write_size = newline_pos ? newline_pos - buffer + 1 : bytes_received;
        if (write(fd, buffer, write_size) == -1) {
            syslog(LOG_ERR, "Write to file failed.");
            break;
        }
        if (newline_pos) break;
    }
    pthread_mutex_unlock(&mutex_file);
    close(fd);

    fd = open("/var/tmp/aesdsocketdata", O_RDONLY);
    if (fd == -1) {
        syslog(LOG_ERR, "File read failed.");
        close(con_sockfd);
        
        return NULL;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, BUFFER_LENGTH)) > 0) {
        if (send(con_sockfd, buffer, bytes_read, 0) == -1) {
            syslog(LOG_ERR, "Send to client failed.");
            break;
        }
    }

    close(fd);
    close(con_sockfd);

    pthread_mutex_lock(&mutex_list);
    SLIST_REMOVE(&head, client, Node, entries);
    pthread_mutex_unlock(&mutex_list);

    return NULL;

}

int main(int argc, char** argv)
{
    bool daemon_mode = false;
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        daemon_mode = true;
        printf("Daemon mode activated.\n");
    }
    else
    {
        printf("Not in Daemon mode.\n");
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction new_action = {0};
    new_action.sa_handler = signal_handler;
    sigaction(SIGTERM, &new_action, NULL);
    sigaction(SIGINT, &new_action, NULL);

    int sockfd, con_sockfd;
    char client_ip[BUFFER_LENGTH];
    char buffer[BUFFER_LENGTH];
    struct addrinfo hints = {0}, *res = NULL;
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

    // Set SO_REUSEADDR option
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        syslog(LOG_ERR, "setsockopt(SO_REUSEADDR) failed.");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        syslog(LOG_ERR, "Socket bind failed.");
        freeaddrinfo(res);
        close(sockfd);
        return -1;
    }

    freeaddrinfo(res);  // Ensure memory is freed once `bind` succeeds

    // Fork the process if in daemon mode
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            printf("Fork failed. Exiting process now.\n");
            close(sockfd);
            return -1;
        } else if (pid > 0) {
            // Parent process exits
            close(sockfd);
            printf("Fork Successful. Child pid: %d, terminating parent process now.\n", pid);
            return 0;
        }

        // Child process (daemon)
        umask(0);
        if (setsid() < 0) {
            syslog(LOG_ERR, "Failed to set new session.");
            close(sockfd);
            return -1;
        }
        
        // Redirect standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    listen(sockfd, BACKLOG);
    syslog(LOG_INFO, "Listening on port %s...", MYPORT);

    pthread_t timer_thread;
    if(pthread_create(&timer_thread, NULL, handle_time, NULL) != 0)
    {
        syslog(LOG_ERR, "Could not start timer thread.");
        close(sockfd);
        return -1;
    }

    while (!caught_signal) 
    {
        
        con_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (con_sockfd < 0) {
            syslog(LOG_ERR, "Accept failed.");
            continue;
        }

        if (getnameinfo((struct sockaddr *)&client_addr, addr_size, client_ip, sizeof(client_ip), NULL, 0, NI_NUMERICHOST) == 0)
            syslog(LOG_INFO, "Accepted connection from %s", client_ip);
        

        // Create Node for linked list.
        struct Node * tmp = malloc(sizeof(struct Node));
        memset(tmp , 0, sizeof(struct Node));
        tmp->con_sockfd = con_sockfd;

        //Start thread.
        pthread_t thread;
        if(pthread_create(&thread, NULL, handle_connection, tmp) != 0)
        {
            free(tmp);
            close(con_sockfd);
            continue;
        }
        tmp->thread_id = thread;

        // Insert node into the linked list.
        pthread_mutex_lock(&mutex_list);
        SLIST_INSERT_HEAD(&head, tmp, entries);
        pthread_mutex_unlock(&mutex_list);

    }

    syslog(LOG_INFO, "Caught signal, exiting");

    struct Node *tdata;
    SLIST_FOREACH(tdata, &head, entries) {
        pthread_join(tdata->thread_id, NULL);
        free(tdata);
    }

    pthread_join(timer_thread, NULL);

    close(sockfd);
    remove("/var/tmp/aesdsocketdata");
    closelog();
    return 0;
}
