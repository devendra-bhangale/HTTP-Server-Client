/*
 * server.h
 *
 *  Created on: Feb 27, 2016
 *      Author: devendra
 */

/* LIBRARIES */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

/* MACROS */
#define BUFFSIZE 8192					// max size of buffer
#define VERSION "HTTP/1.1"				// http version
#define SERVER "My Server"				// server name
#define EOR "\r\n\r\n"					// end of response
#define GETLENGTH "Content-Length: "		// get length parameter from header content
#define LISTENQ 1024					// max queue of pending connections
#define NUM_THREAD 20					// number of worker threads to serve client requests
#define ALIVE 1						// worker thread is already busy serving a request and cannot accept new request
#define DEAD 0							// worker thread is free and can accept new request
#define FOUND 1						// if requested file is found
#define NOTFOUND 0						// if requested file is not found

/* STRUCTURES */
struct thread_args {			// Used as argument to serve_request()
	int thread_num;			// Application-defined thread #
	int connfd;				// client socket
};

struct socfd {				// linked list of active socket connections
	int socketfd;			// socket file descriptor
	struct socfd *next;		// pointer to next socket file descriptor
};

/* FUNCTIONS */
void sigproc(int param);		// called when user hits 'ctrl-c'
void quitproc(int param);	// called when user hits 'ctrl-\' and quits gracefully by closing all open connections
void *serve_request(void *arg);		// serves the client request
void get_filetype(char *filename, char *filetype);		// extracts the file-type from its name

/* THREAD LOCKS */
pthread_mutex_t mutex;
pthread_mutex_t service_mutex;

/* GLOBAL VARIABLES AND STRUCTURES*/
struct socfd *root, *curr_soc, *next_soc;

int req_count = 1;		// counter for number of requests served
int thread_status[NUM_THREAD] = { 0 };	// status of worker thread:	0 = DEAD	1 = ALIVE

