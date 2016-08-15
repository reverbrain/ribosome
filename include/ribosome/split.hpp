#ifndef __RIBOSOME_SPLIT_HPP
#define __RIBOSOME_SPLIT_HPP

#include "ribosome/charset.hpp"
#include "ribosome/lstring.hpp"

#include <string>
#include <vector>
#include <unordered_map>

#include <unicode/ubrk.h>

namespace ioremap { namespace ribosome {

class split {
public:
	std::vector<lstring> convert_split_words(const char *text, size_t size, const std::string &drop) {
		std::vector<UChar> uc(size+1);

		size_t usize = uc.size();

		m_ch.convert((UChar *)uc.data(), &usize, text, size);
		uc.resize(usize);

		if (drop.size()) {
			std::vector<UChar> drop_chars(drop.size() + 1);
			size_t dsize = drop_chars.size();

			m_ch.convert((UChar *)drop_chars.data(), &dsize, drop.data(), drop.size());
			drop_chars.resize(dsize);

			std::unordered_map<UChar, int> m;
			for (size_t i = 0; i < dsize; ++i) {
				m[drop_chars[i]] = 1;
			}

			static UChar space = ' ';
			for (size_t i = 0; i < uc.size(); ++i) {
				auto it = m.find(uc[i]);
				if (it != m.end()) {
					uc[i] = space;
				}
			}
		}

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
				for (auto it = tmp.begin(); it != tmp.end(); ++it) {
				}

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
