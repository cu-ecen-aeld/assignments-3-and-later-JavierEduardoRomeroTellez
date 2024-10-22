#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char* argv[]) {
	openlog("writer", LOG_PID | LOG_NDELAY, LOG_USER);

	if(argc < 3){
		syslog(LOG_ERR, "Please provide the 2 arguments");
		exit(EXIT_FAILURE);
	}

	FILE* fd;

	fd = fopen(argv[1], "w");
	
	syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
	fprintf(fd, "%s", argv[2]);

	fclose(fd);	
	closelog();
	return EXIT_SUCCESS;
}

