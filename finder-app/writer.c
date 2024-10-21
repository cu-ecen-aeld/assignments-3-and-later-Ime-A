#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#define NO_OF_ARG 3
#define FIRST_ARG 1
#define SECOND_ARG 2

int main(int argc, char *argv[])
{

    openlog(NULL, 0, LOG_USER);
    if (argc != NO_OF_ARG)
    {

        syslog(LOG_ERR, "Exactly two arguments are required\n");
        syslog(LOG_ERR, "Usage: %s <write file path> <string to write>\n", argv[0]);
        syslog(LOG_ERR, "Argc count: %d.\n", argc);
        return 1;
    }

    char *write_to_file = argv[FIRST_ARG];
    char *string_to_write = argv[SECOND_ARG];
    int fd = open(write_to_file, O_WRONLY | O_TRUNC);

    if (fd == -1)
    {
        syslog(LOG_ERR, "File with path %s was not opened.\n", write_to_file);
        return 1;
    }

    syslog(LOG_DEBUG, "Opened file %s \n", write_to_file);

    ssize_t nr = write(fd, string_to_write, strlen(string_to_write));

    if (nr == -1)
    {
        syslog(LOG_ERR, "Was not able to write %s file.\n", string_to_write);
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s \n", string_to_write, write_to_file);
    close(fd);

    return 0;
}