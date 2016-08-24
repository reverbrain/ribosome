#ifndef __RIBOSOME_SPLIT_HPP
#define __RIBOSOME_SPLIT_HPP

#include "ribosome/alphabet.hpp"
#include "ribosome/lstring.hpp"

#include <string>
#include <vector>
#include <unordered_map>

#include <unicode/ubrk.h>

namespace ioremap { namespace ribosome {

class split {
public:

	std::vector<lstring> convert_split_words_allow_alphabet(const lstring &lt, const alphabet &allow) {
		lstring copy;
		lstring *ptr = (lstring *)&lt;

		bool changed = false;
		static letter space(' ');
		for (size_t i = 0; i < lt.size(); ++i) {
			if (!allow.ok(lt[i])) {
				if (!changed) {
					copy = lt;
					ptr = &copy;

					changed = true;
				}

				copy[i] = space;
			}
		}

		return convert_split_words(*ptr);
	}

	std::vector<lstring> convert_split_words_drop_alphabet(const lstring &lt, const alphabet &drop) {
		lstring copy;
		lstring *ptr = (lstring *)&lt;


		bool changed = false;
		static letter space(' ');
		for (size_t i = 0; i < lt.size(); ++i) {
			if (drop.ok(lt[i])) {
				if (!changed) {
					copy = lt;
					ptr = &copy;

					changed = true;
				}

				copy[i] = space;
			}
		}

		return convert_split_words(*ptr);
	}

	std::vector<lstring> convert_split_words(const lstring &lt) {
		std::vector<lstring> ret;

		UBreakIterator* bi;
		int prev = -1, pos;

		UErrorCode err = U_ZERO_ERROR;
		bi = ubrk_open(UBRK_WORD, get_locale(), (UChar *)lt.data(), lt.size(), &err);
		if (U_FAILURE(err))
			return ret;

		pos = ubrk_first(bi);
		while (pos != UBRK_DONE) {
			int rules = ubrk_getRuleStatus(bi);
			if ((rules == UBRK_WORD_NONE) || (prev == -1)) {
				prev = pos;
			} else {
				ret.emplace_back(lt.substr(prev, pos - prev));

				prev = -1;
			}

			pos = ubrk_next(bi);
		}

		ubrk_close(bi);

		return ret;
	}

	std::vector<lstring> convert_split_words(const lstring &lt, const std::string &drop) {
		alphabet d(drop);
		return convert_split_words_drop_alphabet(lt, d);
	}

	std::vector<lstring> convert_split_words(const char *text, size_t size, const std::string &drop) {
		lstring lt = ribosome::lconvert::from_utf8(text, size);
		return convert_split_words(lt, drop);
	}

	std::vector<lstring> convert_split_words(const char *text, size_t size) {
		lstring lt = ribosome::lconvert::from_utf8(text, size);
		return convert_split_words(lt);
	}

private:
};

}} // namespace ioremap::ribosome

#endif // __RIBOSOME_SPLIT_HPP
