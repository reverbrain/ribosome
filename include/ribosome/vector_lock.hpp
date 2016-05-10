#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>

namespace ioremap { namespace ribosome {

struct lock_entry {
	lock_entry(bool l): locked(l) {}
	std::condition_variable cond;
	int waiting = 0;
	bool locked = false;
};

typedef std::unique_ptr<lock_entry> lock_entry_ptr;
static inline lock_entry_ptr new_lock_entry_ptr(bool l) {
	return std::unique_ptr<lock_entry>(new lock_entry(l));
}

class vector_lock {
public:
	vector_lock() {
	}

	void lock(const std::string &key) {
		std::unique_lock<std::mutex> lock(m_sync_lock);
		auto it = m_locks.find(key);
		if (it == m_locks.end()) {
			m_locks.insert(std::pair<std::string, lock_entry_ptr>(key, new_lock_entry_ptr(true)));
			return;
		}

		lock_entry_ptr &ptr = it->second;

		ptr->waiting++;
		ptr->cond.wait(lock, [&] { return ptr->locked == false; });
		ptr->locked = true;
		ptr->waiting--;
	}

	bool try_lock(const std::string &key) {
		std::unique_lock<std::mutex> lock(m_sync_lock);
		auto it = m_locks.find(key);
		if (it == m_locks.end()) {
			m_locks.insert(std::pair<std::string, lock_entry_ptr>(key, new_lock_entry_ptr(true)));
			return true;
		}

		return false;
	}

	void unlock(const std::string &key) {
		std::unique_lock<std::mutex> lock(m_sync_lock);
		auto it = m_locks.find(key);
		if (it == m_locks.end()) {
			throw std::runtime_error(key + ": trying to unlock key which is not locked");
		}

		lock_entry_ptr &ptr = it->second;

		ptr->locked = false;
		if (ptr->waiting != 0) {
			ptr->cond.notify_one();
			return;
		}

		m_locks.erase(it);
	}

private:
	std::mutex m_sync_lock;
	std::map<std::string, lock_entry_ptr> m_locks;
};

template <typename T>
class locker {
public:
	locker(T *t, const std::string &key) : m_t(t), m_key(key) {
	}
	locker(const locker &other) {
		m_key = other.m_key;
		m_t = other.m_t;
	}
	locker(locker &&other) {
		m_key = other.m_key;
		m_t = other.m_t;
	}
	~locker() {
	}

	void lock() {
		m_t->lock(m_key);
	}

	bool try_lock(const std::string &key) {
		return m_t->try_lock(m_key);
	}

	void unlock() {
		m_t->unlock(m_key);
	}

private:
	T *m_t;
	std::string m_key;
};

}} // namespace ioremap::ribosome
