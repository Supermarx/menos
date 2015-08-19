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
	scraper::scraper(product_callback_t _product_callback, tag_hierarchy_callback_t, size_t _ratelimit, bool _cache, bool)
	: product_callback(_product_callback)
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

		add_f("https://www.plus.nl/assortiment");

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
			product_parser pp([&](const message::product_base& p, boost::optional<std::string> const& _image_uri, datetime retrieved_on, confidence conf, problems_t probs)
			{
				++product_i;

				boost::optional<std::string> image_uri;
				if(_image_uri)
					image_uri = "https://www.plus.nl/" + *_image_uri;

				product_callback(_curi, image_uri, p, retrieved_on, conf, probs);
			});

			std::string cat_src(stubborn::attempt<std::string>([&](){
				return dl.fetch(_curi + "?PageSize=100000").body;
			}));

			pp.parse(cat_src);

			if(product_i == 0) // If no products, add _curi to stack to expand tree deeper
				cp.parse(cat_src);
		}
	}

	raw scraper::download_image(const std::string& uri)
	{
		std::string buf(dl.fetch(uri).body);
		return raw(buf.data(), buf.length());
	}
}
