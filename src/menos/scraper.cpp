#include <menos/scraper.hpp>

#include <iostream>
#include <deque>
#include <fstream>
#include <boost/locale.hpp>

#include <supermarx/util/stubborn.hpp>

#include <menos/parsers/product_parser.hpp>
#include <menos/parsers/category_parser.hpp>

namespace supermarx
{
	scraper::scraper(callback_t _callback, size_t _ratelimit, bool _cache)
	: callback(_callback)
	, dl("supermarx menos/1.0", _ratelimit, _cache ? boost::optional<std::string>("./cache") : boost::none)
	{}

	void scraper::scrape()
	{
		std::stack<category_parser::category_uri_t> todo;
		std::set<category_parser::category_uri_t> cats;

		auto add_f([&](category_parser::category_uri_t const& _curi)
		{
			if(!cats.emplace(_curi).second) // If already added _curi
				return;

			todo.push(_curi);
		});

		add_f("http://www.plus.nl/assortiment");

		category_parser cp([&](category_parser::category_uri_t const& _curi)
		{
			if(boost::algorithm::contains(_curi, "?SearchParameter=")) // Skip if expanded search query
				return;

			add_f(_curi);
		});

		while(!todo.empty())
		{
			auto _curi = todo.top();
			todo.pop();

			size_t product_i = 0;
			product_parser pp([&](const product& p, boost::optional<std::string> const& _image_uri, datetime retrieved_on, confidence conf, problems_t probs)
			{
				++product_i;
				callback(_curi, _image_uri, p, retrieved_on, conf, probs);
			});

			std::string cat_src(dl.fetch(_curi + "?PageSize=100000"));

			std::cerr << _curi << std::endl;
			pp.parse(cat_src);

			if(product_i == 0) // If no products, add _curi to stack to expand tree deeper
				cp.parse(cat_src);
		}
	}

	raw scraper::download_image(const std::string& uri)
	{
		std::string buf(dl.fetch(uri));
		return raw(buf.data(), buf.length());
	}
}
