/*
 * Copyright 2014+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __RIBOSOME_LSTRING_HPP
#define __RIBOSOME_LSTRING_HPP

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <unicode/uchar.h>
#include <unicode/ucnv.h>
#include <unicode/ustring.h>

#include <string.h>

namespace ioremap { namespace ribosome {

void set_locale(const char *l);
char *get_locale();

typedef UChar basic_letter;

template <typename T>
struct base_letter {
	T l;

	base_letter() : l(0) {}
	base_letter(const T &_l) : l(_l) {}
	base_letter(const base_letter &other) {
		l = other.l;
	}

	std::string str() const {
		char tmp[8 + 1];
		UErrorCode err = U_ZERO_ERROR;
		UConverter *conv = ucnv_open("utf8", &err);
		int len = ucnv_fromUChars(conv, tmp, sizeof(tmp) - 1, &l, 1, &err);
		tmp[len] = '\0';
		ucnv_close(conv);

		return tmp;
	}

	bool operator==(const base_letter &other) const {
		return l == other.l;
	}
	bool operator!=(const base_letter &other) const {
		return !operator==(other);
	}
	bool operator<(const base_letter &other) const {
		return l < other.l;
	}
} __attribute__ ((packed));

typedef base_letter<basic_letter> letter;

struct letter_hash {
	unsigned long operator()(const letter &l) const {
		return l.l;
	}
};

inline std::ostream &operator <<(std::ostream &out, const letter &l)
{
	out << l.str();
	return out;
}

template <typename T>
struct letter_traits {
	typedef base_letter<T> char_type;
	typedef base_letter<T> int_type;
	typedef std::streampos pos_type;
	typedef std::streamoff off_type;
	typedef std::mbstate_t state_type;

	static void assign(char_type &c1, const char_type &c2) {
		c1 = c2;
	}

	static bool eq(const char_type &c1, const char_type &c2) {
		return c1.l == c2.l;
	}

	static bool lt(const char_type &c1, const char_type &c2) {
		return c1.l < c2.l;
	}

	static int compare(const char_type *s1, const char_type *s2, std::size_t n) {
		for (std::size_t i = 0; i < n; ++i) {
			if (eq(s1[i], char_type())) {
				if (eq(s2[i], char_type())) {
					return 0;
				}

				return -1;
			}

			if (lt(s1[i], s2[i]))
				return -1;
			else if (lt(s2[i], s1[i]))
				return 1;
		}

		return 0;
	}

	static std::size_t length(const char_type* s) {
		std::size_t i = 0;

		while (!eq(s[i], char_type()))
			++i;

		return i;
	}

	static const char_type *find(const char_type *s, std::size_t n, const char_type& a) {
		for (std::size_t i = 0; i < n; ++i)
			if (eq(s[i], a))
				return s + i;
		return 0;
	}

	static char_type *move(char_type *s1, const char_type *s2, std::size_t n) {
		return static_cast<char_type *>(memmove(s1, s2, n * sizeof(char_type)));
	}

	static char_type *copy(char_type *s1, const char_type *s2, std::size_t n) {
		std::copy(s2, s2 + n, s1);
		return s1;
	}

	static char_type *assign(char_type *s, std::size_t n, char_type a) {
		std::fill_n(s, n, a);
		return s;
	}

	static char_type to_char_type(const int_type &c) {
		return static_cast<char_type>(c);
	}

	static int_type to_int_type(const char_type &c) {
		return static_cast<int_type>(c);
	}

	static bool eq_int_type(const int_type &c1, const int_type &c2) {
		return c1.l == c2.l;
	}

	static int_type eof() {
		return static_cast<int_type>(~0U);
	}

	static int_type not_eof(const int_type &c) {
		return !eq_int_type(c, eof()) ? c : to_int_type(char_type());
	}
};

typedef std::basic_string<letter, letter_traits<basic_letter>> lstring;

inline std::ostream &operator <<(std::ostream &out, const lstring &ls)
{
	char tmp[ls.size() * 4 + 1];
	out << u_austrncpy(tmp, (UChar *)ls.data(), sizeof(tmp));
	return out;
}


class lconvert {
	public:
		static lstring from_unicode(const UChar *text, size_t size) {
			lstring ret;
			ret.resize(size);
			u_memcpy((UChar *)ret.data(), text, size);
			return ret;
		}

		static lstring from_utf8(const char *text, size_t size) {
			lstring ret;
			ret.resize(size + 1);

			UErrorCode err = U_ZERO_ERROR;
			int real_size = 0;
			u_strFromUTF8Lenient((UChar *)ret.data(), ret.size(), &real_size, text, size, &err);
			ret.resize(real_size);

			return ret;
		}

		static lstring from_utf8(const std::string &text) {
			return from_utf8(text.c_str(), text.size());
		}

		static std::string to_string(const std::string &l) {
			return l;
		}

		static std::string to_string(const lstring &l) {
			std::string ret;
			ret.resize(l.size() * 2 + 1);

			int rs = 0;
			UErrorCode err = U_ZERO_ERROR;
			u_strToUTF8((char *)ret.data(), ret.size(), &rs, (const UChar *)l.data(), l.size(), &err);
			ret.resize(rs);
			return ret;
		}

		static lstring to_lower(const lstring &ls) {
			lstring ret;
			ret.resize(ls.size());

			UErrorCode err = U_ZERO_ERROR;
			u_strToLower((UChar *)ret.data(), ret.size(), (UChar *)ls.data(), ls.size(), get_locale(), &err);
			return ret;
		}

		static std::string string_to_lower(const char *text, size_t size) {
			lstring ls = from_utf8(text, size);
			lstring lower = to_lower(ls);
			return to_string(lower);
		}

		static std::string string_to_lower(const std::string &str) {
			return string_to_lower(str.data(), str.size());
		}

		static lstring to_upper(const lstring &ls) {
			lstring ret;
			ret.resize(ls.size());

			UErrorCode err = U_ZERO_ERROR;
			u_strToUpper((UChar *)ret.data(), ret.size(), (UChar *)ls.data(), ls.size(), get_locale(), &err);
			return ret;
		}

		static std::string string_to_upper(const char *text, size_t size) {
			lstring ls = from_utf8(text, size);
			lstring lower = to_upper(ls);
			return to_string(lower);
		}

		static std::string string_to_upper(const std::string &str) {
			return string_to_lower(str.data(), str.size());
		}
};

}} // ioremap::ribosome

#endif /* __RIBOSOME_LSTRING_HPP */
