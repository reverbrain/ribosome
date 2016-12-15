#include "ribosome/timer.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace ioremap;

int main()
{
	ribosome::timer tm;

	sleep(1);
	usleep(10000);
	float sec = tm.elapsed() / 1000.;
	printf("elapsed: %ld/%.3f msecs, %.3f seconds\n", tm.elapsed(), sec, tm.elapsed_seconds());
	if (abs(sec - tm.elapsed_seconds()) > 0.0000001) {
		printf("This is a bug, elapsed values must be equal\n");
		exit(-1);
	}

	return 0;
}
