#include "util.hpp"

void die(const char *msg) noexcept {
	int err = errno;
	fprintf(stderr, "[%d] %s\n", err, msg);
	abort();
}

void msg(const char *msg) noexcept {
	fprintf(stderr, "%s\n", msg);
}

template <typename T>
requires std::convertible_to<T, bool>
void assert_p(const std::string& message, const T& value) noexcept {
	if (!value) {
		std::cerr << "Assertion failed: " << message << std::endl;
		__builtin_trap();
	}
}

int32_t read_full(int fd, char *buf, size_t n) noexcept {
	while (n > 0) {
		ssize_t rv = read(fd, buf, n);
		if (rv <= 0) {
			return -1;  // error, or unexpected EOF
		}
		assert_p("checking to see if read length is under the limit", (size_t)rv <= n);
		n -= (size_t)rv;
		buf += rv;
	}
	return 0;
}

int32_t write_all(int fd, const char *buf, size_t n) noexcept {
	while (n > 0) {
		ssize_t rv = write(fd, buf, n);
		if (rv <= 0) {
			return -1;  // error
		}
		assert_p("checking to see if write length is under the limit", (size_t)rv <= n);
		n -= (size_t)rv;
		buf += rv;
	}
	return 0;
}

void fd_set_nb(int fd) noexcept {
	errno = 0;
	int flags = fcntl(fd, F_GETFL, 0);
	if (errno) {
		die("fcntl error");
		return;
	}

	flags |= O_NONBLOCK;

	errno = 0;
	(void)fcntl(fd, F_SETFL, flags);
	if (errno) {
		die("fcntl error");
	}
}
