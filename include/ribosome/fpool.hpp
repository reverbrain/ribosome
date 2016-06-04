#pragma once

#include <condition_variable>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace ioremap { namespace ribosome { namespace fpool {

template <typename T>
class array_deleter {
public:
	void operator () (T* d) const {
		delete [] d;
	}
};

struct message {
	enum {
		serialize_version_1 = 1,
	};

	struct {
		uint64_t size = 0;
		uint64_t flags = 0;
		uint64_t cmd = 0;
	} header;

	static const size_t header_size = sizeof(header);

	std::shared_ptr<char> data;


	message() {
	}
	message(uint64_t size) {
		header.size = size;
		data = std::shared_ptr<char>(new char[size], array_deleter<char>());
	}

	~message() {
	}
};

class io_scheduler {
public:
	io_scheduler(int efd, int fd, uint32_t event);
	~io_scheduler();

	bool ready(long timeout_ms);

private:
	int m_efd;
	int m_fd;
	uint32_t m_event;
};

class worker {
public:
	typedef std::function<message (const message &)> callback_t;

	worker();
	~worker();

	int start(callback_t callback);
	int restart(callback_t callback);
	int stop(int *status);

	void close();

	int write(const message &msg);
	int read(message &msg);

	pid_t pid() const;

private:
	int m_fd = -1;
	int m_epollfd = -1;
	pid_t m_pid = -1;
	bool m_need_exit = false;

	int read_raw(char *ptr, size_t size);
	int write_raw(const char *ptr, size_t size);

	void run(callback_t callback);
};

class controller {
public:
	typedef std::function<void (int status, const message &msg)> completion_t;

	controller(int size, worker::callback_t callback);
	~controller();

	void schedule(const message &msg, completion_t complete);

	std::vector<pid_t> pids() const;

private:
	std::vector<worker> m_workers;
	worker::callback_t m_callback;
	std::condition_variable m_cv;

	bool m_wait_need_exit = false;
	std::thread m_wait_thread;

	void wait_for_children();
};

}}} // namespace ioremap::ribosome::fpool
