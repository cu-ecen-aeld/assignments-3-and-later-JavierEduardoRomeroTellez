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

#define PORT "9000"

#define BACKLOG 10
#define MAXBUFFERSIZE 20000
#define MAXREADSIZE 1000
bool exit_program = false;

void sigchld_handler(int s)
{
	syslog(LOG_INFO, "Caught signal, exiting");

	exit_program = true;

	printf("\n Server: exiting on signal \n");
	remove("/var/tmp/aesdsocketdata");
}

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, new_fd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	char buff[MAXBUFFERSIZE];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	int numbytes;
	uint rec_size = 0;
	size_t read_b = 0;
	FILE *fptr;

	fprintf(stderr, "Server: argc = %d\n", argc);

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

	if (argc > 1)
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

	while (!exit_program)
	{
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1)
		{
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
				  get_in_addr((struct sockaddr *)&their_addr),
				  s, sizeof s);
		syslog(LOG_INFO, "Connection from %s\n", s);
		printf("server: got connection from %s\n", s);

		fptr = fopen("/var/tmp/aesdsocketdata", "a+");
		perror("Open file");

		while (true)
		{
			if ((numbytes = recv(new_fd, &buff[rec_size], MAXREADSIZE - 1, 0)) == -1)
			{
				perror("recv");
				exit(1);
			}

			printf("Server: received %d bytes\n", numbytes);
			rec_size = rec_size + (numbytes);

			if (buff[rec_size - 1] == '\n')
			{
				fwrite(buff, sizeof(char), rec_size, fptr);
				printf("Server: saving %d bytes \n", numbytes);
				fflush(fptr);
				memset(buff, 0, sizeof(buff));
				rec_size = 0;

				fseek(fptr, 0, SEEK_SET);

				while (true)
				{
					read_b = fread(buff, sizeof(char), MAXREADSIZE, fptr);

					if (read_b == 0)
					{
						perror("fread");
						break;
					}
					else
					{
						if (send(new_fd, buff, read_b, 0) == -1)
						{
							perror("send");
						}
						printf("Server: sending more bytes %d\n", (int)read_b);
					}
				}

				syslog(LOG_INFO, "Closed connection from %s\n", s);

				close(new_fd);
				fclose(fptr);
				break;
			}
			else if (0 == numbytes)
			{
				syslog(LOG_INFO, "Closed connection from %s\n", s);
				printf("Server: received %d bytes \n", numbytes);
				break;
			}
			else
			{
				printf("Server: received %d bytes, waiting for more \n", numbytes);
			}
		}
	}

	printf("Server: Shuting down \n");
	close(sockfd);
	close(new_fd);
	return 0;
}
