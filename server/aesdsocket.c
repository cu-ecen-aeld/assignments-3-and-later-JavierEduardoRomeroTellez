#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#define PORT "9000"
#define BACKLOG 10
#define MAXBUFFERSIZE 20000
#define MAXREADSIZE 1000
#define FILE_PATH "/var/tmp/aesdsocketdata"
bool exit_program = false;

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct ThreadNode {
	pthread_t thread;
	int client_fd;
	struct ThreadNode *next;
} ThreadNode;

ThreadNode *thread_list_head = NULL;

void sigchld_handler(int s)
{
	syslog(LOG_INFO, "Caught signal, exiting");

	exit_program = true;

	printf("\n Server: exiting on signal \n");
	remove(FILE_PATH);
}

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void *handle_client(void *client_fd_ptr){
	int new_fd = *(int *)client_fd_ptr;
	free(client_fd_ptr);
	char buff[MAXBUFFERSIZE];
	struct sockaddr_storage their_addr;
	socklen_t sin_size = sizeof their_addr;
	getpeername(new_fd, (struct sockaddr *)&their_addr, &sin_size);
	char s[INET6_ADDRSTRLEN];
	inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
	syslog(LOG_INFO, "Connection from %s\n", s);
	printf("Server: got connection from %s\n", s);
	FILE *fptr;
	int numbytes;
	uint rec_size = 0;
	size_t read_b = 0;

	pthread_mutex_lock(&file_mutex);
	fptr = fopen(FILE_PATH, "a+");
	if(fptr == NULL) {
		perror("Open file");
		syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
		close(new_fd);
		pthread_mutex_unlock(&file_mutex);
		pthread_exit(NULL);
	}
	pthread_mutex_unlock(&file_mutex);

	while (true){
		if ((numbytes = recv(new_fd, &buff[rec_size], MAXREADSIZE - 1, 0)) == -1) {
			perror("recv");
			syslog(LOG_ERR, "recv error: %s", strerror(errno));
			break;
		}
		printf("Server: received %d bytes\n", numbytes);
		rec_size += numbytes;

		if (buff[rec_size - 1] == '\n'){
			pthread_mutex_lock(&file_mutex);
			fwrite(buff, sizeof(char), rec_size, fptr);
			fflush(fptr);
			printf("Server: saving %d bytes \n", rec_size);
			memset(buff, 0, sizeof(buff));
			rec_size = 0;

			fseek(fptr, 0, SEEK_SET);
			while (true)
			{
				read_b = fread(buff, sizeof(char), MAXREADSIZE, fptr);
				if (read_b == 0){
					break;
				} else {
					if (send(new_fd, buff, read_b,0) == -1) {
						perror("send");
						syslog(LOG_ERR, "send error: %s", strerror(errno));
						break;
					}
					printf("Server: sending more bytes %d\n", (int)read_b);
				}
			}
			pthread_mutex_unlock(&file_mutex);

			syslog(LOG_INFO, "Closed connection from %s\n", s);
			close(new_fd);
			fclose(fptr);
			break;
		} else if (0 == numbytes) {
			syslog(LOG_INFO, "Closed connection from %s\n", s);
			printf("Server: receivd %d bytes \n", numbytes);
			break;
		} else {
			printf("Server: received %d bytes, waiting for more \n", numbytes);
		}
	}
	close(new_fd);
	fclose(fptr);
	pthread_exit(NULL);
}

void add_thread_node_list(pthread_t thread, int client_fd) {
	ThreadNode *new_node = malloc(sizeof(ThreadNode));
	if(new_node == NULL){
		perror("malloc failed");
		exit(EXIT_FAILURE);
	}
	new_node->thread = thread;
	new_node->client_fd = client_fd;
	new_node->next = NULL;

	pthread_mutex_lock(&list_mutex);
	if(thread_list_head == NULL){
		thread_list_head = new_node;
	} else {
		ThreadNode *current = thread_list_head;
		while (current->next != NULL)
		{
			current = current->next;
		}
		current->next = new_node;
	}
	pthread_mutex_unlock(&list_mutex);
}

void join_and_remove_thread() {
	pthread_mutex_lock(&list_mutex);
	ThreadNode *current = thread_list_head;
	ThreadNode *prev = NULL;

	while (current != NULL){
		int result = pthread_join(current->thread, NULL);
		if (result == 0){
			ThreadNode *temp = current;
			current = current->next;
			if (prev) {
				prev->next = current;
			} else {
				thread_list_head = current;
			}
			free(temp);
		} else if(result == EDEADLK) {
			prev = current;
			current = current->next;
		} else {
			prev = current;
			current = current->next;
		}
	}
	pthread_mutex_unlock(&list_mutex);
}

void *timestamp_thread(void *arg){
	FILE *fptr;
	time_t rawtime;
	struct tm *info;
	char buffer[80];

	while (!exit_program) {
		pthread_mutex_lock(&file_mutex);
		fptr = fopen(FILE_PATH, "a+");
		if (fptr != NULL) {
			time(&rawtime);
			info = localtime(&rawtime);
			strftime(buffer, sizeof(buffer), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", info);
			fprintf(fptr, "%s", buffer);
			fclose(fptr);
		}
		pthread_mutex_unlock(&file_mutex);
		sleep(10);
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct sigaction sa;
	int yes = 1;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	openlog(NULL, 0, LOG_USER);

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			perror("setsockopt");
			exit(-1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo);

	if (p == NULL)
	{
		fprintf(stderr, "server: failed to bind\n");
		exit(-1);
	}

	if (argc > 1 && strcmp(argv[1], "-d") == 0)
	{
		int pid = fork();
		if (pid == -1)
		{
			perror("fork");
			exit(-1);
		}
		else if (pid == 0)
		{
			fprintf(stderr, "Server: child created \n");
		}
		else
		{
			return 0;
		}
	}

	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("listen");
		exit(-1);
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction((SIGINT), &sa, NULL) == -1)
	{
		perror("SIGINT");
		exit(-1);
	}

	if (sigaction((SIGTERM), &sa, NULL) == -1)
	{
		perror("SIGTERM");
		exit(-1);
	}

	printf("server: waiting for connections...\n");

	pthread_t timestamp_thead_id;
	if(pthread_create(&timestamp_thead_id, NULL, timestamp_thread, NULL) != 0) {
		perror("pthread_create timestamp thread");
		exit(-1);
	}

	while (!exit_program)
	{
		struct sockaddr_storage their_addr;
		socklen_t sin_size = sizeof their_addr;
		int *new_fd_ptr = malloc(sizeof(int));
		*new_fd_ptr = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

		if(*new_fd_ptr == -1){
			perror("accept");
			free(new_fd_ptr);
			continue;
		}

		pthread_t client_thread;
		if (pthread_create(&client_thread, NULL, handle_client, (void *)new_fd_ptr) != 0){
			perror("pthread_create");
			close(*new_fd_ptr);
			free(new_fd_ptr);
			continue;
		}
		add_thread_node_list(client_thread, *new_fd_ptr);
		join_and_remove_thread();
	}

	printf("Server: Shuting down \n");
	close(sockfd);

	pthread_join(timestamp_thead_id, NULL);

	while(thread_list_head){
		pthread_join(thread_list_head->thread, NULL);
		ThreadNode *temp = thread_list_head;
		thread_list_head = thread_list_head->next;
		free(temp);
	}

	pthread_mutex_destroy(&file_mutex);
	pthread_mutex_destroy(&list_mutex);
	closelog();

	return 0;
}
