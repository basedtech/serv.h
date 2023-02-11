#include "serv.h"

char *serv_recieve(char *url, int *length) {
	char *buf = malloc(256);
	sprintf(buf, "Hello, World. url: %s\n", url);
	*length = 0;
	return buf;
}

int main() {
	serv_start(1234);
}
