#include <gtest/gtest.h>
#include <glog/logging.h>

#include "ribosome/fpool.hpp"

using namespace ioremap::ribosome;

class fpool_test: public ::testing::Test {
public:
	fpool_test() : m_ctl(1, std::bind(&fpool_test::process, this, std::placeholders::_1)) {
	}


protected:
	std::string m_message = "this is a test message";
	fpool::controller m_ctl;

	fpool::message process(const fpool::message &msg) {
		fpool::message reply(m_message.size());
		reply.header.cmd = msg.header.cmd + 1;
		memcpy(reply.data.get(), m_message.data(), reply.header.size);
#if 0
		fprintf(stderr, "%d: process: message: cmd: %ld, size: %ld -> cmd: %ld, size: %ld, message_size: %ld, message: %.*s\n",
				getpid(),
				msg.header.cmd, msg.header.size,
				reply.header.cmd, reply.header.size,
				m_message.size(),
				(int)reply.header.size, reply.data.get());
#endif
		return reply;
	}
};

TEST_F(fpool_test, ping)
{
	for (int i = 0; i < 10; ++i) {
		fpool::message msg;
		msg.header.cmd = i;
		m_ctl.schedule(msg, [=](int status, const fpool::message &reply) {
					ASSERT_EQ(status, 0);
					ASSERT_EQ(reply.header.cmd, msg.header.cmd + 1);
					ASSERT_EQ(reply.header.size, fpool_test::m_message.size());

					std::string rdata(reply.data.get(), reply.header.size);
					ASSERT_EQ(rdata, fpool_test::m_message);
				});
	}
}

int main(int argc, char **argv)
{
	google::InitGoogleLogging(argv[0]);
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
