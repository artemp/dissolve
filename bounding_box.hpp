#ifndef BOUNDING_BOX_HPP
#define BOUNDING_BOX_HPP

#include <boost/geometry.hpp>
#include <mapnik/geometry.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>

namespace mapnik { namespace geometry {

struct bounding_box
{
    bounding_box() {} // no-init
    bounding_box(double lox, double loy, double hix, double hiy)
        : p0(lox,loy),
          p1(hix,hiy) {}
    point<double> p0;
    point<double> p1;
};

}}


namespace boost { namespace geometry { namespace traits {

template<> struct tag<mapnik::geometry::bounding_box> { using type = box_tag; };

template<> struct point_type<mapnik::geometry::bounding_box> { using type = mapnik::geometry::point<double>; };


template <>
struct indexed_access<mapnik::geometry::bounding_box, min_corner, 0>
{
    static inline double get(mapnik::geometry::bounding_box const& b) { return b.p0.x;}
    static inline void set(mapnik::geometry::bounding_box& b, double value) { b.p0.x = value; }
};

template <>
struct indexed_access<mapnik::geometry::bounding_box, min_corner, 1>
{
    static inline double get(mapnik::geometry::bounding_box const& b) { return b.p0.y;}
    static inline void set(mapnik::geometry::bounding_box& b, double value) { b.p0.y = value; }
};

template <>
struct indexed_access<mapnik::geometry::bounding_box, max_corner, 0>
{
    static inline double get(mapnik::geometry::bounding_box const& b) { return b.p1.x;}
    static inline void set(mapnik::geometry::bounding_box& b, double value) { b.p1.x = value; }
};

template <>
struct indexed_access<mapnik::geometry::bounding_box, max_corner, 1>
{
    static inline double get(mapnik::geometry::bounding_box const& b) { return b.p1.y;}
    static inline void set(mapnik::geometry::bounding_box& b, double value) { b.p1.y = value; }
};

}}}

#endif //BOUNDING_BOX_HPP
