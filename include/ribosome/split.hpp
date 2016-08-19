#ifndef __RIBOSOME_SPLIT_HPP
#define __RIBOSOME_SPLIT_HPP

#include "ribosome/lstring.hpp"

#include <string>
#include <vector>
#include <unordered_map>

#include <unicode/ubrk.h>

namespace ioremap { namespace ribosome {

class split {
public:
	std::vector<lstring> convert_split_words(const lstring &lt, const std::string &drop) {
		lstring copy;
		lstring *ptr = (lstring *)&lt;

		if (drop.size()) {
			lstring ld = lconvert::from_utf8(drop);

			std::unordered_map<letter, int, letter_hash> m;
			for (auto &ch: ld) {
				m[ch] = 1;
			}

			bool changed = false;
			static letter space(' ');
			for (size_t i = 0; i < copy.size(); ++i) {
				auto it = m.find(lt[i]);
				if (it != m.end()) {
					if (!changed) {
						copy = lt;
						ptr = &copy;

						changed = true;
					}

					copy[i] = space;
				}
			}
		}

		std::vector<lstring> ret;

		UBreakIterator* bi;
		int prev = -1, pos;

		UErrorCode err = U_ZERO_ERROR;
		bi = ubrk_open(UBRK_WORD, get_locale(), (UChar *)ptr->data(), ptr->size(), &err);
		if (U_FAILURE(err))
			return ret;

		pos = ubrk_first(bi);
		while (pos != UBRK_DONE) {
			int rules = ubrk_getRuleStatus(bi);
			if ((rules == UBRK_WORD_NONE) || (prev == -1)) {
				prev = pos;
			} else {
				ret.emplace_back(ptr->substr(prev, pos - prev));

				prev = -1;
			}

			pos = ubrk_next(bi);
		}

		ubrk_close(bi);

		return ret;
	}

	std::vector<lstring> convert_split_words(const char *text, size_t size, const std::string &drop) {
		lstring lt = ribosome::lconvert::from_utf8(text, size);
		return convert_split_words(lt, drop);
	}

	std::vector<lstring> convert_split_words(const char *text, size_t size) {
		return convert_split_words(text, size, "");
	}

private:
};

}} // namespace ioremap::ribosome

#endif // __RIBOSOME_SPLIT_HPP
