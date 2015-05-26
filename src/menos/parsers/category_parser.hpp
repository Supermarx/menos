#pragma once

#include <functional>
#include <stdexcept>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/optional.hpp>

#include <supermarx/scraper/util.hpp>

namespace supermarx
{
	class category_parser : public html_parser::default_handler
	{
	public:
		typedef std::string category_uri_t;
		typedef std::function<void(category_uri_t)> category_callback_t;

	private:
		category_callback_t category_callback;

	public:
		category_parser(category_callback_t category_callback_)
		: category_callback(category_callback_)
		{}

		template<typename T>
		void parse(T source)
		{
			html_parser::parse(source, *this);
		}

		virtual void startElement(const std::string& /* namespaceURI */, const std::string& /* localName */, const std::string& qName, const AttributesT& atts)
		{
			if(qName == "a" && util::contains_attr("ish-pageNavigation-filters-filter-item-name", atts.getValue("class")))
				category_callback(atts.getValue("href"));
		}
	};
}
