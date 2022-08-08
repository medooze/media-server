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

	~FileDescriptor()
	{
		if (fd >= 0 && close(fd) < 0)
			Error("-FileDescriptor::~FileDescriptor() | failed closing descriptor: %s\n", strerror(errno));
	}

	FileDescriptor(FileDescriptor&& other)
	{
		swap(*this, other);
	}
	FileDescriptor& operator=(FileDescriptor&& other)
	{
		swap(*this, other);
		return *this;
	}

	FileDescriptor(const FileDescriptor& other)
	{
		fd = dup(other.fd);
	}
	FileDescriptor& operator=(FileDescriptor other)
	{
		swap(*this, other);
		return *this;
	}

	bool isValid()
	{
		return (fd!=-1);
	}
	/**
	 * @brief Return stored FD (not checked valid)
	 */
	operator int()
	{
		return fd;
	}
	/**
	 * @brief Release stored FD from ownership; the holder becomes empty
	 */
	int release()
	{
		int fd = -1;
		std::swap(this->fd, fd);
		return fd;
	}

	friend void swap(FileDescriptor& a, FileDescriptor& b)
	{
		std::swap(a.fd, b.fd);
	}
private:
	int fd = -1;
};

#endif
