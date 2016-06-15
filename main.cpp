#include <iostream>
#include <string>
#include <mapnik/config.hpp>
#include <boost/geometry/extensions/algorithms/dissolve.hpp>
#include <boost/geometry/extensions/multi/algorithms/dissolve.hpp>
#include <boost/geometry/algorithms/remove_spikes.hpp>
#include <mapnik/geometry.hpp>
#include <mapnik/geometry_adapters.hpp>
#include <mapnik/geometry_is_simple.hpp>
#include <mapnik/geometry_is_valid.hpp>
#include <mapnik/wkt/wkt_factory.hpp>
#include <mapnik/json/geometry_parser.hpp> // from_geojson
#include <mapnik/util/geometry_to_wkt.hpp>
#include <fstream>
#include <streambuf>

// To generate sample input:
// psql osm -t -c "select ST_AsGeoJSON(ST_Simplify(way,1000)) from planet_osm_polygon where not ST_IsValid(ST_Simplify(way,1000));" -o polygons.geojson
// psql osm -tA -c "select ST_AsGeoJSON(ST_Simplify(way,100)) from planet_osm_polygon;" | grep -v "^$"  > polygons.geojson

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <GeoJSON file>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string filename(argv[1]);

    std::ifstream in(filename.c_str());
    if (!in)
    {
        std::cerr << "Can't open " << filename << std::endl;
        return EXIT_FAILURE;
    }

    std::size_t failed_count = 0;
    std::cerr << "Processing ..." << std::endl;
    for (std::string json; std::getline(in, json); )
    {
        if (json.empty()) continue;
        //std::string json((std::istreambuf_iterator<char>(in)),
        //            std::istreambuf_iterator<char>());

        //std::cerr << json << std::endl;
        std::cerr << ".";
        mapnik::geometry::geometry<double> geom;
        if (!mapnik::json::from_geojson(json, geom))
        {
            std::cerr << "Failed to parse GeoJSON geometry" << std::endl;
            return EXIT_FAILURE;
        }

        if (geom.is<mapnik::geometry::polygon<double>>())
        {

            auto const& poly = geom.get<mapnik::geometry::polygon<double>>();

            mapnik::geometry::multi_polygon<double> dissolved;
            boost::geometry::dissolve(poly, dissolved);
            if (dissolved.size() > 0)
            {
                for (auto & p : dissolved)
                {
                    //boost::geometry::remove_spikes(p);
#if 1
                    bool valid = mapnik::geometry::is_valid(p);
                    bool simple = mapnik::geometry::is_simple(p);
                    if (!simple || !valid)
                    {
                        std::cerr << "x" << std::endl;

                        std::cerr << "<<<<<<<<<<<<<<<<<<<< INPUT " << std::endl;
                        std::string wkt;
                        if (!mapnik::util::to_wkt(wkt,geom))
                        {
                            throw std::runtime_error("Generate WKT failed");
                        }
                        std::cerr << wkt << std::endl;
                        std::cerr << "Is simple? " << mapnik::geometry::is_simple(geom) << std::endl;
                        std::cerr << "Is valid? " << mapnik::geometry::is_valid(geom) << std::endl;
                        std::cerr << ">>>>>>>>>>>>>>>>>>>> OUTPUT" << std::endl;

                        std::cerr << "Is simple? " << simple << std::endl;
                        std::cerr << "Is valid? " << valid << std::endl;
                        //return EXIT_FAILURE;
                        mapnik::geometry::geometry<double> geom_out(p);
                        std::string wkt_out;
                        if (!mapnik::util::to_wkt(wkt_out,geom_out))
                        {
                            throw std::runtime_error("Generate WKT failed");
                        }
                        std::cout << wkt_out << std::endl;
                        ++failed_count;
                    }
#endif
                }
            }
        }
        else
        {
            std::cerr << "Ensure input is a valid Polygon" << std::endl;
        }
    }
    std::cerr << "FAILED count=" << failed_count << std::endl;
    return EXIT_SUCCESS;
}
