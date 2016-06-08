#include "ribosome/fpool.hpp"

#include <atomic>

#include <gtest/gtest.h>
#include <glog/logging.h>

using namespace ioremap::ribosome;

class fpool_test: public ::testing::Test {
public:
	fpool_test() : m_ctl(m_workers, std::bind(&fpool_test::process, this, std::placeholders::_1)), m_completed(0) {
	}

	void send_and_test(int cmd) {
		fpool::message msg;
		msg.header.cmd = cmd;
		m_ctl.schedule(msg, [=](const fpool::message &reply) {
					ASSERT_EQ(reply.header.status, 0);
					ASSERT_EQ(reply.header.cmd, msg.header.cmd + 1);
					ASSERT_EQ(reply.header.size, fpool_test::m_message.size());

					std::string rdata(reply.data.get(), reply.header.size);
					ASSERT_EQ(rdata, fpool_test::m_message);

					m_completed++;
					m_cv.notify_one();
				});
	}

protected:
	size_t m_workers = 1;
	std::string m_message = "this is a test message";
	fpool::controller m_ctl;

	std::atomic_int m_completed;
	std::condition_variable m_cv;

	fpool::message process(const fpool::message &msg) {
		fpool::message reply(m_message.size());
		reply.header.cmd = msg.header.cmd + 1;
		memcpy(reply.data.get(), m_message.data(), reply.header.size);

		LOG(INFO) << getpid() << ": process: message: " << msg.str() << " -> " << reply.str();
		return reply;
	}
};

TEST_F(fpool_test, ping)
{
	int num = 100;
	for (int i = 0; i < num; ++i) {
		send_and_test(i);
	}

	std::mutex lock;
	std::unique_lock<std::mutex> l(lock);
	m_cv.wait_for(l, std::chrono::seconds(3), [&] {return m_completed == num;});
}

TEST_F(fpool_test, restart)
{
	send_and_test(0);
	auto pids = m_ctl.pids();
	ASSERT_EQ(pids.size(), m_workers);
	pid_t pid = pids[0];

	std::mutex lock;
	std::unique_lock<std::mutex> l(lock);

	// do not kill process until it completes processing the first message
	m_cv.wait_for(l, std::chrono::seconds(3), [&] {return m_completed == 1;});
	l.unlock();

	kill(pid, SIGTERM);

	usleep(100000);
	send_and_test(1);
	pids = m_ctl.pids();
	ASSERT_EQ(pids.size(), m_workers);
	ASSERT_NE(pid, pids[0]);

	l.lock();
	m_cv.wait_for(l, std::chrono::seconds(3), [&] {return m_completed == 2;});
}


int main(int argc, char **argv)
{
	google::InitGoogleLogging(argv[0]);
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
