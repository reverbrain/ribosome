#ifndef __RIBOSOME_CHARSET_HPP
#define __RIBOSOME_CHARSET_HPP

#include <ribosome/lstring.hpp>

#include <unicode/ucsdet.h>

#include <iostream>
#include <stdexcept>

namespace ioremap { namespace ribosome {

class charset {
public:
	charset() {
		UErrorCode status = U_ZERO_ERROR;

		m_csd = ucsdet_open(&status);
		if (U_FAILURE(status)) {
			throw std::runtime_error("could not open charset detector: " + std::string(u_errorName(status)));
		}
	}

	~charset() {
		ucsdet_close(m_csd);
	}

	std::string detect(const std::string &text, const std::string &enc_hint) {
		UErrorCode status = U_ZERO_ERROR;

		if (enc_hint.size() != 0)
			ucsdet_setDeclaredEncoding(m_csd, enc_hint.c_str(), enc_hint.size(), &status);

		ucsdet_setText(m_csd, text.c_str(), text.size(), &status);
		ucsdet_enableInputFilter(m_csd, TRUE);

		const UCharsetMatch *ucm = ucsdet_detect(m_csd, &status);

		const char *name = ucsdet_getName(ucm, &status);
		if (U_FAILURE(status)) {
			return enc_hint;
		}

		return std::string(name);
	}

	void convert(UChar *dst, int *dst_size, const char *src, size_t src_size, const std::string &enc_hint) {
		UErrorCode err = U_ZERO_ERROR;

		if (enc_hint.size() != 0)
			ucsdet_setDeclaredEncoding(m_csd, enc_hint.c_str(), enc_hint.size(), &err);

		ucsdet_setText(m_csd, src, src_size, &err);
		ucsdet_enableInputFilter(m_csd, TRUE);

		const UCharsetMatch *ucm = ucsdet_detect(m_csd, &err);
		if (U_FAILURE(err)) {
			*dst_size = 0;
			return;
		}

		*dst_size = ucsdet_getUChars(ucm, dst, *dst_size, &err);
		if (U_FAILURE(err)) {
			*dst_size = 0;
			return;
		}
	}
#if 0
	std::vector<ribosome::lstring> convert(const char *text, size_t size) {
		UErrorCode err = U_ZERO_ERROR;

		if (declared_encoding.size() != 0)
			ucsdet_setDeclaredEncoding(m_csd, enc_hint.c_str(), enc_hint.size(), &err);

		ucsdet_setText(m_csd, text, size, &err);
		ucsdet_enableInputFilter(m_csd, TRUE);

		ucm = ucsdet_detect(m_csd, &err);

		std::vector<UChar> buf(size + 1); // needed for ending NULL-byte

		int len = ucsdet_getUChars(ucm, const_cast<UChar *>(buf.data()), buf.size(), &err);
		buf.resize(len);

		UBreakIterator* bi;
		int pos;

		bi = ubrk_open(UBRK_WORD, 0, s, len, &err);
		if (U_FAILURE(err))
			return;

		pos = ubrk_first(bi);
		while (pos != UBRK_DONE) {
			printf("Boundary at position %d\n", pos);
			pos = ubrk_next(bi);
		}

		ubrk_close(bi);
	}
#endif

private:
	UCharsetDetector* m_csd;
};

}} // namespace ioremap::ribosome

#endif // __RIBOSOME_CHARSET_HPP
