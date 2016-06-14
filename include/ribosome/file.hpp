#pragma once

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace ioremap { namespace ribosome {

class file {
public:
	virtual ~file() {
		if (m_fd >= 0) {
			close(m_fd);
		}
	}

	virtual int open(const char *filename, int flags, mode_t mode) {
		int err = ::open(filename, flags, mode);
		if (err < 0) {
			err = -errno;
			return err;
		}

		m_fd = err;

		struct stat st;
		err = fstat(m_fd, &st);
		if (err < 0) {
			err = -errno;

			close(m_fd);
			m_fd = -1;
			return err;
		}

		m_size = st.st_size;
		return 0;
	}

	size_t size() const {
		return m_size;
	}

protected:
	int m_fd = -1;
	size_t m_size = 0;
};

class mapped_file : public file {
public:
	virtual ~mapped_file() {
		if (m_data) {
			munmap(m_data, m_size);
		}
	}

	virtual int open(const char *filename, int flags, mode_t mode) {
		int err = file::open(filename, flags, mode);
		if (err < 0)
			return err;

		int prot = PROT_READ;
		if (flags & O_RDWR) {
			prot |= PROT_WRITE;
		}
		if (mode & 0700) {
			prot |= PROT_EXEC;
		}

		m_data = mmap(NULL, m_size, prot, MAP_SHARED, m_fd, 0);
		if (m_data == MAP_FAILED) {
			err = -errno;
			close(m_fd);
			m_fd = -1;
			return err;
		}

		return 0;
	}

	template <typename T = char>
	T *data() const {
		return reinterpret_cast<T *>(m_data);
	}

private:
	void *m_data = NULL;
};

}} // namespace ioremap::ribosome
