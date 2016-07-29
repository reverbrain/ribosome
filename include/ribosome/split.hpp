#ifndef __RIBOSOME_SPLIT_HPP
#define __RIBOSOME_SPLIT_HPP

#include "ribosome/charset.hpp"
#include "ribosome/lstring.hpp"

#include <vector>

#include <unicode/ubrk.h>

namespace ioremap { namespace ribosome {

class split {
public:
	std::vector<lstring> convert_split_words(const char *text, size_t size, const std::string &enc_hint) {
		std::vector<UChar> uc(size+1);

		size_t usize = uc.size();

		m_ch.convert((UChar *)uc.data(), &usize, text, size, enc_hint);
		uc.resize(usize);

		std::vector<lstring> ret;

		UBreakIterator* bi;
		int prev = -1, pos;

		UErrorCode err = U_ZERO_ERROR;
		bi = ubrk_open(UBRK_WORD, get_locale(), uc.data(), uc.size(), &err);
		if (U_FAILURE(err))
			return ret;

		pos = ubrk_first(bi);
		while (pos != UBRK_DONE) {
			int rules = ubrk_getRuleStatus(bi);
			if ((rules == UBRK_WORD_NONE) || (prev == -1)) {
				prev = pos;
			} else {
				lstring tmp = lconvert::from_unicode(uc.data() + prev, pos - prev);
				ret.emplace_back(tmp);

				prev = -1;
			}

			pos = ubrk_next(bi);
		}

		ubrk_close(bi);

		return ret;
	}

	std::vector<lstring> convert_split_words(const char *text, size_t size) {
		return convert_split_words(text, size, "");
	}

private:
	charset m_ch;
};

}} // namespace ioremap::ribosome

#endif // __RIBOSOME_SPLIT_HPP
