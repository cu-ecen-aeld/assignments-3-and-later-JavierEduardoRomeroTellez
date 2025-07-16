# include <sys/socket.h>
# include <sys/types.h>
# include <unistd.h>
# include <stdlib.h>
# include <stdio.h>
# include <netinet/in.h>
# include <arpa/inet.h>

# include <syslog.h>

# include <errno.h>
# include <string.h>

int listen_and_accept_con(int fd, char *addr)
{
    struct sockaddr clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int rc = listen(fd, 1);
    if (rc != 0) {
        printf("%s\n", strerror(errno));
        return -1;
    }
    int client = accept(fd, &clientAddr, &clientAddrLen);
    if (client == -1) {
        printf("%s\n", strerror(errno));
        return -1;
    }
    char *ip = inet_ntoa(((struct sockaddr_in*)&clientAddr)->sin_addr);
    memcpy(addr, ip, 14);
    syslog(LOG_INFO, "Accepted connection from %s", addr);
    return client;
}

static int is_packet_complete(char *buffer, int size)
{
    for (int i = 0; i < size; i++)
        if (buffer[i] == '\n')
            return 1;
    return 0;
}

char *receive_data(int client)
{
    int bufferLength = 1024;
    ssize_t totalReadedByte = 0;
    char *buffer = malloc(sizeof(char) * bufferLength);

    if (!buffer) {
        printf("Out of memory!");
        return NULL;
    }

    memset(buffer, 0, bufferLength);
    ssize_t readedByte = recv(client, buffer, bufferLength, 0);
    totalReadedByte += readedByte;
    int i = 0;
    while (readedByte !=  0) {
        if (is_packet_complete(buffer, totalReadedByte) == 1)
            break;
        
        bufferLength += 1024;
        buffer = realloc(buffer, sizeof(char) * bufferLength);
        if(buffer == NULL) {
            printf("Out of memory!");
            return NULL;
        }
        memset(&(buffer[totalReadedByte]), 0, 1024);
        readedByte = recv(client, &(buffer[totalReadedByte]), 1024, 0);
        totalReadedByte += readedByte;
        i++;
    }
    return buffer;
}
