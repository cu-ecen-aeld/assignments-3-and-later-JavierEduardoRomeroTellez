# include <unistd.h>
# include <fcntl.h>
# include <stdio.h>
# include <stdlib.h>

# include <errno.h>
# include <string.h>

int open_file()
{
    int fd = open("/var/tmp/aesdsocketdata", O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    return fd;
}

char *read_file(int fd)
{
    char *data = NULL;

    off_t fileSize = lseek(fd, 0, SEEK_END);
    if(fileSize == -1) {
        printf("%s", strerror(errno));
        return NULL;
    }

    off_t offset = lseek(fd, 0, SEEK_SET);
    if(offset != 0) {
        printf("%s", strerror(errno));
        return NULL;
    }
    data = malloc(sizeof(char) * fileSize + 1);
    if (!data) {
        printf("Out of memory!");
        return NULL;
    }
    if (read(fd, data, fileSize) == -1) {
        printf("%s", strerror(errno));
        free(data);
        return NULL;
    }
    data[fileSize] = 0;

    return data;
}
