#include <iostream>
#include <string>
#include <mapnik/config.hpp>
#include <boost/geometry/extensions/algorithms/dissolve.hpp>
#include <boost/geometry/extensions/multi/algorithms/dissolve.hpp>

#include <mapnik/geometry.hpp>
#include <mapnik/geometry_adapters.hpp>
#include <mapnik/geometry_is_simple.hpp>
#include <mapnik/geometry_is_valid.hpp>
#include <mapnik/wkt/wkt_factory.hpp>
#include <mapnik/util/geometry_to_wkt.hpp>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <WKT>" << std::endl;
        return EXIT_FAILURE;
    }
    std::string wkt(argv[1]);


    mapnik::geometry::geometry<double> geom;
    if (!mapnik::from_wkt(wkt, geom))
    {
        std::cerr << "Failed to parse WKT geometry" << std::endl;
        return EXIT_FAILURE;
    }
    std::cerr << "<<<<<<<<<<<<<<<<<<<< INPUT " << std::endl;
    std::cerr << wkt << std::endl;
    std::cerr << "Is simple? " << mapnik::geometry::is_simple(geom) << std::endl;
    std::cerr << "Is valid? " << mapnik::geometry::is_valid(geom) << std::endl;

    if (geom.is<mapnik::geometry::polygon<double>>())
    {
        std::cerr << ">>>>>>>>>>>>>>>>>>>> OUTPUT" << std::endl;
        auto const& poly = geom.get<mapnik::geometry::polygon<double>>();

        std::vector<mapnik::geometry::polygon<double>> dissolved;
        boost::geometry::dissolve(poly, dissolved);

        for (auto const& p : dissolved)
        {
            mapnik::geometry::geometry<double> geom_out(p);
            std::string wkt_out;
            if (!mapnik::util::to_wkt(wkt_out,geom_out))
            {
                throw std::runtime_error("Generate WKT failed");
            }
            std::cerr << wkt_out << std::endl;
            std::cerr << "Is simple? " << mapnik::geometry::is_simple(geom_out) << std::endl;
            std::cerr << "Is valid? " << mapnik::geometry::is_valid(geom_out) << std::endl;
        }
    }
    else
    {
        std::cerr << "Ensure must is a valid Polygon" << std::endl;
    }
    return EXIT_SUCCESS;
}
