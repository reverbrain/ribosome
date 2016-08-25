#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include "ribosome/lstring.hpp"

namespace ioremap { namespace ribosome {

static const std::string drop_characters = "`~1234567890-=!@#$%^&*()_+[]\\{}|';\":/.,?><\n\r\t";

class alphabet {
public:
	alphabet(){}

	alphabet(const std::string &a) : alphabet(lconvert::from_utf8(a)) {
	}

	alphabet(const lstring &lw) {
		for (auto ch: lw) {
			m_alphabet[ch] = 1;
		}
	}

	bool ok(const lstring &lw) const {
		if (m_alphabet.empty())
			return true;

		for (auto ch: lw) {
			if (!ok(ch)) {
				return false;
			}
		}

		return true;
	}

	bool ok(const letter &l) const {
		return m_alphabet.find(l) != m_alphabet.end();
	}

	size_t merge(const std::string &a) {
		ribosome::lstring lw = ribosome::lconvert::from_utf8(a);
		return merge(lw);
	}

	size_t merge(const ribosome::lstring &lw) {
		size_t num = 0;
		for (auto ch: lw) {
			auto it = m_alphabet.find(ch);
			if (it == m_alphabet.end()) {
				m_alphabet[ch] = 1;
				num++;
			} else {
				it->second++;
			}
		}

		return num;
	}

	size_t merge(const alphabet &other) {
		size_t num = 0;
		for (auto &p: other.m_alphabet) {
			auto it = m_alphabet.find(p.first);
			if (it == m_alphabet.end()) {
				m_alphabet[p.first] = 1;
				num++;
			} else {
				it->second++;
			}
		}

		return num;
	}

private:
	std::unordered_map<ribosome::letter, int, ribosome::letter_hash> m_alphabet;
};

class alphabets_checker {
public:
	void add(const std::string &lang, const std::string &a) {
		m_alphabets.emplace(std::pair<std::string, alphabet>(lang, alphabet(a)));
	}

	bool ok(const std::string &lang, const ribosome::lstring &lw) {
		auto it = m_alphabets.find(lang);
		if (it == m_alphabets.end())
			return true;

		return it->second.ok(lw);
	}
private:
	std::map<std::string, alphabet> m_alphabets;
};

class numbers_alphabet: public alphabet {
public:
	numbers_alphabet() : alphabet("1234567890") {}
};
class russian_alphabet: public alphabet {
public:
	russian_alphabet() : alphabet("абвгдеёжзийклмнопрстуфхцчшщъыьэюя") {}
};
class english_alphabet: public alphabet {
public:
	english_alphabet() : alphabet("absdefghijklmnopqrstuvwxyz") {}
};
class german_alphabet: public alphabet {
public:
	german_alphabet() : alphabet("absdefghijklmnopqrstuvwxyzäöüß") {}
};
class french_alphabet: public alphabet {
public:
	french_alphabet() : alphabet("absdefghijklmnopqrstuvwxyzéèçëòôöùàâ") {}
};

}} // namespace ioremap::warp
