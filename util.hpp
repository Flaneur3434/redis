#pragma once

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <string>
#include <concepts>
#include <fcntl.h>
#include <unistd.h>

const size_t k_max_msg = 4096;
void die(const char *msg) noexcept;
void msg(const char *msg) noexcept;

template <typename T>
requires std::convertible_to<T, bool>
void assert_p(const std::string& message, const T& value) noexcept;

int32_t read_full(int fd, char *buf, size_t n) noexcept;
int32_t write_all(int fd, const char *buf, size_t n) noexcept;
void fd_set_nb(int fd) noexcept;
