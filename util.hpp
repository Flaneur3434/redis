#pragma once

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <string>
#include <concepts>

const size_t k_max_msg = 4096;

void die(const char *msg) {
	int err = errno;
	fprintf(stderr, "[%d] %s\n", err, msg);
	abort();
}

void msg(const char *msg) {
	fprintf(stderr, "%s\n", msg);
}

template <typename T>
requires std::convertible_to<T, bool>
void assert_p(const std::string& message, const T& value) {
	if (!value) {
		std::cerr << "Assertion failed: " << message << std::endl;
		__builtin_trap();
	}
}

int32_t read_full(int fd, char *buf, size_t n) {
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

int32_t write_all(int fd, const char *buf, size_t n) {
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
