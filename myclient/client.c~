/*
 * client.c
 *
 *  Created on: Feb 28, 2016
 *      Author: devendra
 */

#include "client.h"

int main(int argc, char **argv) {
	/* network related variables and structures */
	int listenfd, err_no;
	struct addrinfo hints, *servinfo, *temp;

	/* check if cmdline args are correct */
	if (argc != 5) {
		fprintf(stderr, "usage: %s <hostname> <port> <command> <filename>\n", argv[0]);
		/* example: ./myclient localhost 8000 GET/PUT home.html */
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
	if ((err_no = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
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

		/* connect to the remote host */
		if (connect(listenfd, temp->ai_addr, temp->ai_addrlen) == -1) {
			close(listenfd); // close the file descriptor if connect fails
			fprintf(stderr, "Connect Error: %s\n", strerror(errno));
			continue;
		}

		break;
	}

	/* free the linked list structure since we have the required information */
	freeaddrinfo(servinfo);

	if (!strcasecmp(argv[3], "GET")) {
		// printf("DEBUG: %s:Requested GET method\n", argv[3]);
		get_request(listenfd, argv[3], argv[4]);
	} else if (!strcasecmp(argv[3], "PUT")) {
		// printf("DEBUG: %s:Requested PUT method\n", argv[3]);
		put_request(listenfd, argv[3], argv[4]);
	} else {
		printf("Requested Method '%s' Is Not Implemented. Implemented Methods: GET/PUT\n", argv[3]);
	}

	/* Close the client socket connection once the request is served */
	/* int close(int connfd); */
	if (close(listenfd) == -1)
		fprintf(stderr, "Close Error: %s\n", strerror(errno));

	return 0;
}

/* send a GET request to server */
void get_request(int connfd, char method[100], char filename[100]) {
	char request[BUFFSIZE], buffer[BUFFSIZE];
	char *srcp;
	int len, bytes_sent, bytes_recv;

	/* extract the filename without leading '/' */
	get_filename(filename);

	/* make a GET request for server */
	sprintf(request, "%s /%s %s\r\n", method, filename, VERSION);
	sprintf(request, "%sHost: %s\r\n", request, HOST);
	sprintf(request, "%sConnection: Keep-Alive\r\n\r\n", request);

	/* send request to server */
	len = strlen(request);
	if ((bytes_sent = send(connfd, &request, len, 0)) < 0)
		return;

	/* receive the server response */
	sleep(1);	// wait for response
	if ((bytes_recv = recv(connfd, buffer, BUFFSIZE - 1, 0)) == -1) {
		fprintf(stderr, "Receive Error: %s\n", strerror(errno));
		return;
	}

	// printf("DEBUG: Server response:\n%s\n", buffer);

	FILE *fp = fopen(filename, "w"); // open an existing file descriptor or create new in writing/over-writing mode
	srcp = strstr(buffer, EOR) + strlen(EOR); // adjust the pointer to skip the header and point at actual data

	// printf("DEBUG: The substring is:\n%s\n", srcp);

	fprintf(fp, "%s", srcp); // write the data into the opened file descriptor
	fclose(fp); // close the file descriptor
}

/* send a PUT request to server */
void put_request(int connfd, char method[100], char filename[100]) {
	char filetype[100];
	char request[BUFFSIZE], buffer[BUFFSIZE];
	char *srcp;
	int srcfd, len, bytes_sent, bytes_recv, sent = 0;

	struct stat sbuf;

	/* extract the filetype and filename without leading '/' */
	get_filename(filename);
	get_filetype(filename, filetype);

	/* get the statistics of the file requested by client */
	if (stat(filename, &sbuf) < 0) {
		fprintf(stderr, "File Stat Error: %s\n", strerror(errno));
	} else {
		/* make a PUT request for server */
		sprintf(request, "%s /%s %s\r\n", method, filename, VERSION);
		sprintf(request, "%sHost: %s\r\n", request, HOST);
		sprintf(request, "%sConnection: Keep-Alive\r\n", request);
		sprintf(request, "%sContent-type: %s\r\n", request, filetype);
		sprintf(request, "%sContent-Length: %d\r\n\r\n", request, (int) sbuf.st_size);

		// printf("DEBUG: send request: %s\n", request);

		/* send request to server */
		len = strlen(request);
		if ((bytes_sent = send(connfd, &request, len, 0)) < 0)
			return;

		/* receive the server response */
		sleep(1);	// wait for response
		if ((bytes_recv = recv(connfd, buffer, BUFFSIZE - 1, 0)) == -1) {
			fprintf(stderr, "Receive Error: %s\n", strerror(errno));
			return;
		}

		// printf("DEBUG: recv buffer: %s\n", buffer);

		if (strstr(buffer, OK)) {
			/* open a file descriptor (srcfd) for the file requested by client */
			if ((srcfd = open(filename, O_RDONLY, 0)) < 0)
				fprintf(stderr, "File Open Error: %s\n", strerror(errno));
			/* create a new mapping for the local file descriptor (srcfd) */
			if ((srcp = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, srcfd, 0)) == ((void *) -1))
				fprintf(stderr, "Memory Map Error: %s\n", strerror(errno));
			close(srcfd); // close the local fd

			// printf("DEBUG: sending file: %s\n", srcp);

			/* Send requested file to client from the new mapping (srcp) */
			sent = 0;
			while (sent < sbuf.st_size) {
				if ((bytes_sent = send(connfd, srcp, sbuf.st_size, 0)) < 0)
					fprintf(stderr, "File Send Error: %s\n", strerror(errno));
				sent += bytes_sent;
			}
			munmap(srcp, sbuf.st_size); // free the new mapping (srcp) created for the local fd
		} else {
			srcp = strstr(buffer, EOR) + strlen(EOR);
			printf("Server Error:\n%s\n", srcp);
		}
	}
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

/* extract the filename (if leading with '/') and filetype */
void get_filename(char *filename) {
	char temp[100];

	if (filename[0] == '/') {
		int i;
		for (i = 0; i < strlen(filename); i++) {
			temp[i] = filename[i + 1];
			filename[i] = temp[i];
			temp[i] = 0;
			if (filename[i + 1] == '\0')
				break;
		}
	}
}

