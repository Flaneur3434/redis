#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void die(const char *msg) {
	int err = errno;
	fprintf(stderr, "[%d] %s\n", err, msg);
	abort();
}

static void msg(const char *msg) {
	fprintf(stderr, "%s\n", msg);
}

static void do_something(int connfd) {
	char rbuf[64] = {};
	ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
	if (n < 0) {
		msg("read() error");
		return;
	}
	printf("client says: %s\n", rbuf);

	char wbuf[] = "world";
	write(connfd, wbuf, strlen(wbuf));
}

auto main () -> int {
	// AF_INET is for IPv4
	// AF_INET6 is for IPv6
	// SOCK_STREAM is for TCP
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		die("socket()");
	}

	int val = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	// Initialize the socket address structure
	struct sockaddr_in addr{
		.sin_family = AF_INET,
		.sin_port = ntohs(1234),
		.sin_addr = {.s_addr = INADDR_ANY},
		.sin_zero = {0},
	};

	// Bind the socket to the address
	int rv = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if (rv < 0) {
		die("bind()");
	}

	rv = listen(sockfd, SOMAXCONN);
	if (rv < 0) {
		die("listen()");
	}

	while (true) {
		struct sockaddr_in client_addr = {};
		socklen_t socklen = sizeof(client_addr);
		int connfd = accept(sockfd, (struct sockaddr *)&client_addr, &socklen);
		if (connfd < 0) {
			// error
			continue;
		}

		do_something(connfd);
		close(connfd);
	}

	return 0;
}
