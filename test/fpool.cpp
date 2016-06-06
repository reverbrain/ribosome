#include <gtest/gtest.h>
#include <glog/logging.h>

#include "ribosome/fpool.hpp"

using namespace ioremap::ribosome;

class fpool_test: public ::testing::Test {
public:
	fpool_test() : m_ctl(m_workers, std::bind(&fpool_test::process, this, std::placeholders::_1)) {
	}

	void send_and_test(int cmd) {
		fpool::message msg;
		msg.header.cmd = cmd;
		m_ctl.schedule(msg, [=](int status, const fpool::message &reply) {
					ASSERT_EQ(status, 0);
					ASSERT_EQ(reply.header.cmd, msg.header.cmd + 1);
					ASSERT_EQ(reply.header.size, fpool_test::m_message.size());

					std::string rdata(reply.data.get(), reply.header.size);
					ASSERT_EQ(rdata, fpool_test::m_message);
				});
	}

protected:
	size_t m_workers = 1;
	std::string m_message = "this is a test message";
	fpool::controller m_ctl;

	fpool::message process(const fpool::message &msg) {
		fpool::message reply(m_message.size());
		reply.header.cmd = msg.header.cmd + 1;
		memcpy(reply.data.get(), m_message.data(), reply.header.size);

#if 1
		fprintf(stderr, "%d: process: message: "
				"cmd: %d, id: %ld, size: %ld -> "
				"cmd: %d, id: %ld, size: %ld, message_size: %ld, message: %.*s\n",
				getpid(),
				msg.header.cmd, msg.header.id, msg.header.size,
				reply.header.cmd, reply.header.id, reply.header.size,
				m_message.size(),
				(int)reply.header.size, reply.data.get());
#endif
		return reply;
	}
};

TEST_F(fpool_test, ping)
{
	for (int i = 0; i < 10; ++i) {
		send_and_test(i);
	}
}

TEST_F(fpool_test, restart)
{
	send_and_test(0);
	auto pids = m_ctl.pids();
	ASSERT_EQ(pids.size(), m_workers);
	pid_t pid = pids[0];
	kill(pid, SIGTERM);
	usleep(100000);
	send_and_test(1);
	pids = m_ctl.pids();
	ASSERT_EQ(pids.size(), m_workers);
	ASSERT_NE(pid, pids[0]);
}


int main(int argc, char **argv)
{
	google::InitGoogleLogging(argv[0]);
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
