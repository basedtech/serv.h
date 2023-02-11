#ifndef SERV_MAIN_H
#define SERV_MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_CONNECTION 500
#define RESP_BUF_MAX 10000

#define SEND_BUF_MAX 200000000

enum ServErr {
	SERV_ERR_404 = -1,
	SERV_ERR_500 = -2,
};

// Return a malloc'd buffer from this function, and set *length
char *serv_recieve(char *url, int *length);

#ifndef WIN32

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

char *serv_get_mime(char *url) {
	char mime[32];
	int c = 0;
	while (*url != '.') {
		url++;
		if (*mime == '\0') {
			return "text/html";
		}
	}

	url++;

	while (isalpha(*url)) {
		mime[c] = *url;
		c++;
		url++;
		if (mime[c] == '\0') break;
	}

	mime[c] = '\0';

	if (!strcmp(mime, "js")) {
		return "text/javascript";
	} else if (!strcmp(mime, "xml")) {
		return "image/svg+xml";
	} else if (!strcmp(mime, "png")) {
		return "image/png";
	} else if (!strcmp(mime, "css")) {
		return "text/css";
	} else if (!strcmp(mime, "jpg")) {
		return "image/jpeg";
	} else {
		return "text/html";
	}
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

	char *mime = serv_get_mime(resp);
	printf("%s\n", mime);

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
#endif // ifndef WIN32

#ifdef WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <http.h>
#include <stdio.h>

void InitHttpResponse(HTTP_RESPONSE *r, USHORT status, PSTR reason) {
	RtlZeroMemory(r, sizeof(*r));
	r->StatusCode = status;
	r->pReason = reason;
	r->ReasonLength = (USHORT)strlen(reason);
}

void AddKnownHeaders(HTTP_RESPONSE r, int id, PSTR str) {
	r.Headers.KnownHeaders[id].pRawValue = str;
	r.Headers.KnownHeaders[id].RawValueLength = (USHORT)strlen(str);
}

DWORD SendHttpResponse(HANDLE hReqQueue, PHTTP_REQUEST pRequest, USHORT StatusCode, PSTR pReason, PSTR pEntityString, int length) {
	HTTP_RESPONSE response;
	InitHttpResponse(&response, StatusCode, pReason);

	char *mime = serv_get_mime(pRequest->CookedUrl.pAbsPath);

	AddKnownHeaders(response, HttpHeaderContentType, "text/html");

	HTTP_DATA_CHUNK dataChunk;
	dataChunk.DataChunkType = HttpDataChunkFromMemory;
	dataChunk.FromMemory.pBuffer = pEntityString;
	dataChunk.FromMemory.BufferLength = (ULONG)length;

	response.EntityChunkCount = 1;
	response.pEntityChunks = &dataChunk;

	DWORD bytesSent;
	HRESULT r = HttpSendHttpResponse(
		hReqQueue,
		pRequest->RequestId,
		0,
		&response,
		NULL,
		&bytesSent,
		NULL,
		0,
		NULL,
		NULL
	);

	return r;
}

int SwitchHttpRequest(HANDLE hReqQueue, PHTTP_REQUEST pRequest) {
	char path[256];
	wcstombs(path, pRequest->CookedUrl.pAbsPath, sizeof(path));

	int respCode = 0;
	char *respReason = 0;

	if (pRequest->Verb == HttpVerbGET) {
		int length = 0;
		char *content = serv_recieve(path, &length);

		respCode = 200;
		respReason = "OK";

		if (content == NULL) {
			respCode = 500;
			respReason = "Internal Server Error";
			content = respReason;
			length = strlen(content);
		} else if (length == SERV_ERR_404) {
			respCode = 404;
			respReason = "Not Found";
			content = respReason;
			length = strlen(content);			
		} else if (length == 0) {
			length = strlen(content);
		}

		HRESULT r = SendHttpResponse(
			hReqQueue,
			pRequest,
			respCode,
			respReason,
			content,
			length
		);

		free(content);
	}
}

#define WIN_REQ_SIZE 100000

int serv_start(int port) {
	HTTPAPI_VERSION HttpApiVersion = HTTPAPI_VERSION_1;
	HRESULT r = HttpInitialize(HttpApiVersion, HTTP_INITIALIZE_SERVER, NULL);
	if (r != NO_ERROR) {
		puts("Failed to initialize");
		return 1;
	}

	HANDLE hReqQueue = NULL;
	r = HttpCreateHttpHandle(&hReqQueue, 0);
	if (r != NO_ERROR) {
		puts("Failed to create handle");
		return 1;
	}

	wchar_t url[128];
	swprintf(url, sizeof(url), L"http://localhost:%d/", 1234);
	r = HttpAddUrl(hReqQueue, url, NULL);
	if (r != NO_ERROR) {
		puts("Failed to add url");
		return 1;
	}

	printf("Listening on %d\n", port);

	PHTTP_REQUEST pRequest;
	PCHAR pRequestBuffer;

	pRequestBuffer = malloc(REQ_SIZE);

	pRequest = (PHTTP_REQUEST)pRequestBuffer;

	HTTP_REQUEST_ID requestId;
	HTTP_SET_NULL_ID(&requestId);
	DWORD bytesRead;
	while (1) {
		memset(pRequest, 0, REQ_SIZE);

		r = HttpReceiveHttpRequest(
			hReqQueue,
			requestId,
			0,
			pRequest,
			REQ_SIZE,
			&bytesRead,
			NULL
		);

		if (r == NO_ERROR) {
			SwitchHttpRequest(hReqQueue, pRequest);
		} else if (r == ERROR_MORE_DATA) {
			puts("More data");
		} else if (r == ERROR_CONNECTION_INVALID) {
			puts("Invalid connection");
		} else {
			printf("Failed to recieve request: %d\n", r);
			return 1;
		}
	}
}

#endif // ifdef WIN32

#endif
