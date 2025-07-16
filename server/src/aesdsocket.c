# include <sys/socket.h>
# include <netdb.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>

# include <syslog.h>
# include <errno.h>
# include <string.h>

# include <stdint.h>

# include <sys/wait.h>
# include <signal.h>

# include "file_io.h"
# include "thread_list.h"
# include "network.h"

int server = -1;
int fd = -1;
pthread_mutex_t fd_mutex;
head_t head;
pthread_t timestamp_thread;

void signal_handler(int sig)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    struct thread_node_s *data;
    struct thread_node_s *temp = NULL;
threads_cleanup:
    SLIST_FOREACH(data, &head, entries){
        if (data->done != 1) {
            pthread_kill(data->id, SIGABRT);
        }
        pthread_join(data->id, NULL);
        temp = data;
        break;
    }
    if (temp){
        SLIST_REMOVE(&head, temp, thread_node_s, entries);
        free(temp);
        temp = NULL;
        goto threads_cleanup;
    }
    pthread_kill(timestamp_thread, SIGABRT);
    pthread_join(timestamp_thread, NULL);
    if(fd != -1)
        close(fd);
    if (server != -1)
        close(server);
    unlink("/var/tmp/aesdsocketdata");
}

void thread_signal_handler(int sig)
{
    pthread_exit(NULL);
}

int open_socket()
{
    const int enable = 1;
    struct addrinfo hints;
    struct addrinfo *res;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        printf("%s\n", strerror(errno));
        return -1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        printf("%s\n", strerror(errno));
        return -1;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    int rc = getaddrinfo(NULL, "9000", &hints, &res);
    if (rc != 0) {
        printf("%s\n", gai_strerror(rc));
        return -1;
    }

    rc = bind(fd, res->ai_addr, sizeof(struct sockaddr));
    if (rc != 0) {
        printf("%s\n", strerror(errno));
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return fd;
}

void *timestamp_routine(void *threadData)
{
    struct sigaction action;

    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = thread_signal_handler;
    if (sigaction(SIGABRT, &action, NULL) != 0) {
        printf("%s: %s", __FUNCTION__, strerror(errno));
        return NULL;
    }
    time_t t;
    struct tm *tmp;

    while (1) {
        char buffer[1024];
        t = time(NULL);
        tmp = localtime(&t);
        int rc = strftime(buffer, 1024, "timestamp:%a, %d %b %Y %T %z\n", tmp);
        if (rc != 0) {
            pthread_mutex_lock(&fd_mutex);
            if (write(fd, buffer, rc) == -1) {
                printf("%s: %s", __FUNCTION__, strerror(errno));
            }
            pthread_mutex_unlock(&fd_mutex);
        }
        sleep(10);
    }
}

void *connection_routine(void *threadData)
{
    struct sigaction action;

    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = thread_signal_handler;
    if (sigaction(SIGABRT, &action, NULL) != 0) {
        printf("%s: %s", __FUNCTION__, strerror(errno));
        return NULL;
    }
    char *data;
    char *fileData;
    struct thread_node_s* threadInfo = (struct thread_node_s*) threadData;

    data = receive_data(threadInfo->client_fd);
    if (!data) {
        printf("%s: error in receive_data().\n", __FUNCTION__);
        goto close_client;
    }

    pthread_mutex_lock(&fd_mutex);
    if (write(fd, data, strlen(data)) == -1) {
        pthread_mutex_unlock(&fd_mutex);
        printf("%s: %s", __FUNCTION__, strerror(errno));
        goto free_data;
    }
    pthread_mutex_unlock(&fd_mutex);
    free(data);

    pthread_mutex_lock(&fd_mutex);
    fileData = read_file(fd);
    pthread_mutex_unlock(&fd_mutex);
    if (!fileData){
        printf("%s: error in read_file().\n", __FUNCTION__);
        goto free_data;
    }

    if (send(threadInfo->client_fd, fileData, strlen(fileData), 0) == -1) {
        printf("%s: %s", __FUNCTION__, strerror(errno));
        goto free_fileData;
    }
    free(fileData);


    char buff[1024];
    int readByte = recv(threadInfo->client_fd, buff, 1024, 0);
    while (readByte != 0 && readByte != -1) {
        readByte = recv(threadInfo->client_fd, buff, 1024, 0);
    }

    syslog(LOG_INFO, "Closed connection from %s", threadInfo->addr);
    close(threadInfo->client_fd);
    threadInfo->done = 1;
    return NULL;

free_fileData:
    free(fileData);
free_data:
    free(data);
close_client:
    close(threadInfo->client_fd);
    threadInfo->done = 1;
    return NULL;
}

int main(int ac, char **av)
{
    struct sigaction action;

    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signal_handler;
    if (sigaction(SIGTERM, &action, NULL) != 0) {
        printf("%s: %s", __FUNCTION__, strerror(errno));
        return -1;
    }
    if (sigaction(SIGINT, &action, NULL) != 0) {
        printf("%s: %s", __FUNCTION__, strerror(errno));
        return -1;
    }

    fd = open_file();
    if (fd == -1) {
        printf("%s: %s", __FUNCTION__, strerror(errno));
        return -1;
    }

    openlog(NULL, 0, LOG_USER);
    server = open_socket();
    if (server == -1) {
        printf("%s: error in open_socket().\n", __FUNCTION__);
        goto close_file;
    }

    if (ac == 2 && strcmp(av[1], "-d") == 0) {
        printf("Running as a daemon!\n");
        int pid = fork();
        if (pid != 0) {
            exit(0);
        }
    }

    pthread_mutex_init(&fd_mutex, NULL);

    int rc = pthread_create(&timestamp_thread, NULL, timestamp_routine, NULL);
    if (rc != 0) {
        close(fd);
        printf("%s(1.%d): Error in pthread_create with code %d", __FUNCTION__, __LINE__, rc);
        return -1;
    }

    SLIST_INIT(&head);

    while (1) {
        struct thread_node_s *data;
        struct thread_node_s *temp = NULL;
join_threads:
        SLIST_FOREACH(data, &head, entries) {
            if (data->done == 1) {
                temp = data;
                break;
            }
        }
        if (temp) {
            pthread_join(temp->id, NULL);
            SLIST_REMOVE(&head, temp, thread_node_s, entries);
            free(temp);
            temp = NULL;
            goto join_threads;
        }
        char addr[15];
        memset(addr, 0, 15);

        int client_fd = listen_and_accept_con(server, addr);
        if (client_fd == -1) {
            printf("%s: error in listen_and_accept_con().\n", __FUNCTION__);
            goto exit;
        }
        struct thread_node_s *threadInfo = malloc(sizeof(struct thread_node_s));
        threadInfo->done = 0;
        threadInfo->client_fd = client_fd;
        memcpy(threadInfo->addr, addr, 14);
        rc = pthread_create(
            &(threadInfo->id), NULL, connection_routine, (void*)threadInfo);
        if (rc != 0) {
            printf("%s(1.%d): Error in pthread_create with code %d", __FUNCTION__, __LINE__, rc);
            goto exit;
        }
        SLIST_INSERT_HEAD(&head, threadInfo, entries);
    }
    close(server);
    close(fd);
    return 0;
exit:
    close(server);
close_file:
    close(fd);
    return -1;
}
