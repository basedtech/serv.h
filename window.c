#include "serv.h"

int digit = 0;

char *serv_recieve(char *url, int *length, int *allocated) {
	char *buf = malloc(256);

	if (!strcmp(url, "/")) {
		snprintf(buf, 256, "Refresh to increment: %d", digit);
		digit++;
	} else {
		snprintf(buf, 256, "404");
	}
	
	*length = 0;
	*allocated = 1;
	return buf;
}

void *backend() {
	return serv_start(1234);
}

int main() {
	char text[] = "Hello, World";
	char title[] = "Hello, World";
	char link[] = "http://127.0.0.1:1234";

	int r = window_server(&backend, title, text, link);
	if (r) {
		puts("Running in CLI mode");
	} else {
		puts("Successful close");
	}
}
