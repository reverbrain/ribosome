#pragma once

#include <buffio.h>
#include <tidy.h>
#include <tidyenum.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace ioremap { namespace ribosome {

class html_parser {
public:
	html_parser() {
	}

	~html_parser() {
	}

	void feed_file(const char *path) {
		reset();

		std::ifstream in(path);
		if (in.bad()) {
			std::ostringstream ss;
			ss << "parser: could not feed file '" << path << "': " << in.rdstate();
			throw std::runtime_error(ss.str());
		}

		std::ostringstream ss;

		ss << in.rdbuf();

		feed_text(ss.str());
	}

	void feed_text(const char *data, size_t size) {
		reset();

		if (size == 0)
			return;

		TidyDoc tdoc = tidyCreate();

		TidyBuffer errbuf, buf;
		tidyBufInit(&errbuf);
		tidyBufInit(&buf);

		tidySetCharEncoding(tdoc, "raw");
		tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
		tidySetErrorBuffer(tdoc, &errbuf);

		tidyBufAttach(&buf, (byte *)data, size);

		int err = tidyParseBuffer(tdoc, &buf);
		if (err < 0) {
			tidyBufFree(&errbuf);
			tidyBufDetach(&buf);
			tidyBufFree(&buf);
			tidyRelease(tdoc);

			std::ostringstream ss;
			ss << "parser: failed to parse page: " << err;
			throw std::runtime_error(ss.str());
		}

		parse_doc(tdoc);

		tidyBufDetach(&buf);
		tidyBufFree(&buf);
		tidyBufFree(&errbuf);

		tidyRelease(tdoc);
	}

	void feed_text(const std::string &page) {
		feed_text(page.c_str(), page.size());
	}

	const std::vector<std::string> urls(void) const {
		std::vector<std::string> ret;
		ret.insert(ret.end(), m_links.begin(), m_links.end());
		ret.insert(ret.end(), m_images.begin(), m_images.end());
		return ret;
	}

	const std::vector<std::string> &links(void) const {
		return m_links;
	}
	const std::vector<std::string> &images(void) const {
		return m_images;
	}

	const std::vector<std::string> &tokens(void) const {
		return m_tokens;
	}

	std::string text(const char *join) const {
		std::ostringstream ss;

		std::copy(m_tokens.begin(), m_tokens.end(), std::ostream_iterator<std::string>(ss, join));
		return std::move(ss.str());
	}

private:
	std::vector<std::string> m_links, m_images;
	std::vector<std::string> m_tokens;

	void reset(void) {
		m_links.clear();
		m_images.clear();
		m_tokens.clear();
	}

	void parse_doc(TidyDoc tdoc) {
		traverse_tree(tdoc, tidyGetRoot(tdoc));
	}

	std::string get_url(TidyAttr attr) {
		if (attr) {
			ctmbstr value = tidyAttrValue(attr);
			if (value) {
				return std::string(value);
			}
		}

		return std::string();
	}

	void traverse_tree(TidyDoc tdoc, TidyNode tnode) {
		TidyNode child;
		TidyBuffer buf;
		std::string url;

		for (child = tidyGetChild(tnode); child; child = tidyGetNext(child)) {
			bool inside_ignored = false;

			switch (tidyNodeGetType(child)) {
				case TidyNode_Text:
					tidyBufInit(&buf);

					tidyNodeGetText(tdoc, child, &buf);
					m_tokens.emplace_back(std::string((char *)buf.bp, buf.size));

					tidyBufFree(&buf);
					break;
				// only Start event is ever emitted for html, probably startend event is emitted for some xml
				// but end event is never emitted, instead start event should be considered as 'new tag event' event
				// if we dig into this node we will traverse over its children, if we skip this node we kind of jump
				// to the next char after this tag is closed
				case TidyNode_Start:
				case TidyNode_StartEnd:
				case TidyNode_End:
				default:
					switch (tidyNodeGetId(child)) {
						case TidyTag_SCRIPT:
						case TidyTag_STYLE:
							inside_ignored = true;
							break;
						case TidyTag_A:
							url = get_url(tidyAttrGetById(child, TidyAttr_HREF));
							m_links.push_back(url);
							break;

						case TidyTag_IFRAME:
						case TidyTag_IMG:
							url = get_url(tidyAttrGetById(child, TidyAttr_SRC));
							m_images.push_back(url);
							break;
						default:
							break;
					}
					break;
			}

			if (!inside_ignored) {
				traverse_tree(tdoc, child);
			}
		}
	}
};

}} // namespace ioremap::ribosome
