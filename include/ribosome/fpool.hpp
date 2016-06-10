#pragma once

#include <condition_variable>
#include <memory>
#include <sstream>
#include <deque>
#include <stdexcept>
#include <thread>
#include <vector>

#include <sys/epoll.h>

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
		uint64_t id = 0;
		int status = 0;
		int cmd = 0;
	} header;

	static const size_t header_size = sizeof(header);

	std::shared_ptr<char> data;
	size_t io_offset = 0;

	message() {
	}
	message(uint64_t size) {
		header.size = size;
		data = std::shared_ptr<char>(new char[size], array_deleter<char>());
	}

	~message() {
	}

	static message copy_header(const message &other) {
		message ret;
		ret.header = other.header;
		ret.header.size = 0;
		return ret;
	}

	bool io_completed() const {
		return io_offset == header_size + header.size;
	}

	std::string str() const {
		std::ostringstream ss;
		ss << "cmd: " << header.cmd <<
			", id: " << header.id <<
			", size: " << header.size;
		return ss.str();
	}
};

class io_scheduler {
public:
	io_scheduler(int efd, int fd, uint32_t event);
	~io_scheduler();

	int ready(long timeout_ms, epoll_event *ev);

private:
	int m_efd;
	int m_fd;
	uint32_t m_event;
};

class worker {
public:
	typedef std::function<message (const message &)> callback_t;
	typedef std::function<void (const message &)> completion_t;

	worker();
	~worker();

	int start(callback_t callback);
	int restart(callback_t callback);
	int stop(int *status);

	void close();

	pid_t pid() const;

	size_t queue_size() const;
	void queue(const message &msg, completion_t complete);

private:
	int m_fd = -1;
	int m_epollfd = -1;
	pid_t m_pid = -1;
	bool m_need_exit = false;

	std::mutex m_lock;
	std::condition_variable m_wait;
	std::deque<std::pair<message, completion_t>> m_queue;

	std::unique_ptr<std::thread> m_io_thread;

	ssize_t read_raw(char *ptr, size_t size);
	ssize_t write_raw(const char *ptr, size_t size);

	int write(message &msg, long timeout);
	int read(message &msg, long timeout);

	// runs in a forked child
	void run(callback_t callback);

	// runs in the IO thread
	int raw_io();
	void io();
};

class controller {
public:
	controller(int size, worker::callback_t callback);
	~controller();

	void schedule(const message &msg, worker::completion_t complete);

	std::vector<pid_t> pids() const;

private:
	std::mutex m_lock;
	std::vector<std::unique_ptr<worker>> m_workers;
	worker::callback_t m_callback;

	bool m_wait_need_exit = false;
	std::thread m_wait_thread;

	void wait_for_children();
};

}}} // namespace ioremap::ribosome::fpool
