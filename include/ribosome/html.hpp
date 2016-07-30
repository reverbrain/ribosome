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

	void feed_text(const std::string &page) {
		reset();

		if (page.size() == 0)
			return;

		TidyBuffer errbuf;
		TidyDoc tdoc = tidyCreate();

		tidyBufInit(&errbuf);

		tidySetCharEncoding(tdoc, "raw");
		tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
		tidySetErrorBuffer(tdoc, &errbuf);

		int err = tidyParseString(tdoc, page.c_str());
		if (err < 0) {
			tidyBufFree(&errbuf);
			std::ostringstream ss;
			ss << "parser: failed to parse page: " << err;
			throw std::runtime_error(ss.str());
		}

		parse_doc(tdoc);

		tidyBufFree(&errbuf);
		tidyRelease(tdoc);
	}

	const std::vector<std::string> &urls(void) const {
		return m_urls;
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
	std::vector<std::string> m_urls;
	std::vector<std::string> m_tokens;

	void reset(void) {
		m_urls.clear();
		m_tokens.clear();
	}

	void parse_doc(TidyDoc tdoc) {
		traverse_tree(tdoc, tidyGetRoot(tdoc));
	}

	void get_url(TidyAttr attr) {
		if (attr) {
			ctmbstr value = tidyAttrValue(attr);
			if (value) {
				m_urls.emplace_back(std::string(value));
			}
		}
	}

	void traverse_tree(TidyDoc tdoc, TidyNode tnode) {
		TidyNode child;
		TidyBuffer buf;

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
							get_url(tidyAttrGetById(child, TidyAttr_HREF));
							break;

						case TidyTag_IFRAME:
						case TidyTag_IMG:
							get_url(tidyAttrGetById(child, TidyAttr_SRC));
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
