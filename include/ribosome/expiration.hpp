#ifndef __RIBOSOME_EXPIRATION_HPP
#define __RIBOSOME_EXPIRATION_HPP

#include <chrono>
#include <condition_variable>
#include <map>
#include <thread>
#include <vector>

namespace ioremap { namespace ribosome {

class expiration {
	typedef std::function<void ()> expiration_callback_t;
public:
	expiration() : m_thread(std::bind(&expiration::run, this)) {
	}

	~expiration() {
		stop();
		m_thread.join();
	}

	void stop() {
		m_need_exit = true;
		m_wait.notify_one();
	}

	void insert(const std::chrono::system_clock::time_point &expires_at, const expiration_callback_t &key) {
		std::unique_lock<std::mutex> guard(m_lock);

		auto it = m_timeouts.find(expires_at);
		if (it == m_timeouts.end()) {
			m_timeouts.insert(std::make_pair(expires_at, std::vector<expiration_callback_t>({key})));
		} else {
			it->second.emplace_back(key);
		}

		guard.unlock();
		m_wait.notify_one();
	}

private:
	bool m_need_exit = false;
	std::condition_variable m_wait;
	std::mutex m_lock;

	std::thread m_thread;

	std::map<std::chrono::system_clock::time_point, std::vector<expiration_callback_t>> m_timeouts;

	void run() {
		while (!m_need_exit) {
			std::unique_lock<std::mutex> guard(m_lock);

			auto next_check = std::chrono::system_clock::now() + std::chrono::seconds(1);
			if (m_timeouts.size())
				next_check = m_timeouts.begin()->first;

			m_wait.wait_until(guard, next_check);

			std::vector<expiration_callback_t> keys;

			// mutex is locked
			while (m_timeouts.size() != 0) {
				auto f = m_timeouts.begin();
				auto timestamp = f->first;
				if (std::chrono::system_clock::now() < timestamp)
					break;

				keys.insert(keys.end(), f->second.begin(), f->second.end());
				m_timeouts.erase(f);
			}

			guard.unlock();
			for (auto &k : keys) {
				k();
			}
		}
	}
};

}} // namespace ioremap::ribosome

#endif // __RIBOSOME_EXPIRATION_HPP
