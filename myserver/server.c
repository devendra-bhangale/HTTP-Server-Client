/*
 * server.c
 *
 *  Created on: Feb 26, 2016
 *      Author: devendra
 */

#include "server.h"

int main(int argc, char **argv) {
	signal(SIGINT, sigproc); // sigproc() is called when user hits 'ctrl-c'
	signal(SIGQUIT, quitproc); // quitproc() is called when user hits 'ctrl-\'
	printf("\n\nWARNING: 'ctrl-c' IS DISABLED; USE 'ctrl-\\' TO QUIT.\n\n");

	/* multithread related variables and structures */
	int policy;

	struct thread_args *args;
	struct sched_param my_param;

	pthread_t workers[NUM_THREAD];
	pthread_attr_t lp_attr;

	/* network related variables and structures */
	int listenfd, connfd, clientlen, err_no, yes = 1;

	struct sockaddr_in clientaddr;
	struct addrinfo hints, *servinfo, *temp;

	/* check if cmdline args are correct */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	/* alternate method for bzero:	memset(&hints, 0, sizeof(hints)); */
	bzero((char *) &hints, sizeof(hints));

	/* set basic network parameters */
	hints.ai_family = AF_UNSPEC; // IP protocol: IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // SOCK_STREAM (1) or SOCK DGRAM (2)
	hints.ai_flags = AI_PASSIVE; // use host IP

	/* get the linked list with network information */
	/* int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res); */
	if ((err_no = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "Getaddrinfo Error: %s\n", gai_strerror(err_no));
		return -1;
	}

	/* loop through the 'servinfo' linked list and bind to the first possible socket */
	for (temp = servinfo; temp != NULL; temp = temp->ai_next) {
		/* Create a socket - file descriptor */
		/* int socket(int domain, int type, int protocol); * domain: IP protocol(2); * type: SOCK_STREAM(1); SOCK_DGRAM(2) * protocol: tcp(6); udp(17)*/
		if ((listenfd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol)) == -1) {
			fprintf(stderr, "Socket Error: %s\n", strerror(errno));
			continue;
		}

		/* Eliminates "Address already in use" error from bind */
		/* int setsockopt(int sockfd, int level, int optionname, const void *optionval, socklen_t optionlen); */
		if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &yes, sizeof(int)) == -1) {
			fprintf(stderr, "Setsockopt Error: %s\n", strerror(errno));
			continue;
		}

		/* Listenfd will be an endpoint for all requests to port on any IP address for this host */
		/* int bind(int sockfd, struct sockaddr *my_addr, int addrlen); */
		if (bind(listenfd, temp->ai_addr, temp->ai_addrlen) == -1) {
			close(listenfd); // close the file descriptor if bind fails
			fprintf(stderr, "Bind Error: %s\n", strerror(errno));
			continue;
		}

		break;
	}

	/* free the linked list structure since we have the required information */
	freeaddrinfo(servinfo);

	/* Make a listening socket ready to accept connection requests */
	/* int listen(int sockfd, int queue); */
	if (listen(listenfd, LISTENQ) == -1) {
		fprintf(stderr, "Listen Error: %s\n", strerror(errno));
		return -1;
	}

	/* add the listenfd socket fd to the linked list 'socfd' of opened connections as a root connection */
	root = (struct socfd *) malloc(sizeof(struct socfd));
	root->socketfd = listenfd; // socket connection returned by socket() is add as root connection to the linked list 'socfd'
	root->next = 0; // pointer to next connection is initialized as null

	// printf("DEBUG: listenfd: %d\n", listenfd);

	/* MAIN-THREAD WITH MAX PRIORITY */
	my_param.sched_priority = sched_get_priority_max(SCHED_FIFO); // The max priority value according to SCHED_FIFO policy
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &my_param); // set MAIN thread scheduling parameters according to SHED_FIFO policy
	pthread_getschedparam(pthread_self(), &policy, &my_param); // get the scheduling parameters of MAIN thread into my_param

	/* SCHEDULING POLICY AND PRIORITY FOR WORKER THREADS */
	my_param.sched_priority = sched_get_priority_min(SCHED_FIFO); // The min priority value according to SCHED_FIFO policy
	pthread_attr_init(&lp_attr); // initialize thread attributes
	pthread_attr_setinheritsched(&lp_attr, PTHREAD_EXPLICIT_SCHED); // set scheduling inheritance mode as EXPLICIT
	pthread_attr_setschedpolicy(&lp_attr, SCHED_FIFO); // set scheduling policy according to SCHED_FIFO policy
	pthread_attr_setschedparam(&lp_attr, &my_param); // set thread attributes similar to MAIN thread

	printf("Server ready to accept requests!\n");

	int thread_num = 0;
	while (1) {
		/* wait and accept connections; return a new socket - file descriptor for each new connection */
		/* int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen); */
		clientlen = sizeof(clientaddr);
		if ((connfd = accept(listenfd, (struct sockaddr *) &clientaddr, (socklen_t *) &clientlen)) == -1) {
			fprintf(stderr, "Accept Error: %s\n", strerror(errno));
			continue;
		}

		// printf("DEBUG: new connection accepted\n");

		/* Loop through '0 to NUM_THREAD(20)' and assign each new client request to a newly created thread */
		pthread_mutex_lock(&mutex); // acquire the lock
		/* ALIVE thread suggests that all the server threads are busy and hence can't accept new request */
		if (thread_status[thread_num] == ALIVE) {
			char req[50] = "Server busy! Please try again later!\n";
			int len = strlen(req);

			/* respond to client request with server busy message and terminate the connection */
			if ((send(connfd, &req, len, 0)) < 0)
				fprintf(stderr, "Server Busy Send Error: %s\n", strerror(errno));
			if (close(connfd) == -1)
				fprintf(stderr, "Server Busy Close Error: %s\n", strerror(errno));
		} else {
			/* Allocate new memory each time a request is accepted to pass arguments so that no 2 threads contend for information at same memory location */
			args = malloc(sizeof(struct thread_args));
			args->thread_num = thread_num; // thread number
			args->connfd = connfd; // file descriptor for client connection

			/* Create a new thread to serve the new connection */
			if (pthread_create(&workers[thread_num], &lp_attr, &serve_request, args) != 0)
				fprintf(stderr, "thread create Error: %s\n", strerror(errno));

			thread_status[thread_num] = ALIVE; // assign the thread ALIVE value
			thread_num++;
			thread_num = thread_num % NUM_THREAD; // loop the threads from 0 to 19; since NUM_THREAD=20

			/* add the new socket connection acquired by 'accept' to the linked list of opened connections */
			curr_soc = root;
			if (curr_soc != 0) {
				while (curr_soc->next != 0) { // loop through the linked list 'socfd' upto the last element
					curr_soc = curr_soc->next;
				}
				/* create a new element at last position and initialize it with appropriate values */
				curr_soc->next = (struct socfd *) malloc(sizeof(struct socfd));
				curr_soc = curr_soc->next;
				curr_soc->socketfd = connfd;
				curr_soc->next = 0;
			}
		}
		pthread_mutex_unlock(&mutex); // release lock

	}

	/* Close the server's listening socket before the server shutdown */
	/* int close(int connfd); */
	if (close(listenfd) == -1)
		fprintf(stderr, "Close Error: %s\n", strerror(errno));

	return 0;
}

/* Serving the client request */
void *serve_request(void *param) {
	/* When a detached thread terminates, its resources are automatically released back to the system without the need for another thread to join with the terminated thread */
	pthread_detach(pthread_self());

	char method[100], version[100], temp[100], filename[100], filetype[100];
	char response[BUFFSIZE], buffer[BUFFSIZE];
	char *srcp;
	int file, srcfd, len, bytes_sent, bytes_recv, sent = 0;

	struct stat sbuf;
	struct thread_args *args = param;

	printf("DEBUG: pthread_id%d: %ld\targs->connfd: %d\n", req_count, pthread_self(), args->connfd);

	req_count++;

	/* receive the client request */
	if ((bytes_recv = recv(args->connfd, buffer, BUFFSIZE - 1, 0)) == -1) {
		fprintf(stderr, "Receive Error: %s\n", strerror(errno));
		return NULL;
	}

	//printf("DEBUG: recv client request: %s\n", buffer);

	sscanf(buffer, "%s %s %s", method, filename, version); // parse the client request

	/* extract the filename and filetype */
	int i;
	for (i = 0; i < strlen(filename); i++) {
		temp[i] = filename[i + 1];
		filename[i] = temp[i];
		if (filename[i + 1] == '\0')
			break;
	}
	get_filetype(filename, filetype);

	/* check if client requested GET method request */
	if (!strcasecmp(method, "GET")) {
		printf("Client Request:%s /%s %s\n", method, filename, version);

		/* get the statistics of the file requested by client */
		if (stat(filename, &sbuf) < 0) {
			/* 404 NOT FOUND response if error */
			file = NOTFOUND;
			sprintf(response, "HTTP/1.1 404 NOT FOUND\r\n\r\nRequested file not found. Suggestions: index.html, home.html\n");
			fprintf(stderr, "File Stat Error: %s\n", strerror(errno));
		} else {
			/* 200 OK response if no error */
			file = FOUND;
			sprintf(response, "HTTP/1.1 200 OK\r\n");
			sprintf(response, "%sServer: %s\r\n", response, SERVER);
			sprintf(response, "%sContent-length: %d\r\n", response, (int) sbuf.st_size);
			sprintf(response, "%sContent-type: %s\r\n", response, filetype);
			sprintf(response, "%sConnection: Keep-Alive\r\n\r\n", response);
		}

		/* Send the primary response (404/200)) to the client */
		len = strlen(response);
		if ((bytes_sent = send(args->connfd, &response, len, 0)) < 0)
			return NULL;

		if (file == FOUND) {
			/* open a file descriptor (srcfd) for the file requested by client */
			if ((srcfd = open(filename, O_RDONLY, 0)) < 0)
				fprintf(stderr, "File Open Error: %s\n", strerror(errno));
			/* create a new mapping for the local file descriptor (srcfd) */
			if ((srcp = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, srcfd, 0)) == ((void *) -1))
				fprintf(stderr, "Memory Map Error: %s\n", strerror(errno));
			close(srcfd); // close the local fd

			/* Send requested file to client from the new mapping (srcp) */
			sent = 0;
			while (sent < sbuf.st_size) {
				if ((bytes_sent = send(args->connfd, srcp, sbuf.st_size, 0)) < 0)
					fprintf(stderr, "File Send Error: %s\n", strerror(errno));
				sent += bytes_sent;
			}
			munmap(srcp, sbuf.st_size); // free the new mapping (srcp) created for the local fd
		}
	}
	/* check if client requested PUT method request */
	else if (!strcasecmp(method, "PUT")) {
		printf("Client Request:%s /%s %s\n", method, filename, version);

		srcp = strstr(buffer, GETLENGTH) + strlen(GETLENGTH);
		FILE *fp;
		if ((fp = fopen(filename, "w")) != NULL) {
			sprintf(response, "HTTP/1.1 200 OK\r\n");
			sprintf(response, "%sServer: %s\r\n", response, SERVER);
			sprintf(response, "%sConnection: Keep-Alive\r\n\r\n", response);
		} else {
			sprintf(response, "HTTP/1.1 500 Internal Server Error\r\n\r\nInternal Server Error. File cannot be saved\n");
			fprintf(stderr, "File Create Error: %s\n", strerror(errno));
		}
		fclose(fp);

		//printf("DEBUG: send primary response: %s\n", response);

		/* Send the primary response (404/200)) to the client */
		len = strlen(response);
		if ((bytes_sent = send(args->connfd, &response, len, 0)) < 0)
			return NULL;

		//printf("DEBUG: recv client file\n");

		/* receive the client file */
		bzero((char *) &buffer, sizeof(buffer));
		sleep(1);	// wait for response
		if ((bytes_recv = recv(args->connfd, buffer, BUFFSIZE - 1, 0)) == -1) {
			fprintf(stderr, "Receive Error: %s\n", strerror(errno));
			return NULL;
		}

		//printf("DEBUG: file recvd:\n%s\n", buffer);

		fp = fopen(filename, "w");

		//printf("DEBUG: The substring is:\n%s\nend of substring\n", srcp);

		fprintf(fp, "%s", buffer);
		if (fclose(fp))
			fprintf(stderr, "Close Error: %s\n", strerror(errno));
	}
	/* if client requested UNKNOWN method request */
	else {
		printf("CLient Request:%s /%s %s\n", method, filename, version);
		sprintf(response, "HTTP/1.1 501 NOT IMPLEMENTED\r\n\r\nMethod not implemented. Only GET and PUT methods are supported\n");
		len = strlen(response);
		if ((bytes_sent = send(args->connfd, &response, len, 0)) < 0)
			return NULL;
	}

	/* Close the client socket connection (received from 'accept') once the request is served */
	/* int close(int connfd); */
	if (close(args->connfd) == -1)
		fprintf(stderr, "Close Error: %s\n", strerror(errno));

	pthread_mutex_lock(&mutex); // acquire lock
	thread_status[args->thread_num] = DEAD; // assign the thread DEAD value after serving request

	/* delete the current socket connection from the linked list 'socfd' */
	curr_soc = root;
	if (curr_soc != 0) {
		/* loop through the linked list 'socfd' upto the last element */
		while (curr_soc->next != 0) {
			next_soc = curr_soc->next;
			/* if the next_soc element points to 'connfd', then change the pointer of curr_soc->next to the next-to-next element and delete the next_soc element */
			if (next_soc->socketfd == args->connfd) {
				curr_soc->next = next_soc->next;
				curr_soc = next_soc;
				break;
			}
			curr_soc = next_soc;
		}
		free(curr_soc); // free the memory of the element to be deleted which was acquired by malloc
	}

	pthread_mutex_unlock(&mutex); // release lock

	free(param); // free the allocated memory for paramters
	return 0;
}

/* derive file type from file name */
void get_filetype(char *filename, char *filetype) {
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
	else
		strcpy(filetype, "text/plain");
}

/* called when user hits 'ctrl-c' */
void sigproc(int param) {
	/* NOTE some versions of UNIX will reset signal to default after each call. So for portability reset signal each time */
	signal(SIGINT, sigproc); /*  */
	printf("\n\nWARNING: ctrl-c IS DISABLED; USE ctrl-\\ TO QUIT.\n\n");
}

/* called when user hits 'ctrl-\' and exits gracefully by closing all open connections */
void quitproc(int param) {
	printf("\nPREPARING TO QUIT:\n");
	printf("Closing Open Socket Connections...\n");

	curr_soc = root;
	/* loop through the linked list 'socfd' upto the last element and close connections one-by-one */
	while (curr_soc->next != 0) {
		next_soc = curr_soc->next;
		printf("Closing socket connection: %d\n", curr_soc->socketfd);
		if (close(curr_soc->socketfd) == -1)
			fprintf(stderr, "Close Error: %s\n", strerror(errno));
		free(curr_soc); // free the memory of the element to be deleted which was acquired by malloc
		curr_soc = next_soc;
	}
	/* close the last connection in the linked list 'socfd' */
	printf("Closing socket connection: %d\n", curr_soc->socketfd);
	if (close(curr_soc->socketfd) == -1)
		fprintf(stderr, "Close Error: %s\n", strerror(errno));
	free(curr_soc); // free the memory of the element to be deleted which was acquired by malloc

	exit(0); // normal exit status
}


