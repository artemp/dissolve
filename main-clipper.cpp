#include <iostream>
#include <string>
#include <mapnik/config.hpp>

#include <mapnik/geometry.hpp>
#include <mapnik/geometry_envelope.hpp>
#include <mapnik/geometry_adapters.hpp>
#include <mapnik/geometry_is_simple.hpp>
#include <mapnik/geometry_is_valid.hpp>
#include <mapnik/geometry_transform.hpp>
#include <mapnik/geometry_correct.hpp>
#include <mapnik/wkt/wkt_factory.hpp>
#include <mapnik/json/geometry_parser.hpp> // from_geojson
#include <mapnik/util/geometry_to_wkt.hpp>
#include <fstream>
#include <streambuf>
#include <boost/geometry/policies/robustness/get_rescale_policy.hpp>
#include <boost/geometry/geometries/concepts/check.hpp>
#include "clipper.hpp"

// To generate sample input:
// psql osm -t -c "select ST_AsGeoJSON(ST_Simplify(way,1000)) from planet_osm_polygon where not ST_IsValid(ST_Simplify(way,1000));" -o polygons.geojson
// psql osm -tA -c "select ST_AsGeoJSON(ST_Simplify(way,100)) from planet_osm_polygon;" | grep -v "^$"  > polygons.geojson

//#define CLIPPER_IMPL_INCLUDE <mapnik/geometry.hpp>

namespace {

template <typename T>
struct transformer
{
    using rescale_policy = T;

    explicit transformer(T & policy)
        : policy_(policy) {}

    template <typename P1, typename P2>
    inline bool apply(P1 const& p1, P2& p2) const
    {
        using p2_type = typename boost::geometry::coordinate_type<P2>::type;
        double x = boost::geometry::get<0>(p1);
        double y = boost::geometry::get<1>(p1);
        p2_type x_scaled = policy_.template apply<1>(x);
        p2_type y_scaled = policy_.template apply<1>(y);
        boost::geometry::set<0>(p2, x_scaled);
        boost::geometry::set<1>(p2, y_scaled);
        return true;
    }
    rescale_policy & policy_;
};

}

namespace detail
{

inline void process_polynode_branch(ClipperLib::PolyNode* polynode,
                                    mapnik::geometry::multi_polygon<std::int64_t> & mp,
                                    double area_threshold)
{
    mapnik::geometry::polygon<std::int64_t> polygon;
    polygon.set_exterior_ring(std::move(polynode->Contour));
    if (polygon.exterior_ring.size() > 2) // Throw out invalid polygons
    {
        double outer_area = ClipperLib::Area(polygon.exterior_ring);
        if (std::abs(outer_area) >= area_threshold)
        {
            // The view transform inverts the y axis so this should be positive still despite now
            // being clockwise for the exterior ring. If it is not lets invert it.
            if (outer_area < 0)
            {
                std::reverse(polygon.exterior_ring.begin(), polygon.exterior_ring.end());
            }

            // children of exterior ring are always interior rings
            for (auto * ring : polynode->Childs)
            {
                if (ring->Contour.size() < 3)
                {
                    continue; // Throw out invalid holes
                }
                double inner_area = ClipperLib::Area(ring->Contour);
                if (std::abs(inner_area) < area_threshold)
                {
                    continue;
                }

                if (inner_area > 0)
                {
                    std::reverse(ring->Contour.begin(), ring->Contour.end());
                }
                polygon.add_hole(std::move(ring->Contour));
            }
            mp.push_back(std::move(polygon));
        }
    }
    for (auto * ring : polynode->Childs)
    {
        for (auto * sub_ring : ring->Childs)
        {
            process_polynode_branch(sub_ring, mp, area_threshold);
        }
    }
}

} // end ns detail

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
        std::cerr << ".";
        mapnik::geometry::geometry<double> geom;
        if (!mapnik::json::from_geojson(json, geom))
        {
            std::cerr << "Failed to parse GeoJSON geometry" << std::endl;
            return EXIT_FAILURE;
        }

        if (geom.is<mapnik::geometry::polygon<double>>())
        {
            using point_type = mapnik::geometry::point<double>;
            using rescale_policy_type = boost::geometry::rescale_policy_type<point_type>::type;
            using rescale_policy = transformer<rescale_policy_type>;
            auto const& poly_in = geom.get<mapnik::geometry::polygon<double>>();
            auto b = mapnik::geometry::envelope(poly_in);

            // re-scale
            auto robust_policy = boost::geometry::get_rescale_policy<rescale_policy_type>(poly_in);
            rescale_policy scaler(robust_policy);
            mapnik::geometry::geometry<std::int64_t> new_geom = mapnik::geometry::transform<std::int64_t>(geom, scaler);
            mapnik::geometry::polygon<std::int64_t> & poly = new_geom.get<mapnik::geometry::polygon<std::int64_t>>();

            // Prepare the clipper object
            ClipperLib::Clipper clipper;
            clipper.StrictlySimple(true);
            double clean_distance = 1.415;// * robust_policy.m_multiplier;
            ClipperLib::CleanPolygon(poly.exterior_ring, clean_distance);
            if (!clipper.AddPath(poly.exterior_ring, ClipperLib::ptSubject, true))
            {
                return EXIT_FAILURE;
            }
            for (auto & hole : poly.interior_rings)
            {
                ClipperLib::CleanPolygon(hole, clean_distance);
                if (!clipper.AddPath(hole, ClipperLib::ptSubject, true)) return EXIT_FAILURE;
            }

            mapnik::geometry::correct(poly);

            // Setup the box for clipping
            mapnik::geometry::linear_ring<double> clip_box_in;
            double pad = 0.1 * b.width();

            clip_box_in.reserve(5);
            clip_box_in.emplace_back(b.minx() + pad , b.miny() + pad);
            clip_box_in.emplace_back(b.maxx() - pad , b.miny() + pad);
            clip_box_in.emplace_back(b.maxx() - pad , b.maxy() - pad);
            clip_box_in.emplace_back(b.minx() + pad,  b.maxy() - pad);
            clip_box_in.emplace_back(b.minx() + pad , b.miny() + pad);

            mapnik::geometry::linear_ring<std::int64_t> clip_box = mapnik::geometry::transform<std::int64_t>(clip_box_in, scaler);
            mapnik::geometry::correct(clip_box);
            // add clip polygon
            if (!clipper.AddPath( clip_box, ClipperLib::ptClip, true))
            {
                return EXIT_FAILURE;
            }
            ClipperLib::PolyTree polygons;
            clipper.Execute(ClipperLib::ctIntersection, polygons, ClipperLib::pftNonZero, ClipperLib::pftEvenOdd);
            clipper.Clear();

            mapnik::geometry::multi_polygon<std::int64_t> mp;

            for (auto * polynode : polygons.Childs)
            {
                detail::process_polynode_branch(polynode, mp, 1.0);
            }

            if (!mp.empty())
            {
                mapnik::geometry::correct(mp);
                bool valid = mapnik::geometry::is_valid(mp);
                bool simple = mapnik::geometry::is_simple(mp);

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
                    mapnik::geometry::geometry<std::int64_t> geom_out(mp);
                    //mapnik::geometry::geometry<double> geom_out = mapnik::geometry::transform<double>(poly, rescale(1.0/scale_factor));
                    std::string wkt_out;
                    if (!mapnik::util::to_wkt(wkt_out,geom_out))
                    {
                        throw std::runtime_error("Generate WKT failed");
                    }
                    std::cout << wkt_out << std::endl;
                    ++failed_count;
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
