# dissolve

### Requirements
* Boost 1.61
* Latest boost-geometry with boost-geometry-dissolve.patch applied
* Edit `Jamroot` 
* `b2 release`

### Running 

* Generate input data e.g `psql osm -tA -c "select ST_AsGeoJSON(ST_Simplify(way,100)) from planet_osm_polygon;" | grep -v "^$"  > polygons.geojson` 
* Run `./bin/clang-darwin-4.2.1/release/dissolve-test polygons.geojson`

