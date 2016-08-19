#include "ribosome/lstring.hpp"
#include "ribosome/split.hpp"

#include <iostream>
#include <vector>

#include <unicode/ustdio.h>

using namespace ioremap::ribosome;

int main(int argc, char *argv[])
{
	std::string str("это такой...test,.' नमस्ते");

	std::cout << "string: " << str << std::endl;

	std::vector<UChar> chars;
	chars.resize(str.size() + 1);
	UErrorCode err = U_ZERO_ERROR;
	int real_size = 0;
	u_strFromUTF8((UChar *)chars.data(), str.size(), &real_size, str.c_str(), -1, &err);
	if (U_FAILURE(err)) {
		std::cerr << "could not convert std::string to unicode: " << u_errorName(err) << std::endl;
		return -1;
	}
	chars.resize(real_size);
	printf("std::string.size: %zd, std::vector<UChar>.size: %zd, real_size: %d\n", str.size(), chars.size(), real_size);
#if 0
	for (const auto &ch: chars) {
		printf("%x\n", (uint16_t)ch);
	}
#endif
	UFILE* u_stdout = u_finit(stdout, NULL, NULL);
	u_fprintf(u_stdout, "another string: %S\n", chars.data());
	u_fclose(u_stdout);

	char tmp[chars.size() * 4 + 1];
	memset(tmp, 0, sizeof(tmp));
	u_strToUTF8(tmp, sizeof(tmp), &real_size, chars.data(), chars.size(), &err);
	if (U_FAILURE(err)) {
		std::cerr << "could not convert unicode to std::string: " << u_errorName(err) << std::endl;
		return -1;
	}
#if 0
	for (size_t i = 0; i < sizeof(tmp); ++i) {
		printf("%x %c\n", tmp[i], tmp[i]);
	}
#endif
	lstring ls = lconvert::from_utf8(str.data(), str.size());
	std::cout << ls << std::endl;
	ls[7] = u'Й';
	std::cout << ls << std::endl;

	split spl;

	for (const auto &w: spl.convert_split_words(ls, "")) {
		std::cout << w << std::endl;
	}

	if (argc != 0) {
		std::ifstream in(argv[1]);
		std::ostringstream ss;

		ss << in.rdbuf();
		str = ss.str();
	}
	ls = lconvert::from_utf8(str.data(), str.size());
	std::cout << ls << std::endl;

	for (const auto &w: spl.convert_split_words(ls, "")) {
		std::cout << lconvert::to_lower(w) << std::endl;
	}
	return 0;
}
