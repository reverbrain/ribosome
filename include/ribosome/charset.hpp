#ifndef __RIBOSOME_CHARSET_HPP
#define __RIBOSOME_CHARSET_HPP

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

	void convert(UChar *dst, size_t *dst_size, const char *src, size_t src_size) {
		UErrorCode err = U_ZERO_ERROR;

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

private:
	UCharsetDetector* m_csd;
};

}} // namespace ioremap::ribosome

#endif // __RIBOSOME_CHARSET_HPP
