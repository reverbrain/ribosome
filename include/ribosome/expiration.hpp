#ifndef __RIBOSOME_EXPIRATION_HPP
#define __RIBOSOME_EXPIRATION_HPP

#include <chrono>
#include <condition_variable>
#include <map>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ioremap { namespace ribosome {

class expiration {
public:
	typedef std::function<void ()> callback_t;
	typedef uint64_t token_t;

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

	token_t insert(const std::chrono::system_clock::time_point &expires_at, callback_t callback) {
		std::unique_lock<std::mutex> guard(m_lock);

		control_t ctl;
		ctl.token = ++m_seq;
		ctl.callback = callback;

		auto it = m_timeouts.find(expires_at);
		if (it == m_timeouts.end()) {
			m_timeouts.insert(std::make_pair(expires_at, std::vector<control_t>({ctl})));
		} else {
			it->second.push_back(ctl);
		}

		m_tok2time[ctl.token] = expires_at;

		guard.unlock();
		m_wait.notify_one();

		return ctl.token;
	}

	callback_t remove(const token_t token) {
		std::unique_lock<std::mutex> guard(m_lock);

		auto etime = m_tok2time.find(token);
		if (etime == m_tok2time.end())
			return callback_t();

		auto expires_at = etime->second;
		m_tok2time.erase(etime);

		auto ctl = m_timeouts.find(expires_at);
		if (ctl == m_timeouts.end())
			return callback_t();

		for (auto it = ctl->second.begin(), end = ctl->second.end(); it != end; ++it) {
			if (it->token == token) {
				auto callback = it->callback;
				ctl->second.erase(it);
				return callback;
			}
		}

		return callback_t();
	}

private:
	token_t m_seq = 0;
	bool m_need_exit = false;
	std::condition_variable m_wait;
	std::mutex m_lock;

	std::thread m_thread;

	typedef struct {
		token_t	token;
		callback_t	callback;
	} control_t;

	std::map<std::chrono::system_clock::time_point, std::vector<control_t>> m_timeouts;
	std::unordered_map<token_t, std::chrono::system_clock::time_point> m_tok2time;

	void run() {
		while (!m_need_exit) {
			std::unique_lock<std::mutex> guard(m_lock);

			auto next_check = std::chrono::system_clock::now() + std::chrono::seconds(1);
			if (m_timeouts.size())
				next_check = m_timeouts.begin()->first;

			m_wait.wait_until(guard, next_check);

			std::vector<control_t> expired;

			// mutex is locked
			while (m_timeouts.size() != 0) {
				auto f = m_timeouts.begin();
				auto timestamp = f->first;
				if (std::chrono::system_clock::now() < timestamp)
					break;

				std::swap(expired, f->second);
				m_timeouts.erase(f);

				for (auto &ctl: expired) {
					m_tok2time.erase(ctl.token);
				}
			}

			guard.unlock();

			for (auto &ctl: expired) {
				ctl.callback();
			}
		}
	}
};

}} // namespace ioremap::ribosome

#endif // __RIBOSOME_EXPIRATION_HPP
