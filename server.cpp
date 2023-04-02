#include <stdint.h>
#include <cstdlib>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <poll.h>
#include <vector>
#include <memory>

#include "util.hpp"
#include "server.hpp"
#include "hashmap.hpp"

static void conn_put(std::vector<std::unique_ptr<Conn>> &fd2conn, auto&& conn) noexcept {
	if (fd2conn.size() <= (size_t)conn->fd) {
		fd2conn.resize(conn->fd + 1);
	}

	fd2conn[conn->fd] = std::move(conn);
}

static int32_t accept_new_conn(std::vector<std::unique_ptr<Conn>> &fd2conn, int fd) noexcept {
	// accept
	struct sockaddr_in client_addr = {};
	socklen_t socklen = sizeof(client_addr);
	int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
	if (connfd < 0) {
		msg("accept() error");
		return -1;  // error
	}

	// set the new connection fd to nonblocking mode
	fd_set_nb(connfd);
	// creating the struct Conn

	std::unique_ptr<Conn> conn(new Conn(connfd, STATE_REQ));

	if (!conn) {
		close(connfd);
		return -1;
	}

	conn_put(fd2conn, std::move(conn));

	return 0;
}

static bool try_flush_buffer(const std::unique_ptr<Conn>& conn) noexcept {
	ssize_t rv = 0;
	do {
		size_t remain = conn->wbuf_size - conn->wbuf_sent;
		rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
	} while (rv > 0 && errno == EINTR);

	if (rv < 0 && errno == EAGAIN) {
		// got EAGAIN, stop.
		return false;
	}

	if (rv < 0) {
		msg("write() error");
		conn->state = STATE_END;
		return false;
	}

	conn->wbuf_sent += (size_t)rv;
	assert_p("The write buffer did not flush correctly", conn->wbuf_sent <= conn->wbuf_size);

	if (conn->wbuf_sent == conn->wbuf_size) {
		// response was fully sent, change state back
		conn->state = STATE_REQ;
		conn->wbuf_sent = 0;
		conn->wbuf_size = 0;
		return false;
	}

	// still got some data in wbuf, could try to write again
	return true;
}

static void state_res(const std::unique_ptr<Conn>& conn) noexcept {
	while (try_flush_buffer(conn)) {
		;
	}
}

static uint32_t do_get(
    const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen) noexcept {
	if (!g_map.count(cmd[1])) {
		return RES_NX;
	}

	std::string &val = g_map[cmd[1]];
	assert_p("Check to see if requested data is correct length", val.size() <= k_max_msg);
	memcpy(res, val.data(), val.size());
	*reslen = (uint32_t)val.size();
	return RES_OK;
}

static uint32_t do_set(
    const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen) noexcept {
	(void)res;
	(void)reslen;
	g_map[cmd[1]] = cmd[2];
	return RES_OK;
}

static uint32_t do_del(
    const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen) noexcept {
	(void)res;
	(void)reslen;
	g_map.erase(cmd[1]);
	return RES_OK;
}


static inline bool cmd_is(const std::string &word, const std::string &cmd) noexcept {
	return word == cmd;
}

static int32_t parse_req(
    const uint8_t *data,
    size_t data_len,
    std::vector<std::string>& out
) noexcept {
	/*
	 * +------+-----+------+-----+------+-----+-----+------+
	 * | nstr | len | str1 | len | str2 | ... | len | strn |
	 * +------+-----+------+-----+------+-----+-----+------+
	 */

	if (data_len < 4) {
		return -1;
	}

	uint32_t nstr = 0;
	memcpy(&nstr, &data[0], 4);
	if (nstr > k_max_msg) {
		return -1;
	}

	size_t pos = 4;
	while (nstr--) {
		if (pos + 4 > data_len) {
			return -1;
		}

		uint32_t len = 0;
		memcpy(&len, &data[pos], 4);
		if (pos + 4 + len > data_len) {
			return -1;
		}

		out.push_back(std::string((char *)&data[pos + 4], len));
		pos += 4 + len;
	}

	if (pos != data_len) {
		return -1;  // trailing garbage
	}

	return 0;
}

static int32_t do_request(
    const uint8_t* req,
    uint32_t reqlen,
    uint32_t *rescode,
    uint8_t *res,
    uint32_t *reslen
) noexcept {
	std::vector<std::string> cmd;
	if (parse_req(req, reqlen, cmd) != 0) {
		msg("bad req");
		return -1;
	}

	if (cmd.size() == 2 && cmd_is(cmd[0], "get")) {
		*rescode = do_get(cmd, res, reslen);
	}
	else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {
		*rescode = do_set(cmd, res, reslen);
	} else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) {
		*rescode = do_del(cmd, res, reslen);
	} else {
		// cmd is not recognized
		*rescode = RES_ERR;
		const char *msg = "Unknown cmd";
		strcpy((char *)res, msg);
		*reslen = strlen(msg);
		return 0;
	}
	return 0;
}



static bool try_one_request(const std::unique_ptr<Conn>& conn) noexcept {
	// try to parse a request from the buffer
	if (conn->rbuf_size < 4) {
		// not enough data in the buffer. Will retry in the next iteration
		return false;
	}

	uint32_t len = 0;
	memcpy(&len, &conn->rbuf_head[0], 4);
	if (len > k_max_msg) {
		msg("too long");
		conn->state = STATE_END;
		return false;
	}

	if (4 + len > conn->rbuf_size) {
		// buffer is not large enough for request
		// probably error
		return false;
	}

	// got one request, generate the response
	uint32_t rescode = 0;
	uint32_t wlen = 0;
	int32_t err =
	    do_request(&conn->rbuf_head[4], len, &rescode, &conn->wbuf[4 + 4], &wlen);

	if (err) {
		conn->state = STATE_END;
		return false;
	}

	// generating echoing response
	wlen += 4;
	memcpy(&conn->wbuf[0], &wlen, 4);
	memcpy(&conn->wbuf[4], &rescode, 4);
	conn->wbuf_size = 4 + wlen;

	// remove the request from the buffer.
	size_t remain = conn->rbuf_size - 4 - len;
	if (remain) {
		conn->rbuf_head = &conn->rbuf[4 + len];
	}
	conn->rbuf_size = remain;

	// change state
	conn->state = STATE_RES;
	state_res(conn);

	// continue the outer loop if the request was fully processed
	return (conn->state == STATE_REQ);
}

static bool try_fill_buffer(const std::unique_ptr<Conn>& conn) noexcept {
	// try to fill the buffer
	assert_p("check if buffer is large enough for writing", conn->rbuf_size < conn->rbuf_cap);
	ssize_t rv = 0;
	do {
		size_t cap = conn->rbuf_cap - conn->rbuf_size;
		rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap);
	} while (rv < 0 && errno == EINTR);

	if (rv < 0 && errno == EAGAIN) {
		// got EAGAIN, stop.
		return false;
	}

	if (rv < 0) {
		msg("read() error");
		conn->state = STATE_END;
		return false;
	}

	if (rv == 0) {
		if (conn->rbuf_size > 0) {
			msg("unexpected EOF");
		} else {
			msg("EOF");
		}
		conn->state = STATE_END;
		return false;
	}

	conn->rbuf_size += (size_t)rv;
	assert_p("Check if there is still space left in read buffer", conn->rbuf_size <= conn->rbuf_cap - conn->rbuf_size);

	// Try to process requests one by one.
	// The connection's read buffer could include more than one request
	while (try_one_request(conn)) {
		;
	}

	return (conn->state == STATE_REQ);
}

static void state_req(const std::unique_ptr<Conn>& conn) noexcept {
	while (try_fill_buffer(conn)) {
		;
	}
}

static void connection_io(const std::unique_ptr<Conn>& conn) noexcept {
	switch (conn->state) {
	case STATE_REQ:
		state_req(conn);
		break;
	case STATE_RES:
		state_res(conn);
		break;
	default:
		assert_p("Connection state was an unexpected value", false);
	}
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

	// a map of all client connections, keyed by fd
	std::vector<std::unique_ptr<Conn>> fd2conn;

	// set the listen fd to nonblocking mode
	fd_set_nb(sockfd);

	// the event loop
	std::vector<struct pollfd> poll_args;

	while (true) {
		// prepare the arguments of the poll()
		poll_args.clear();

		// for convenience, the listening fd is put in the first position
		struct pollfd pfd = {
			.fd = sockfd,
			.events = POLLIN,
			.revents = 0,
		};
		poll_args.push_back(std::move(pfd));

		// connection fds
		for (const auto &conn : fd2conn) {
			if (!conn) {
				continue;
			}
			struct pollfd pfd = {};
			pfd.fd = conn->fd;
			pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
			pfd.events = pfd.events | POLLERR;
			poll_args.push_back(std::move(pfd));
		}


		// poll for active fds
		// the timeout argument doesn't matter here
		int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
		if (rv < 0) {
			die("poll");
		}

		// process active connections
		for (size_t i = 1; i < poll_args.size(); ++i) {
			if (poll_args[i].revents) {
				const auto &conn = fd2conn[poll_args[i].fd];
				connection_io(conn);
				if (conn->state == STATE_END) {
					fd2conn.erase(fd2conn.begin() + conn->fd);
				}
			}
		}

		// try to accept a new connection if the listening fd is active
		if (poll_args[0].revents) {
			(void)accept_new_conn(fd2conn, sockfd);
		}
	}

	return 0;
}
