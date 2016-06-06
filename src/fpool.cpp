#include "ribosome/fpool.hpp"
#include "ribosome/timer.hpp"

#include <algorithm>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glog/logging.h>

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
}

worker::~worker()
{
	int status;
	stop(&status);
}

int worker::start(worker::callback_t callback)
{
	int err;
	int fd[2];

	err = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd);
	if (err == -1) {
		err = -errno;
		LOG(ERROR) << "fpool::worker::start: could not create socketpair" <<
			": error: " << strerror(-err) << " [" << err << "]";
		return err;
	}

	m_pid = fork();
	if (m_pid == -1) {
		err = -errno;
		LOG(ERROR) << "fpool::worker::start: could not fork" <<
			": error: " << strerror(-err) << " [" << err << "]";
		return err;
	}

	// child process
	if (m_pid == 0) {
		setsid();

		::close(fd[0]);
		m_fd = fd[1];

		m_epollfd = epoll_create1(EPOLL_CLOEXEC);
		if (m_epollfd < 0) {
			exit(-errno);
		}


		m_pid = getpid();

		run(callback);
		exit(0);
	}

	::close(fd[1]);
	m_fd = fd[0];

	m_epollfd = epoll_create1(EPOLL_CLOEXEC);
	if (m_epollfd < 0) {
		err = -errno;
		LOG(ERROR) << "fpool::worker::start: could not create epoll file descriptor" <<
			": error: " << strerror(-err) << " [" << err << "]";
		return err;
	}

	LOG(INFO) << "fpool::worker::start: process " << m_pid << " has been started";
	return 0;
}

void worker::close()
{
	if (m_fd >= 0) {
		::close(m_fd);
		m_fd = -1;
	}
	if (m_epollfd >= 0) {
		::close(m_epollfd);
		m_epollfd = -1;
	}
}

int worker::stop(int *status)
{
	close();

	if (m_pid < 0) {
		*status = 0;
		return 0;
	}

	int err = kill(m_pid, SIGTERM);
	if (err < 0) {
		err = -errno;
		LOG(ERROR) << "fpool::worker::stop: could not send SIGTERM to pid " << m_pid <<
			", error: " << strerror(-err) << " [" << err << "]";
		return err;
	}

	pid_t pid = 0;
	for (int i = 0; i < 30; ++i) {
		pid = waitpid(m_pid, status, WNOHANG);
		if (pid < 0) {
			err = -errno;

			LOG(ERROR) << "fpool::worker::stop: could not wait1 for to pid " << m_pid <<
				", error: " << strerror(-err) << " [" << err << "]";
			return err;
		}

		if (pid > 0)
			break;

		usleep(30000);
	}

	if (pid == 0) {
		err = kill(m_pid, SIGKILL);
		if (err < 0) {
			err = -errno;
			LOG(ERROR) << "fpool::worker::stop: could not send SIGKILL to pid " << m_pid <<
				", error: " << strerror(-err) << " [" << err << "]";
			return err;
		}

		pid = waitpid(m_pid, status, 0);
		if (pid <= 0) {
			err = -errno;
			LOG(ERROR) << "fpool::worker::stop: could not wait2 for pid " << m_pid <<
				", error: " << strerror(-err) << " [" << err << "]";
			return err;
		}
	}

	LOG(INFO) << "fpool::worker::stop: process " << m_pid << " has been stopped";
	m_pid = -1;
	return 0;
}

int worker::restart(worker::callback_t callback)
{
	int status;
	int err = stop(&status);
	if (err) {
		if (err != -ESRCH)
			return err;
	}

	return start(callback);
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
	: m_callback(callback)
	, m_wait_thread(std::bind(&controller::wait_for_children, this))
{
	for (int i = 0; i < size; ++i) {
		worker w;
		m_workers.emplace_back(w);
	}

	for (auto &w: m_workers) {
		int err = w.start(callback);
		if (err < 0) {
			std::ostringstream ss;
			ss << "could not start new worker thread, error: " << strerror(-err) << " [" << err << "]";
			throw std::runtime_error(ss.str());
		}
	}
}
controller::~controller()
{
	m_wait_need_exit = true;
	m_wait_thread.join();

	for (auto &w: m_workers) {
		int status;
		w.stop(&status);
	}
}

void controller::schedule(const message &msg, completion_t complete)
{
	message reply = message::copy_header(msg);

	if (m_workers.empty()) {
		reply.header.status = -ENOENT;
		complete(reply);
		return;
	}

	worker &w = m_workers[rand() % m_workers.size()];
	reply.header.status = w.write(msg);
	if (reply.header.status < 0) {
		complete(reply);
		return;
	}

	int err = w.read(reply);
	if (reply.header.status == 0 && err != 0) {
		reply.header.status = err;
	}

	complete(reply);
}

void controller::wait_for_children()
{
	while (!m_wait_need_exit) {
		int status;

		pid_t pid = waitpid(-1, &status, WNOHANG);
		if (pid > 0) {
			LOG(ERROR) << "fpool::controller::wait_for_children: detected process termination"
				": pid: " << pid <<
				", exited: " << WIFEXITED(status) <<
					" (status: " << WEXITSTATUS(status) << ")" <<
				", killed-by-signal: " << WIFSIGNALED(status) <<
					" (signal: " << WTERMSIG(status) << ", coredump: " << WCOREDUMP(status) << ")";

			auto it = std::find_if(m_workers.begin(), m_workers.end(), [=] (const worker &w) {
						return w.pid() == pid;
					});
			if (it == m_workers.end()) {
				continue;
			}

			if (WIFEXITED(status) || WIFSIGNALED(status)) {
				int err;

				it->close();
				err = it->start(m_callback);
				if (err < 0) {
					LOG(ERROR) << "fpool::controller::wait_for_children: could not restart process: " <<
						strerror(errno);
					m_workers.erase(it);
				}
			}
		} else {
			usleep(10000);
		}
	}
}

std::vector<pid_t> controller::pids() const {
	std::vector<pid_t> ret;

	for (auto &w: m_workers) {
		ret.push_back(w.pid());
	}

	return ret;
}

} // namespace fpool

}} // namespace ioremap::ribosome
