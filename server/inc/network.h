#ifndef NETWORK_H
# define NETWORK_H

int listen_and_accept_con(int fd, char * addr);
char *receive_data(int clinet);


#endif//!NETWORK_H