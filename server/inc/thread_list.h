#ifndef THREAD_LIST_H
# define THREAD_LIST_H

# include <sys/queue.h>
# include <pthread.h>

struct thread_node_s {
    pthread_t id;
    char addr[15];
    int client_fd;
    int done;
    SLIST_ENTRY(thread_node_s) entries;
} thread_node_t;

typedef SLIST_HEAD(head_s, thread_node_s) head_t;

#endif//!THREAD_LIST_H