/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HEAPTRACE_UTILS_H
#define HEAPTRACE_UTILS_H

#include <cstdio>
#include <cstdarg>

#include <string>

class file_t {
public:
	explicit file_t(const char* filename, const char* mode = "r") {
		fp = fopen(filename, mode);
	}
	~file_t() {
		if (fp) fclose(fp);
	}
	operator FILE*()         { return fp; }
	explicit operator bool() { return fp; }

private:
	FILE *fp;
};

class fmt_string {
public:
	fmt_string()  { }
	~fmt_string() { }
	fmt_string(const fmt_string& s) : str(s.str) { }
	fmt_string(const std::string& s) : str(s)  { }
#define FMT_IMPL					\
	do {						\
		va_list args;				\
		char *ptr;				\
		int ret;				\
		va_start(args, fmt);			\
		ret = vasprintf(&ptr, fmt, args);	\
		this->str = ptr;			\
		free(ptr);				\
		va_end(args);				\
	} while (0)

	fmt_string(const char* fmt, ...) {
		FMT_IMPL;
	}

	explicit fmt_string(size_t size) {
		this->str.resize(size);
	}

	void fmtset(const char* fmt, ...) {
		FMT_IMPL;
	}
	const char* get() const {
		return this->str.c_str();
	}
	char* data() {
		return const_cast<char*>(this->str.data());
	}

	fmt_string operator+(const fmt_string& s) const {
		return fmt_string(this->str + s.str);
	}
	fmt_string& operator=(const fmt_string& s) {
		this->str = s.str;
		return *this;
	}
	bool operator==(const fmt_string& s) const {
		return this->str == s.str;
	}

	// conversion operators
	operator const char*() const {
		return str.c_str();
	}
	operator const std::string() const {
		return str;
	}

private:
	std::string str;
};

#endif /* HEAPTRACE_UTILS_H */
