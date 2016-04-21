#include "ribosome/expiration.hpp"

#include <gtest/gtest.h>
#include <glog/logging.h>

#include <atomic>

using namespace ioremap::ribosome;

TEST(main, expiration)
{
	expiration ex;
	std::atomic_int completed(0), failed(0);
	std::condition_variable cv;

	std::vector<expiration::token_t> tokens;

	int n = 100;
	for (int i = 0; i < n; ++i) {
		auto expires_at = std::chrono::system_clock::now() + std::chrono::milliseconds(1000 + rand() % 1000);
		auto token = ex.insert(expires_at, [&] () {
					completed++;
					cv.notify_one();
				});

		if (i % 2 == 0)
			tokens.push_back(token);
	}

	int fn = tokens.size();
	for (auto token: tokens) {
		auto cb = ex.remove(token);
		if (cb) {
			failed++;
			cb();
		}
	}

	std::mutex lock;
	std::unique_lock<std::mutex> l(lock);
	cv.wait_for(l, std::chrono::seconds(10), [&] {return completed == n;});
	l.unlock();

	ASSERT_EQ(completed, n);
	ASSERT_EQ(failed, fn);
}

int main(int argc, char **argv)
{
	google::InitGoogleLogging(argv[0]);
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
