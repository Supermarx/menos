#pragma once

#include <functional>
#include <stdexcept>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/optional.hpp>

#include <supermarx/scraper/html_parser.hpp>
#include <supermarx/scraper/html_watcher.hpp>
#include <supermarx/scraper/html_recorder.hpp>
#include <supermarx/scraper/util.hpp>

namespace supermarx
{
	class product_parser : public html_parser::default_handler
	{
	public:
		typedef std::function<void(const message::product_base&, boost::optional<std::string>, datetime, confidence, std::vector<std::string>)> product_callback_t;

	private:
		enum state_e {
			S_INIT,
			S_ENABLED,
			S_PRODUCT_LIST,
			S_PRODUCT,
			S_PRODUCT_IMAGE
		};

		product_callback_t product_callback;

		boost::optional<html_recorder> rec;
		html_watcher_collection wc;

		state_e state;

		struct product_proto
		{
			std::string identifier, name;
			boost::optional<std::string> image_uri;
			boost::optional<size_t> price;
			boost::optional<std::string> caliber;
			boost::optional<std::string> discount;

			confidence conf = confidence::NEUTRAL;
			std::vector<std::string> problems;
		};

		product_proto current_p;

		static date first_monday(date d)
		{
			while(d.day_of_week() != weekday::Monday)
				d += boost::gregorian::date_duration(1);

			return d;
		}

		void report_problem_understanding(std::string const& field, std::string const& value)
		{
			std::stringstream sstr;
			sstr << "Unclear '" << field << "' with value '" << value << "'";

			std::cerr << "problem: " << sstr.str() << std::endl;
			current_p.problems.emplace_back(sstr.str());
		}

		void interpret_caliber(std::string unit, uint64_t& volume, measure& volume_measure)
		{
			boost::smatch what;
			static const boost::regex match_prefix("Per (?:[a-z]+ )?(.+)");
			static const boost::regex match_measure("([0-9]+(?:\\.[0-9]+)?)[\\s]+(.+)");

			if(boost::regex_match(unit, what, match_prefix))
				unit = what[1];

			boost::erase_all(unit, ".");

			if(boost::regex_match(unit, what, match_measure))
			{
				std::string measure_type = what[2];

				const static std::set<std::string> generic_types({{
					"stuk", "stuks",
					"pak", "bus", "rol", "vel", "doos", "blik", "paar", "fles",
					"zak", "bord"
				}});

				if(generic_types.find(measure_type) != generic_types.end())
				{
					volume = boost::lexical_cast<float>(what[1]);
					volume_measure = measure::UNITS;
				}
				else if(measure_type == "mg")
				{
					volume = boost::lexical_cast<float>(what[1]);
					volume_measure = measure::MILLIGRAMS;
				}
				else if(measure_type == "g" || measure_type == "gr" || measure_type == "gram" || measure_type == "milligram")
				{
					volume = boost::lexical_cast<float>(what[1])*1000.0;
					volume_measure = measure::MILLIGRAMS;
				}
				else if(measure_type == "kg" || measure_type == "kilo" || measure_type == "kilogram")
				{
					volume = boost::lexical_cast<float>(what[1])*1000000.0;
					volume_measure = measure::MILLIGRAMS;
				}
				else if(measure_type == "ml" || measure_type == "milliliter")
				{
					volume = boost::lexical_cast<float>(what[1]);
					volume_measure = measure::MILLILITRES;
				}
				else if(measure_type == "cl" || measure_type == "centilliter")
				{
					volume = boost::lexical_cast<float>(what[1])*10.0;
					volume_measure = measure::MILLILITRES;
				}
				else if(measure_type == "l" || measure_type == "lt" || measure_type == "liter" || measure_type == "litre")
				{
					volume = boost::lexical_cast<float>(what[1])*1000.0;
					volume_measure = measure::MILLILITRES;
				}
				else if(measure_type == "meter")
				{
					volume = boost::lexical_cast<float>(what[1])*1000.0;
					volume_measure = measure::MILLIMETRES;
				}
				else
				{
					report_problem_understanding("measure_type", measure_type);
					current_p.conf = confidence::LOW;
					return;
				}
			}
			else
			{
				report_problem_understanding("unit", unit);
				current_p.conf = confidence::LOW;
			}
		}

		void interpret_discount(std::string discount_str, uint64_t& price, uint64_t& discount_amount)
		{
			static const boost::regex match_percent_discount("([0-9]+)%KORTING");
			static const boost::regex match_new_price("([0-9]+)");
			boost::smatch what;

			if(boost::regex_match(discount_str, what, match_percent_discount))
			{
				price *= 1.0 - boost::lexical_cast<float>(what[1])/100.0;
			}
			else if(boost::regex_match(discount_str, what, match_new_price))
			{
				price = boost::lexical_cast<uint64_t>(what[1]);
			}
			else if(discount_str == "2eHALVEPRIJS")
			{
				discount_amount = 2;
				price = price * 0.75;
			}
			else if(discount_str == "1+1GRATIS" || discount_str == "1+1")
			{
				discount_amount = 2;
				price = price * 0.5;
			}
			else if(discount_str == "1+2GRATIS" || discount_str == "1+2")
			{
				discount_amount = 3;
				price = price / 3;
			}
			else if(discount_str == "2+1GRATIS" || discount_str == "2+1")
			{
				discount_amount = 3;
				price = (price * 2) / 3;
			}
			else if(discount_str == "3+1GRATIS" || discount_str == "3+1")
			{
				discount_amount = 4;
				price = price * 0.75;
			}
			else
			{
				report_problem_understanding("discount_str", discount_str);
				current_p.conf = confidence::LOW;
			}
		}

		void deliver_product()
		{
			uint64_t volume = 1;
			measure volume_measure = measure::UNITS;

			if(current_p.caliber)
				interpret_caliber(*current_p.caliber, volume, volume_measure);

			if(!current_p.price)
			{
				std::cerr << "Price not set for " << current_p.name << std::endl;
				return;
			}

			uint64_t orig_price = *current_p.price;
			uint64_t price = orig_price;

			uint64_t discount_amount = 1;

			if(current_p.discount)
				interpret_discount(*current_p.discount, price, discount_amount);

			product_callback(
				message::product_base{
					current_p.identifier,
					current_p.name,
					volume,
					volume_measure,
					orig_price,
					price,
					discount_amount,
					datetime_now(),
					{} // TODO (tags)
				},
				current_p.image_uri,
				datetime_now(),
				current_p.conf,
				current_p.problems
			);
		}

	public:
		product_parser(product_callback_t product_callback_)
		: product_callback(product_callback_)
		, rec()
		, wc()
		, state(S_INIT)
		, current_p()
		{}

		template<typename T>
		void parse(T source)
		{
			html_parser::parse(source, *this);
		}

		virtual void startElement(const std::string& /* namespaceURI */, const std::string& /* localName */, const std::string& qName, const AttributesT& atts)
		{
			if(rec)
				rec.get().startElement();

			wc.startElement();

			const std::string att_class = atts.getValue("class");

			switch(state)
			{
			case S_INIT:
				if(util::contains_attr("list-filters-and-pagination", att_class)) // Core product listing with pagination
					state = S_ENABLED;
			break;
			case S_ENABLED:
				if(qName == "ul" && util::contains_attr("ish-productList", att_class))
				{
					state = S_PRODUCT_LIST;
					wc.add([&]() {
						state = S_ENABLED;
					});
				}
			break;
			case S_PRODUCT_LIST:
				if(qName == "div" && util::contains_attr("ish-productTileKlik", att_class))
				{
					//Reset product
					current_p = product_proto();

					state = S_PRODUCT;
					wc.add([&]() {
						state = S_PRODUCT_LIST;

						deliver_product();
					});
				}
			break;
			case S_PRODUCT:
				if(atts.getValue("data-attr-sku") != "")
					current_p.identifier = atts.getValue("data-attr-sku");

				if(qName == "div" && atts.getValue("data-dynamic-block-id") == "PRODUCTTILE-PRODUCTTITLE")
				{
					rec = html_recorder(
						[&](std::string ch) {
							current_p.name = util::sanitize(ch);
						});
				}
				else if(util::contains_attr("ish-productTile-withCartButton-photo", att_class))
				{
					state = S_PRODUCT_IMAGE;
					wc.add([&]() {
						state = S_PRODUCT;
					});
				}
				if(util::contains_attr("shape-tekst", att_class))
				{
					rec = html_recorder(
						[&](std::string ch) {
							current_p.discount = util::sanitize(ch);
						});
				}
				else if(util::contains_attr("product-tile-priceContainer", att_class))
				{
					rec = html_recorder(
						[&](std::string ch) {
							static const boost::regex match_price("([0-9]+) ([0-9]+)");
							boost::smatch what;

							std::string ch_san(util::sanitize(ch));
							if(!boost::regex_match(ch_san, what, match_price))
								throw std::runtime_error("Malformed price '" + ch_san + "'");

							current_p.price = boost::lexical_cast<size_t>(what[1]) * 100 + boost::lexical_cast<size_t>(what[2]);
						});
				}
				else if(util::contains_attr("content-weight-or-per-piece", att_class))
				{
					rec = html_recorder(
						[&](std::string ch) {
							current_p.caliber = util::sanitize(ch);
						});
				}
			break;
			case S_PRODUCT_IMAGE:
				if(qName == "img")
				{
					static const boost::regex match_img("(.*)/M/(.*)");
					boost::smatch what;

					std::string src(atts.getValue("data-src"));
					if(src == "") // No image available
						break;

					if(!boost::regex_match(src, what, match_img))
						throw std::runtime_error("Malformed image uri '" + src + "'");

					current_p.image_uri = what[1] + "/L/" + what[2];
				}
			break;
			}
		}

		virtual void characters(const std::string& ch)
		{
			if(rec)
				rec.get().characters(ch);
		}

		virtual void endElement(const std::string& /* namespaceURI */, const std::string& /* localName */, const std::string& /* qName */)
		{
			if(rec && rec.get().endElement())
				rec = boost::none;

			wc.endElement();
		}
	};
}
