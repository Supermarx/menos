#include <supermarx/scraper/scraper_cli.hpp>
#include <menos/scraper.hpp>

int main(int argc, char** argv)
{
	return supermarx::scraper_cli<supermarx::scraper>::exec(5, "menos", "Plus", argc, argv);
}
