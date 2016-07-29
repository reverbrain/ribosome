#include "ribosome/lstring.hpp"

#include <unicode/uloc.h>

static char *__ribosome_locale;

namespace ioremap { namespace ribosome {

void set_locale(const char *l)
{
	__ribosome_locale = strdup(l);
}

char *get_locale()
{
	return __ribosome_locale;
}

}} // namespace ioremap::ribosome
