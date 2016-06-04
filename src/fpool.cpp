#include "ribosome/fpool.hpp"
#include "ribosome/timer.hpp"


#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef FPOOL_STDOUT_DEBUG
#define dprintf(fmt, a...) printf(fmt, ##a)
#else
#define dprintf(fmt, a...)
#endif

namespace ioremap { namespace ribosome {

namespace fpool {

io_scheduler::io_scheduler(int efd, int fd, uint32_t event) : m_efd(efd), m_fd(fd), m_event(event)
{
	epoll_event ev;
	ev.events = event;
	ev.data.fd = m_fd;
	int err = epoll_ctl(m_efd, EPOLL_CTL_ADD, m_fd, &ev);
	if (err < 0) {
		std::ostringstream ss;
		ss << "could not add epoll event: " << strerror(errno);
		throw std::runtime_error(ss.str());
	}
}

io_scheduler::~io_scheduler()
{
	epoll_event ev;
	ev.events = m_event;
	ev.data.fd = m_fd;
	epoll_ctl(m_efd, EPOLL_CTL_DEL, m_fd, &ev);
}

bool io_scheduler::ready(long timeout_ms)
{
	epoll_event ev;
	ribosome::timer tm;

	while (true) {
		int nfds = epoll_wait(m_efd, &ev, 1, timeout_ms);
		if (nfds < 0) {
			std::ostringstream ss;
			ss << "could not wait for epoll event: " << strerror(errno);
			throw std::runtime_error(ss.str());
		}

		if (nfds == 0)
			return false;

		dprintf("%d: ready: efd: %d, fd: %d, events: %x, requested_event: %x\n",
				getpid(),
				m_efd, m_fd, ev.events, m_event);
		if (ev.events & m_event)
			return true;

		timeout_ms -= tm.restart();
		if (timeout_ms <= 0)
			return false;
	}
}


worker::worker()
{
	m_fd = -1;
	m_pid = -1;
}
worker::worker(const worker &w)
{
	m_fd = -1;
	m_pid = -1;
}

worker::~worker()
{
	if (m_fd != -1) {
		close(m_fd);
	}

	close(m_epollfd);
}

int worker::start(worker::callback_t callback)
{
	int err;
	int fd[2];

	err = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd);
	if (err == -1) {
		return -errno;
	}

	pid_t m_pid = fork();
	if (m_pid == -1) {
		return -errno;
	}

	// child process
	if (m_pid == 0) {
		setsid();

		close(fd[0]);
		m_fd = fd[1];

		m_epollfd = epoll_create1(EPOLL_CLOEXEC);
		if (m_epollfd < 0) {
			exit(-errno);
		}


		m_pid = getpid();

		run(callback);
		exit(0);
	}

	close(fd[1]);
	m_fd = fd[0];

	m_epollfd = epoll_create1(EPOLL_CLOEXEC);
	if (m_epollfd < 0) {
		return -errno;
	}

	return 0;
}

pid_t worker::pid() const {
	return m_pid;
}

void worker::run(callback_t callback)
{
	while (!m_need_exit) {
		int err;
		message msg;

		err = read(msg);
		if (err) {
			exit(err);
		}

		message reply = callback(msg);
		err = write(reply);
		if (err) {
			exit(err);
		}
	}
}

int worker::write_raw(const char *ptr, size_t size)
{
	io_scheduler send(m_epollfd, m_fd, EPOLLOUT);

	size_t written = 0;
	while (written < size) {
		if (!send.ready(-1)) {
			return -ETIMEDOUT;
		}

		ssize_t err = ::send(m_fd, ptr + written, size - written, 0);
		if (err <= 0) {
			err = -errno;
			dprintf("write_raw: size: %ld, written: %ld, err: %ld\n", size, written, err);
			return err;
		}

		written += err;
	}

	return 0;
}
int worker::write(const message &msg)
{
	ssize_t err;

	std::unique_lock<std::mutex> guard(m_lock);

	err = write_raw((char *)&msg.header, msg.header_size);
	if (err < 0)
		return err;

	err = write_raw(msg.data.get(), msg.header.size);
	if (err < 0)
		return err;

	return 0;
}


int worker::read_raw(char *ptr, size_t size)
{
	size_t already_read = 0;
	ssize_t err;

	io_scheduler recv(m_epollfd, m_fd, EPOLLIN);

	while (already_read < size) {
		if (!recv.ready(-1)) {
			return -ETIMEDOUT;
		}

		err = ::recv(m_fd, ptr + already_read, size - already_read, 0);
		if (err <= 0) {
			err = -errno;
			dprintf("read_raw: size: %ld, read: %ld, err: %ld\n", size, already_read, err);
			return err;
		}

		already_read += err;
	}

	return 0;
}
int worker::read(message &msg)
{
	ssize_t err;

	std::unique_lock<std::mutex> guard(m_lock);

	err = read_raw((char *)&msg.header, msg.header_size);
	if (err < 0)
		return err;

	msg.data.reset(new char[msg.header.size], array_deleter<char>());
	err = read_raw(msg.data.get(), msg.header.size);
	if (err < 0)
		return err;

	dprintf("message read: cmd: %ld, size: %ld, data: %.*s\n",
			msg.header.cmd, msg.header.size, (int)msg.header.size, msg.data.get());
	return 0;
}


controller::controller(int size, worker::callback_t callback)
{
	for (int i = 0; i < size; ++i) {
		worker w;
		m_workers.emplace_back(w);
	}

	for (auto &w: m_workers) {
		w.start(callback);
	}
}

void controller::schedule(const message &msg, completion_t complete)
{
	worker &w = m_workers[rand() % m_workers.size()];
	int err = w.write(msg);
	if (err < 0) {
		complete(err, msg);
		return;
	}

	message reply;
	err = w.read(reply);
	complete(err, reply);
}

} // namespace fpool

}} // namespace ioremap::ribosome
