#pragma once

#include "util.hpp"
#include <unistd.h>

enum {
	STATE_REQ = 0,  // Reading requests
	STATE_RES = 1,  // Sending responses
	STATE_END = 2,  // mark the connection for deletion
};

struct Conn {
	int fd = -1;
	uint32_t state = 0;     // either STATE_REQ or STATE_RES

	// buffer for reading
	size_t rbuf_size = 0;
	size_t rbuf_cap = 4 + k_max_msg;
	uint8_t *rbuf;
	uint8_t *rbuf_head;


	// buffer for writing
	size_t wbuf_size = 0;
	size_t wbuf_cap = 4 + k_max_msg;
	size_t wbuf_sent = 0;
	uint8_t *wbuf;

	// Conn () = default;
	Conn (int fd, uint32_t state) : fd(fd), state(state){
		this->rbuf = static_cast<uint8_t *>(malloc(sizeof(unsigned char) * (4 + k_max_msg)));
		this->rbuf_head = rbuf;
		this->wbuf = static_cast<uint8_t *>(malloc(sizeof(unsigned char) * (4 + k_max_msg)));
		this->rbuf_size = 0;
		this->wbuf_size = 0;
		this->wbuf_sent = 0;
	}

	// !! fix the move constructor
	Conn (Conn &&) = default;

	~Conn() {
		free(this->rbuf);
		free(this->wbuf);
		(void)close(this->fd);
	}
};
