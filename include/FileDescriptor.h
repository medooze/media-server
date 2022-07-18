#ifndef _FILEDESCRIPTOR_H_
#define _FILEDESCRIPTOR_H_

#include <utility>
#include <unistd.h>
#include "log.h"

/**
 * @brief RAII class holding a single FD
 */
class FileDescriptor {
public:
	/**
	 * @brief Construct empty FD holder (fd = -1)
	 */
	FileDescriptor() {}
	/**
	 * @brief Construct holder taking ownership of the passed FD
	 */
	explicit FileDescriptor(int fd): fd(fd) {}

	~FileDescriptor() {
		if (fd >= 0 && close(fd) < 0)
			Error("-FileDescriptor::~FileDescriptor() | failed closing descriptor: %s\n", strerror(errno));
	}

	FileDescriptor(FileDescriptor&& other) {
		std::swap(*this, other);
	}
	FileDescriptor& operator=(FileDescriptor&& other) {
		std::swap(*this, other);
		return *this;
	}
	// prevent copy for now; I'm not comfortable implicitly
	// duplicating any FDs this class may hold in the future
	FileDescriptor(const FileDescriptor& other) = delete;
	FileDescriptor& operator=(const FileDescriptor& other) = delete;

	/**
	 * @brief Return stored FD (not checked valid)
	 */
	operator int() {
		return fd;
	}
	/**
	 * @brief Release stored FD from ownership; the holder becomes empty
	 */
	int release() {
		int fd = -1;
		std::swap(this->fd, fd);
		return fd;
	}

	friend void swap(FileDescriptor& a, FileDescriptor& b) {
		std::swap(a.fd, b.fd);
	}
private:
	int fd = -1;
};

#endif
