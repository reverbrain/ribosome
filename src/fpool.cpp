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

//#define FPOLL_STDOUT_DEBUG

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
		err = -errno;

		LOG(ERROR) << "could not add epoll event: " << strerror(-err);
		std::ostringstream ss;
		ss << "could not add epoll event: " << strerror(-err);
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

int io_scheduler::ready(long timeout_ms, epoll_event *ev)
{
	ribosome::timer tm;

	while (true) {
		memset(ev, 0, sizeof(epoll_event));

		int nfds = epoll_wait(m_efd, ev, 1, timeout_ms);
		if (nfds < 0) {
			int err = -errno;
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN)
				continue;

			LOG(ERROR) << "could not wait for epoll event: " << strerror(-err);
			return err;
		}

		if (nfds == 0)
			return 0;

		dprintf("%d: ready: efd: %d, fd: %d, events: %x, requested_event: %x\n",
				getpid(),
				m_efd, m_fd, ev->events, m_event);
		if (ev->events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
			printf("%d: ready: efd: %d, fd: %d, events: %x, requested_event: %x\n",
					getpid(),
					m_efd, m_fd, ev->events, m_event);
		}

		if (ev->events & m_event)
			return 1;

		timeout_ms -= tm.restart();
		if (timeout_ms <= 0)
			return -ETIMEDOUT;
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

	m_need_exit = false;

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

		sigset_t set;
		sigemptyset(&set);
		sigaddset(&set, SIGTERM);
		sigprocmask(SIG_UNBLOCK, &set, NULL);

		LOG(INFO) << "worker: " << m_pid << ", need_exit: " << m_need_exit << ": starting";
		run(callback);
		LOG(INFO) << "worker: " << m_pid << ": exiting";
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

	m_io_thread.reset(new std::thread(std::bind(&worker::io, this)));

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

	if (m_io_thread) {
		m_need_exit = true;
		m_io_thread->join();
		m_io_thread.reset();
	}
}

int worker::stop(int *status)
{
	m_need_exit = true;

	if (m_pid < 0) {
		*status = 0;
		return 0;
	}

	LOG(INFO) << "fpool::worker::stop: sendig SIGTERM to pid " << m_pid;
	int err = kill(m_pid, SIGTERM);
	if (err < 0) {
		err = -errno;
		LOG(ERROR) << "fpool::worker::stop: could not send SIGTERM to pid " << m_pid <<
			", error: " << strerror(-err) << " [" << err << "]";
		return err;
	}

	pid_t pid = 0;
	for (int i = 0; i < 10; ++i) {
		pid = waitpid(m_pid, status, WNOHANG);
		if (pid < 0) {
			err = -errno;

			LOG(ERROR) << "fpool::worker::stop: could not wait1 for to pid " << m_pid <<
				", error: " << strerror(-err) << " [" << err << "]";
			return err;
		}

		if (pid > 0)
			break;

		usleep(10000);
	}

	if (pid == 0) {
		LOG(ERROR) << "fpool::worker::stop: SIGTERM has been ignored, sending SIGKILL to pid " << m_pid;

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

	close();
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

pid_t worker::pid() const
{
	return m_pid;
}

size_t worker::queue_size() const
{
	return m_queue.size();
}

void worker::queue(const message &msg, completion_t complete)
{
	std::unique_lock<std::mutex> lk(m_lock);
	m_queue.push_back(std::pair<message, completion_t>(msg, complete));
	m_wait.notify_one();
}

// runs in forked child
void worker::run(callback_t callback)
{
	while (!m_need_exit) {
		int err;
		message msg;

		while (!msg.io_completed() && !m_need_exit) {
			err = read(msg, -1);
			if (err) {
				LOG(ERROR) << "worker: " << m_pid << ", read error: " << err << ", exiting";
				exit(err);
			}
		}

		LOG(INFO) << "worker: " << m_pid << ": job: " << msg.str();

		message reply = callback(msg);
		LOG(INFO) << "worker: " << m_pid << ": job: " << msg.str() << " -> " << reply.str();

		while (!reply.io_completed() && !m_need_exit) {
			err = write(reply, -1);
			if (err) {
				LOG(ERROR) << "worker: " << m_pid << ", write error: " << err << ", exiting";
				exit(err);
			}
		}
	}
}

void worker::io()
{
	int err = raw_io();

	// there was an IO error, protocol is already broken,
	// worker can not recover and should be restarted
	if (err < 0) {
		LOG(ERROR) << "worker: " << m_pid << ", IO error: " << err << ", killing itself";
		kill(m_pid, SIGTERM);
		return;
	}
}

int worker::raw_io()
{
	long timeout = 100;
	int err = 0;

	while (!m_need_exit) {
		std::unique_lock<std::mutex> lk(m_lock);
		if (m_queue.empty()) {
			m_wait.wait_for(lk, std::chrono::milliseconds(100), [&] () {return !m_queue.empty();});
		}

		if (m_queue.empty()) {
			continue;
		}

		std::pair<message, worker::completion_t> p(m_queue.front());
		m_queue.pop_front();
		lk.unlock();

		message &msg = p.first;
		while (!msg.io_completed() && !m_need_exit) {
			err = write(msg, timeout);
			if (err < 0) {
				message reply = message::copy_header(msg);
				reply.header.status = err;
				p.second(reply);
				return err;
			}
		}

		if (m_need_exit)
			break;

		// as soon as write completed we do not write next message to the worker,
		// instead wait for reply
		message reply;
		while (!reply.io_completed() && !m_need_exit) {
			err = read(reply, timeout);
			if (err < 0) {
				reply = message::copy_header(msg);
				reply.header.status = err;
				p.second(reply);
				return err;
			}
		}
		LOG(INFO) << "going to call completion callback with reply: " << reply.str();
		p.second(reply);
		LOG(INFO) << "completed completion callback with reply: " << reply.str();
	}

	return 0;
}

ssize_t worker::write_raw(const char *ptr, size_t size)
{
	size_t written = 0;
	while (written < size) {
		ssize_t err = ::send(m_fd, ptr + written, size - written, 0);
		if (err <= 0) {
			err = -errno;
			printf("%d: write_raw: size: %ld, written: %ld, err: %ld\n", getpid(), size, written, err);

			if (err == -EAGAIN)
				return written;

			return err;
		}

		written += err;
	}

	return written;
}
int worker::write(message &msg, long timeout)
{
	ssize_t err;
	char *ptr;
	epoll_event ev;

	io_scheduler send(m_epollfd, m_fd, EPOLLOUT);
	err = send.ready(timeout, &ev);
	if (err < 0) {
		if (ev.events & (EPOLLHUP | EPOLLERR))
			return -EIO;

		return err;
	}

	if (err == 0)
		return 0;

	if (msg.io_offset < msg.header_size) {
		ptr = (char *)&msg.header;
		ptr += msg.io_offset;
		err = write_raw(ptr, msg.header_size - msg.io_offset);
		if (err < 0)
			return err;

		msg.io_offset += err;
	}

	size_t data_offset = msg.io_offset - msg.header_size;
	ptr = msg.data.get() + data_offset;
	size_t size = msg.header.size - data_offset;
	err = write_raw(ptr, size);
	if (err < 0)
		return err;

	msg.io_offset += err;

	return 0;
}


ssize_t worker::read_raw(char *ptr, size_t size)
{
	size_t already_read = 0;
	ssize_t err;

	while (already_read < size) {
		err = ::recv(m_fd, ptr + already_read, size - already_read, 0);
		if (err <= 0) {
			err = -errno;
			printf("%d: read_raw: size: %ld, already_read: %ld, err: %ld\n", getpid(), size, already_read, err);

			if (err == 0)
				return -ECONNRESET;

			if (err == -EAGAIN)
				return already_read;

			return err;
		}

		already_read += err;
	}

	return already_read;
}
int worker::read(message &msg, long timeout)
{
	ssize_t err;
	char *ptr;
	epoll_event ev;

	io_scheduler recv(m_epollfd, m_fd, EPOLLIN);
	err = recv.ready(timeout, &ev);
	if (err < 0) {
		if (ev.events & (EPOLLHUP | EPOLLERR))
			return -EIO;

		return err;
	}

	if (err == 0)
		return 0;

	if (msg.io_offset < msg.header_size) {
		ptr = (char *)&msg.header;
		ptr += msg.io_offset;
		err = read_raw(ptr, msg.header_size - msg.io_offset);
		if (err < 0)
			return err;

		msg.io_offset += err;

		if (err == 0 && (ev.events & (EPOLLHUP | EPOLLERR)))
			return -EIO;

		if (msg.io_offset != msg.header_size)
			return 0;
	}

	if (!msg.data) {
		msg.data.reset(new char[msg.header.size], array_deleter<char>());
	}

	size_t data_offset = msg.io_offset - msg.header_size;
	ptr = msg.data.get() + data_offset;
	size_t size = msg.header.size - data_offset;

	err = read_raw(ptr, size);
	if (err < 0)
		return err;

	msg.io_offset += err;

	dprintf("%d: message read: cmd: %d, size: %ld, data: %.*s\n",
			getpid(), msg.header.cmd, msg.header.size, (int)msg.header.size, msg.data.get());

	if (err == 0 && (ev.events & (EPOLLHUP | EPOLLERR)))
		return -EIO;
	return 0;
}


controller::controller(int size, worker::callback_t callback)
	: m_callback(callback)
	, m_wait_thread(std::bind(&controller::wait_for_children, this))
{
	m_workers.resize(size);

	for (auto &w: m_workers) {
		w.reset(new worker());

		int err = w->start(callback);
		if (err < 0) {
			LOG(ERROR) << "could not start new worker thread, error: " << strerror(-err) << " [" << err << "]";

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
		w->stop(&status);
	}
}

void controller::schedule(const message &msg, worker::completion_t complete)
{
	std::unique_lock<std::mutex> guard(m_lock);
	if (m_workers.empty()) {
		guard.unlock();

		message reply = message::copy_header(msg);
		reply.header.status = -ENOENT;
		complete(reply);
		return;
	}

	int pos = 0;
	size_t size = m_workers[pos]->queue_size();
	for (size_t i = 0; i < m_workers.size(); ++i) {
		auto &w = m_workers[i];

		if (w->queue_size() < size) {
			size = w->queue_size();
			pos = i;
		}
	}

	auto &w = m_workers[pos];
	w->queue(msg, complete);
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

			auto it = std::find_if(m_workers.begin(), m_workers.end(), [=] (const std::unique_ptr<worker> &w) {
						return w->pid() == pid;
					});
			if (it == m_workers.end()) {
				continue;
			}

			if (WIFEXITED(status) || WIFSIGNALED(status)) {
				int err;

				(*it)->close();
				err = (*it)->start(m_callback);
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
		ret.push_back(w->pid());
	}

	return ret;
}

} // namespace fpool

}} // namespace ioremap::ribosome
