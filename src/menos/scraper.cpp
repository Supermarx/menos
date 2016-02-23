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
	, m(dl, [&]() { error_count++; })
	, product_count(0)
	, page_count(0)
	, error_count(0)
	{}

	void scraper::scrape()
	{
		product_count = 0;
		page_count = 0;
		error_count = 0;

		std::set<category_parser::category_uri_t> cats;
		std::function<void(std::string)> enqueue_f([&](std::string const& curi) {
			if(!cats.emplace(curi).second) // If already added curi
				return;

			++page_count;

			m.schedule(curi + "?PageSize=100000", [&, curi](downloader::response const& response) {
				size_t product_i = 0;
				product_parser pp([&](const message::product_base& p, boost::optional<std::string> const& _image_uri, datetime retrieved_on, confidence conf, problems_t probs)
				{
					++product_i;
					++product_count;

					boost::optional<std::string> image_uri;
					if(_image_uri)
						image_uri = "https://www.plus.nl/" + *_image_uri;

					product_callback(curi, image_uri, p, retrieved_on, conf, probs);
				});
				pp.parse(response.body);

				if(product_i == 0) // If no products, add _curi to stack to expand tree deeper
				{
					category_parser cp([&](category_parser::category_uri_t const& curi_new)
					{
						if(boost::algorithm::contains(curi, "?SearchParameter=")) // Skip if expanded search query
							return;

						enqueue_f(curi_new);
					});
					cp.parse(response.body);
				}
			});
		});

		enqueue_f("https://www.plus.nl/assortiment");

		m.process_all();
		std::cerr << "Pages: " << page_count << ", products: " << product_count << ", errors: " << error_count << std::endl;
	}

	raw scraper::download_image(const std::string& uri)
	{
		std::string buf(dl.fetch(uri).body);
		return raw(buf.data(), buf.length());
	}
}
