#pragma once

#include <memory>
#include <unistd.h>
#include <map>

enum {
	STATE_REQ = 0,  // Reading requests
	STATE_RES = 1,  // Sending responses
	STATE_END = 2,  // mark the connection for deletion
};

enum {
	RES_OK = 0,
	RES_ERR = 1,
	RES_NX = 2,
};


struct Conn {
	int fd;
	uint32_t state;     // either STATE_REQ or STATE_RES

	// buffer for reading
	size_t rbuf_size;
	size_t rbuf_cap;
	uint8_t *rbuf;
	uint8_t *rbuf_head;


	// buffer for writing
	size_t wbuf_size;
	size_t wbuf_cap;
	size_t wbuf_sent;
	uint8_t *wbuf;

	Conn (int file_descriptor, uint32_t connection_state) :
		fd(file_descriptor), state(connection_state),
		rbuf_size(0), rbuf_cap(4 + k_max_msg),
		wbuf_size(0), wbuf_cap(4 + k_max_msg),
		wbuf_sent(0)
		{
			this->rbuf = (uint8_t *) malloc(sizeof(unsigned char) * (4 + k_max_msg));
			this->wbuf = (uint8_t *) malloc(sizeof(unsigned char) * (4 + k_max_msg));
			this->rbuf_head = this->rbuf;
		}

	//!! fix these copy constructors
	// https://learn.microsoft.com/en-us/cpp/cpp/move-constructors-and-move-assignment-operators-cpp?view=msvc-170
	Conn(const Conn& other) = default;
	Conn& operator=(const Conn& other) {
		if (this == &other) {
			return *this;
		}
	}

	Conn (Conn&& other ) noexcept {
		this->rbuf = other.rbuf;
		this->rbuf_head = other.rbuf;
		this->rbuf_size = other.rbuf_size;
		this->rbuf_cap = 4 + k_max_msg;

		this->wbuf = other.wbuf;
		this->wbuf_size = other.wbuf_size;
		this->wbuf_cap = 4 + k_max_msg;
		this->wbuf_sent = other.wbuf_sent;

		other.rbuf = nullptr;
		other.wbuf = nullptr;
	}

	~Conn() noexcept {
		free(this->rbuf);
		free(this->wbuf);
		(void)close(this->fd);
	}
};

// The data structure for the key space.
std::map<std::string, std::string> g_map;
