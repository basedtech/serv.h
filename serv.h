#ifndef SERV_MAIN_H
#define SERV_MAIN_H

#define MAX_CONNECTION 500
#define RESP_BUF_MAX 10000

#define SEND_BUF_MAX 200000000

enum ServErr {
	SERV_ERR_404 = -1,
	SERV_ERR_500 = -2,
};

// Return a malloc'd buffer from this function, and set *length
char *serv_recieve(char *url, int *length);

#ifdef __linux__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>

#include "serv.h"

int serv_init(int port, int *listenfd) {
	char port_s[16];
	snprintf(port_s, 16, "%u", port);

	printf("Starting on http://127.0.0.1:%s\n", port_s);

	struct addrinfo hints, *res;

	// Pre set to empty connections
	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, port_s, &hints, &res)) {
		perror("getaddrinfo() failed\n");
		return 1;
	}

	for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
		int option = 1;
	
		*listenfd = socket(p->ai_family, p->ai_socktype, 0);
		setsockopt(*listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
		if (*listenfd == -1) {
			continue;
		}

		if (!bind(*listenfd, p->ai_addr, p->ai_addrlen)) {
			break;
		}
	}

	freeaddrinfo(res);

	if (listen(*listenfd, MAX_CONNECTION)) {
		perror("listen() error");
		return 1;
	}

	return 0;
}

int send_response(int fd, char *header, char *content_type, char *body, int content_length) {
	char *response = malloc(SEND_BUF_MAX);
	if (response == NULL) {
		return 0;
	}

	if (body == NULL) {
		body = "Error";
		header = "HTTP/1.1 404 OK";
	}

	// Detect JPEG data (if not already detected)
	if (body[0] == (char)0xFF && body[1] == (char)0xD8) {
		content_type = "image/jpeg";
	}

	int of = snprintf(response, SEND_BUF_MAX,
		"%s\n"
		"Content-Length: %d\n"
		"Content-Type: %s\n"
		"Connection: close\n"
		"\n",
		header, content_length, content_type
	);

	memcpy(response + of, body, content_length);
	int rv = send(fd, response, of + content_length, 0);

	free(response);

	if (rv < 0) {
		perror("send");
	}

	return rv;
}

// client connection
int respond(int n, int clients[MAX_CONNECTION]) {
	char *resp = malloc(RESP_BUF_MAX);
	int rcvd = recv(clients[n], resp, RESP_BUF_MAX, 0);

	if (rcvd < 0) {
		free(resp);
		perror("recv() error\n");
		return 1;
	} else if (rcvd == 0) {
		free(resp);
		perror("Client disconnected upexpectedly\n");
		return 1;
	}

	// Quickly filter parameters from response dump
	char *url = strtok(resp, "\n");
	while (url != NULL) {
		if (!strncmp(url, "GET ", 4)) {
			url = strtok(url + 4, " ");
			break;
		} else if (!strncmp(url, "POST ", 5)) {
			puts("Don't support POST for now");
			return 1;
		}

		url = strtok(NULL, "\n");
	}

	// Parse out %22 / %20 URL encodings
	for (int i = 0; url[i] != '\0'; i++) {
		if (url[i] == '%') {
			if (url[i + 1] == '2' && url[i + 2] == '2') {
				url[i] = '"';
			} else if (url[i + 1] == '2' && url[i + 2] == '0') {
				url[i] = ' ';
			} else {
				continue;
			}

			// Use magic numbers to shift string back two characters
			memcpy(url + i + 1, url + i + 3, strlen(url + i + 3) + 1);
		} else if (url[i] == '?') {
			// For now, ignore
			url[i] = '\0';
			break;
		}
	}

	// Obtain mimetype by parsing URL
	char *mime = resp;
	while (*mime != '.') {
		mime++;
		if (*mime == '\0') {
			mime = "text/html";
			goto m1;
		}
	}

	mime++;
	m1:;
	if (!strcmp(mime, "js")) {
		mime = "text/javascript";
	} else if (!strcmp(mime, "xml")) {
		mime = "image/svg+xml";
	} else if (!strcmp(mime, "png")) {
		mime = "image/png";
	} else if (!strcmp(mime, "css")) {
		mime = "text/css";
	} else if (!strcmp(mime, "jpg")) {
		mime = "image/jpeg";
	}

	int content_length = 0;
	char *buffer = serv_recieve(url, &content_length);
	if (buffer == NULL) {
		buffer = "Internal server error";
		content_length = strlen(buffer);
	}
	
	if (content_length <= 0) {
		content_length = strlen(buffer);
	}

	if (content_length == SERV_ERR_404) {
		send_response(clients[n], "HTTP/1.1 404 Not Found", mime, buffer, content_length);
	} else if (content_length == SERV_ERR_500) {
		send_response(clients[n], "HTTP/1.1 500 Internal Server Error", mime, buffer, content_length);
	} else {
		send_response(clients[n], "HTTP/1.1 200 OK", mime, buffer, content_length);
	}

	free(resp);
	free(buffer);

	return 0;
}

int serv_start(int port) {
	int clients[MAX_CONNECTION];
	memset(clients, -1, sizeof(int) * MAX_CONNECTION);

	struct sockaddr_in clientaddr;
	int listenfd;

	serv_init(port, &listenfd);

	// Ignore SIGCHLD to avoid zombie threads
	signal(SIGCHLD, SIG_IGN);

	while (1) {
		int slot = 0;

		socklen_t addrlen = sizeof(clientaddr);
		clients[slot] = accept(listenfd, (struct sockaddr *)&clientaddr, &addrlen);

		if (clients[slot] < 0) {
			perror("accept() error");
			return 1;
		} else {
			if (respond(slot, clients)) {
				perror("response() error");
			}

			close(clients[slot]);
			clients[slot] = -1;
		}

		while (clients[slot] != -1) {
			slot = (slot + 1) % MAX_CONNECTION;
		}
	}

	return 0;
}
#endif // ifdef __linux__

#endif
