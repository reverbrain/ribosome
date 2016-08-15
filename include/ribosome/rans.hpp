#pragma once

#include "ribosome/rans_byte.h"
#include "ribosome/error.hpp"

#include <string>
#include <sstream>
#include <vector>

#include <msgpack.hpp>

#include <assert.h>

namespace msgpack {
static inline RansEncSymbol &operator >>(msgpack::object o, RansEncSymbol &renc)
{
	if (o.type != msgpack::type::ARRAY) {
		std::ostringstream ss;
		ss << "rans enc unpack: type: " << o.type <<
			", must be: " << msgpack::type::ARRAY <<
			", size: " << o.via.array.size;
		throw std::runtime_error(ss.str());
	}

	object *p = o.via.array.ptr;
	const uint32_t size = o.via.array.size;
	if (size != 5) {
		std::ostringstream ss;
		ss << "rans enc unpack: invalid array size: " << size << ", must be 5";
		throw std::runtime_error(ss.str());
	}

	p[0].convert(&renc.x_max);
	p[1].convert(&renc.rcp_freq);
	p[2].convert(&renc.bias);
	p[3].convert(&renc.cmpl_freq);
	p[4].convert(&renc.rcp_shift);

	return renc;
}

template <typename Stream>
inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const RansEncSymbol &renc)
{
	o.pack_array(5);
	o.pack(renc.x_max);
	o.pack(renc.rcp_freq);
	o.pack(renc.bias);
	o.pack(renc.cmpl_freq);
	o.pack(renc.rcp_shift);

	return o;
}

static inline RansDecSymbol &operator >>(msgpack::object o, RansDecSymbol &rdec)
{
	if (o.type != msgpack::type::ARRAY) {
		std::ostringstream ss;
		ss << "rans dec unpack: type: " << o.type <<
			", must be: " << msgpack::type::ARRAY <<
			", size: " << o.via.array.size;
		throw std::runtime_error(ss.str());
	}

	object *p = o.via.array.ptr;
	const uint32_t size = o.via.array.size;
	if (size != 2) {
		std::ostringstream ss;
		ss << "rans dec unpack: invalid array size: " << size << ", must be 2";
		throw std::runtime_error(ss.str());
	}

	p[0].convert(&rdec.start);
	p[1].convert(&rdec.freq);

	return rdec;
}

template <typename Stream>
inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const RansDecSymbol &rdec)
{
	o.pack_array(2);
	o.pack(rdec.start);
	o.pack(rdec.freq);

	return o;
}


} // namespace msgpack

namespace ioremap { namespace ribosome {

struct symbol_stats {
	std::vector<uint32_t> freqs;
	std::vector<uint32_t> cum_freqs;

	MSGPACK_DEFINE(freqs, cum_freqs);

	symbol_stats() {
		freqs.resize(256);
		cum_freqs.resize(257);
	}

	void count_freqs(uint8_t const* in, size_t nbytes) {
		for (size_t i=0; i < nbytes; i++)
			freqs[in[i]]++;
	}

	void calc_cum_freqs() {
		cum_freqs[0] = 0;
		for (int i=0; i < 256; i++)
			cum_freqs[i+1] = cum_freqs[i] + freqs[i];
	}

	void normalize_freqs(uint32_t target_total) {
		assert(target_total >= 256);

		calc_cum_freqs();
		uint32_t cur_total = cum_freqs[256];

		// resample distribution based on cumulative freqs
		for (int i = 1; i <= 256; i++)
			cum_freqs[i] = ((uint64_t)target_total * cum_freqs[i])/cur_total;

		// if we nuked any non-0 frequency symbol to 0, we need to steal
		// the range to make the frequency nonzero from elsewhere.
		// 
		// this is not at all optimal, i'm just doing the first thing that comes to mind.
		for (int i=0; i < 256; i++) {
			if (freqs[i] && cum_freqs[i+1] == cum_freqs[i]) {
				// symbol i was set to zero freq
				// find best symbol to steal frequency from (try to steal from low-freq ones)
				uint32_t best_freq = ~0u;
				int best_steal = -1;
				for (int j=0; j < 256; j++) {
					uint32_t freq = cum_freqs[j+1] - cum_freqs[j];
					if (freq > 1 && freq < best_freq) {
						best_freq = freq;
						best_steal = j;
					}
				}
				assert(best_steal != -1);

				// and steal from it!
				if (best_steal < i) {
					for (int j = best_steal + 1; j <= i; j++)
						cum_freqs[j]--;
				} else {
					assert(best_steal > i);
					for (int j = i + 1; j <= best_steal; j++)
						cum_freqs[j]++;
				}
			}
		}

		// calculate updated freqs and make sure we didn't screw anything up
		assert(cum_freqs[0] == 0 && cum_freqs[256] == target_total);
		for (int i=0; i < 256; i++) {
			if (freqs[i] == 0) {
				assert(cum_freqs[i+1] == cum_freqs[i]);
			} else {
				assert(cum_freqs[i+1] > cum_freqs[i]);
			}

			// calc updated freq
			freqs[i] = cum_freqs[i+1] - cum_freqs[i];
		}
	}
};

class rans {
public:
	rans() : m_prob_scale(1 << m_prob_bits) {
		m_cum2sym.resize(m_prob_scale);
		m_esyms.resize(256);
		m_dsyms.resize(256);
	}

	void gather_stats(const uint8_t *data, size_t size) {
		m_stats.count_freqs(data, size);
	}

	std::string save_stats() {
		m_stats.normalize_freqs(m_prob_scale);

		for (int s=0; s < 256; s++) {
			for (uint32_t i = m_stats.cum_freqs[s]; i < m_stats.cum_freqs[s+1]; i++) {
				m_cum2sym[i] = s;
			}
		}

		for (int i=0; i < 256; i++) {
			RansEncSymbolInit(&m_esyms[i], m_stats.cum_freqs[i], m_stats.freqs[i], m_prob_bits);
			RansDecSymbolInit(&m_dsyms[i], m_stats.cum_freqs[i], m_stats.freqs[i]);

#if 0
			printf("%d/%d: enc: x_max: %u, rcp_freq: %u, bias: %u, cmpl_freq: %d, rcp_shift: %d\n",
					i, 256, m_esyms[i].x_max, m_esyms[i].rcp_freq, m_esyms[i].bias,
					m_esyms[i].cmpl_freq, m_esyms[i].rcp_shift);
#endif
		}

		m_normalized = true;

		std::stringstream buffer;
		msgpack::pack(buffer, *this);
		buffer.seekg(0);
		return buffer.str();
	}

	ribosome::error_info load_stats(const char *data, size_t size) {
		msgpack::unpacked msg;
		try {
			msgpack::unpack(&msg, data, size);

			msg.get().convert(this);
		} catch (const std::exception &e) {
			std::ostringstream ss;
			ss << msg.get();
			return ribosome::create_error(-EINVAL, "could not unpack data, size: %ld, value: %s, error: %s",
					size, ss.str().c_str(), e.what());
		}

#if 0
		for (int i=0; i < 256; i++) {
			printf("%d/%d: enc: x_max: %u, rcp_freq: %u, bias: %u, cmpl_freq: %d, rcp_shift: %d\n",
					i, 256, m_esyms[i].x_max, m_esyms[i].rcp_freq, m_esyms[i].bias,
					m_esyms[i].cmpl_freq, m_esyms[i].rcp_shift);
		}
#endif
		m_normalized = true;
		return ribosome::error_info();
	}

	ribosome::error_info encode_bytes(const uint8_t *data, size_t size, std::vector<uint8_t> *ret, size_t *offset) {
		if (!m_normalized) {
			return ribosome::create_error(-EROFS, "trying to encode data, but encoder is not normalized");
		}

		ret->resize(size + 128);
		uint8_t *ptr = ret->data() + ret->size(); // points 1 byte past the end of the buffer, will be decremented internally

		RansState rans;
		RansEncInit(&rans);

		for (size_t i = 0; i < size; ++i) {
			size_t rev_pos = size - 1 - i;
			uint8_t s = data[rev_pos];

			long diff = ptr - ret->data();
			if (diff < 0) {
				return ribosome::create_error(-E2BIG, "%ld/%ld: ptr: %p, start: %p, diff: %ld, char: %d\n",
						i, size, ptr, ret->data(), diff, s);
			}
			RansEncPutSymbol(&rans, &ptr, &m_esyms[s]);
		}

		//printf("encode: %ld -> %ld\n", size, ret.size() - (ptr - ret.data()));

		RansEncFlush(&rans, &ptr);
		*offset = ptr - ret->data();
		return ribosome::error_info();
	}

	ribosome::error_info decode_bytes(const uint8_t *data, size_t size, std::vector<uint8_t> *ret) {
		if (!m_normalized) {
			return ribosome::create_error(-EROFS, "trying to decode data, but decoder is not normalized");
		}

		uint8_t *ptr = (uint8_t *)data;

		RansState rans;
		RansDecInit(&rans, &ptr);

		for (size_t i = 0; i < ret->size(); i++) {
			uint8_t s = m_cum2sym[RansDecGet(&rans, m_prob_bits)];
			(*ret)[i] = s;

			long diff = ptr - data;
			if (diff > (long)size) {
				return ribosome::create_error(-E2BIG,
					"ptr: %p, data: %p, diff: %ld, input size: %ld: decoder runs out of input data",
					ptr, data, diff, size);
			}

			RansDecAdvanceSymbol(&rans, &ptr, &m_dsyms[s], m_prob_bits);
		}

		return ribosome::error_info();
	}

	template <typename Stream>
	void msgpack_pack(msgpack::packer<Stream> &o) const {
		o.pack_array(5);
		o.pack(m_prob_bits);
		o.pack(m_stats);
		o.pack(m_cum2sym);
		o.pack(m_esyms);
		o.pack(m_dsyms);
	}

	void msgpack_unpack(msgpack::object o) {
		if (o.type != msgpack::type::ARRAY) {
			std::ostringstream ss;
			ss << "could not unpack document, object type is " << o.type <<
				", must be array (" << msgpack::type::ARRAY << ")";
			throw std::runtime_error(ss.str());
		}

		msgpack::object *p = o.via.array.ptr;
		p[0].convert(&m_prob_bits);
		m_prob_scale = 1 << m_prob_bits;

		p[1].convert(&m_stats);
		p[2].convert(&m_cum2sym);
		p[3].convert(&m_esyms);
		p[4].convert(&m_dsyms);
	}

private:
	bool m_normalized = false;

	uint32_t m_prob_bits = 14;
	uint32_t m_prob_scale;

	symbol_stats m_stats;
	std::vector<uint8_t> m_cum2sym;

	std::vector<RansEncSymbol> m_esyms;
	std::vector<RansDecSymbol> m_dsyms;
};

}} // namespace ioremap::ribosome
