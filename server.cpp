#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include "util.hpp"

/*
 * parses the incoming data to the server
 * parses according to made up protocal
 */
static int32_t one_request(int connfd) {
	// 4 bytes header
	char rbuf[4 + k_max_msg + 1];
	errno = 0;
	int32_t err = read_full(connfd, rbuf, 4);
	if (err) {
		if (errno == 0) {
			msg("EOF");
		} else {
			msg("read() error");
		}
		return err;
	}

	uint32_t len = 0;
	memcpy(&len, rbuf, 4);  // assume little endian
	if (len > k_max_msg) {
		msg("too long");
		return -1;
	}

	// request body
	err = read_full(connfd, &rbuf[4], len);
	if (err) {
		msg("read() error");
		return err;
	}

	// do something
	rbuf[4 + len] = '\0';
	printf("client says: %s\n", &rbuf[4]);

	// reply using the same protocol
	const char reply[] = "world";
	char wbuf[4 + sizeof(reply)];
	len = (uint32_t)strlen(reply);
	memcpy(wbuf, &len, 4);
	memcpy(&wbuf[4], reply, len);
	return write_all(connfd, wbuf, 4 + len);
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
		// accept
		struct sockaddr_in client_addr = {};
		socklen_t socklen = sizeof(client_addr);
		int connfd = accept(sockfd, (struct sockaddr *)&client_addr, &socklen);
		if (connfd < 0) {
			continue;   // error
		}

		// only serves one client connection at once
		while (true) {
			int32_t err = one_request(connfd);
			if (err) {
				break;
			}
		}
		close(connfd);
	}

	return 0;
}
