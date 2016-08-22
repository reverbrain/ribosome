#pragma once

#include "ribosome/error.hpp"
#include "ribosome/lstring.hpp"

#include <expat.h>

#include <deque>
#include <condition_variable>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace ioremap { namespace ribosome {

typedef std::map<std::string, std::vector<std::string>> attributes_map_t;
struct element {
	std::vector<XML_Char> chars;
	attributes_map_t attrs;
	std::string name;
	int thread_num;
};

class xml_parser
{
public:
	xml_parser(int n = 1) {
		m_parser = XML_ParserCreate(NULL);
		if (!m_parser) {
			ribosome::throw_error(-ENOMEM, "could not create XML parser");
		}

		XML_SetUserData(m_parser, this);
		XML_SetElementHandler(m_parser, xml_start, xml_end);
		XML_SetCharacterDataHandler(m_parser, xml_characters);

		m_current.reset(new element);
		for (int i = 0; i < n; ++i) {
			m_pool.emplace_back(std::thread(std::bind(&xml_parser::callback, this, i)));
		}
	}

	~xml_parser() {
		m_need_exit = true;
		m_pool_wait.notify_all();
		for (auto &t: m_pool) {
			t.join();
		}

		XML_ParserFree(m_parser);
	}

	ribosome::error_info feed_data(const char *data, size_t size) {
		int err = XML_Parse(m_parser, data, size, size == 0);
		if (err == XML_STATUS_ERROR) {
			return ribosome::create_error(err, "could not parse chunk: size: %ld, last: %d, line: %ld, error: %s",
					size, size == 0,
					XML_GetCurrentLineNumber(m_parser),
					XML_ErrorString(XML_GetErrorCode(m_parser)));
		}

		return ribosome::error_info();
	}

private:
	XML_Parser m_parser;
	std::unique_ptr<element> m_current;

	bool m_need_exit = false;
	std::mutex m_lock;
	std::deque<std::unique_ptr<element>> m_elements;
	std::condition_variable m_pool_wait, m_parser_wait;
	std::vector<std::thread> m_pool;

	static void xml_start(void *data, const char *el, const char **attr) {
	xml_parser *p = (xml_parser *)data;

		p->m_current->name.assign(el);

		for (size_t i = 0; attr[i]; i += 2) {
			std::string aname(attr[i]);
			std::string aval(attr[i+1]);

			auto it = p->m_current->attrs.find(aname);
			if (it == p->m_current->attrs.end()) {
				p->m_current->attrs[aname] = std::vector<std::string>({aval});
			} else {
				it->second.emplace_back(aval);
			}
		}

		p->on_start(*p->m_current);
	}
	static void xml_end(void *data, const char *el) {
		xml_parser *p = (xml_parser *)data;

		(void)el;

		p->on_end(*p->m_current);
		p->element_is_ready();
	}

	static void xml_characters(void *data, const XML_Char *s, int len) {
		xml_parser *p = (xml_parser *)data;

		p->m_current->chars.insert(p->m_current->chars.end(), s, s + len);
	}

	void element_is_ready() {
		std::unique_lock<std::mutex> guard(m_lock);
		m_elements.emplace_back(std::move(m_current));
		m_current.reset(new element);

		if (m_elements.size() > m_pool.size() * 2) {
			m_parser_wait.wait(guard, [&] {return m_elements.size() < m_pool.size();});
		}

		guard.unlock();
		m_pool_wait.notify_one();
	}

	void callback(int num) {
		while (!m_need_exit) {
			std::unique_lock<std::mutex> guard(m_lock);
			m_pool_wait.wait_for(guard, std::chrono::milliseconds(100), [&] {return !m_elements.empty();});

			while (!m_elements.empty()) {
				std::unique_ptr<element> elm = std::move(m_elements.front());
				m_elements.pop_front();
				guard.unlock();
				m_parser_wait.notify_one();

				elm->thread_num = num;
				on_element(*elm);

				guard.lock();
			}
		}
	}

protected:
	virtual void on_element(const element &elm) {
		(void) elm;
	}

	virtual void on_start(const element &elm) {
		(void) elm;
	}
	virtual void on_end(const element &elm) {
		(void) elm;
	}
};

}} // namespace ioremap::ribosome
