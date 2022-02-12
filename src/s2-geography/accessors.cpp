
#include "geography.hpp"
#include "accessors.hpp"

namespace s2geography {

bool s2_is_collection(const S2GeographyOwningPolygon& geog) {
    int num_outer_loops = 0;
    for (int i = 0; i < geog.Polygon()->num_loops(); i++) {
        S2Loop* loop = geog.Polygon()->loop(i);
        num_outer_loops += loop->depth() == 0;
        if (num_outer_loops > 1) {
            return true;
        }
    }

    return false;
}

bool s2_is_collection(const S2Geography& geog) {
    int dimension = s2_dimension(geog);
    if (dimension == 0) {
        return s2_num_points(geog) > 1;
    }

    if (dimension == 1) {
        int num_chains = 0;
        for (int i = 0; i < geog.num_shapes(); i++) {
            std::unique_ptr<S2Shape> shape = geog.Shape(i);
            num_chains += shape->num_chains();
            if (num_chains > 1) {
                return true;
            }
        }

        return false;
    }

    auto polygon_geog_ptr = dynamic_cast<const S2GeographyOwningPolygon*>(&geog);
    if (polygon_geog_ptr != nullptr) {
        return s2_is_collection(*polygon_geog_ptr);
    } else {
        // if custom subclasses are ever a thing, we can go through the builder
        // to build a polygon
        throw S2GeographyException("s2_area() not implemented for custom S2Geography()");
    }
}

int s2_dimension(const S2Geography& geog) {
    int dimension = -1;
    for (int i = 0; i < geog.num_shapes(); i++) {
        std::unique_ptr<S2Shape> shape = geog.Shape(i);
        if (shape->dimension() > dimension) {
            dimension = shape->dimension();
        }
    }

    return dimension;
}

int s2_num_points(const S2Geography& geog) {
    int num_points = 0;
    for (int i = 0; i < geog.num_shapes(); i++) {
        std::unique_ptr<S2Shape> shape = geog.Shape(i);
        switch (shape->dimension()) {
        case 0:
        case 2:
            num_points += shape->num_edges();
            break;
        case 1:
            num_points += shape->num_edges() + shape->num_chains();
            break;
        }
    }

    return num_points;
}

bool s2_is_empty(const S2Geography& geog) {
    for (int i = 0; i < geog.num_shapes(); i++) {
        std::unique_ptr<S2Shape> shape = geog.Shape(i);
        if (!shape->is_empty()) {
            return false;
        }
    }

    return true;
}

double s2_area(const S2GeographyOwningPolygon& geog) {
    return geog.Polygon()->GetArea();
}

double s2_area(const S2GeographyCollection& geog) {
    double area = 0;
    for (auto& feature: geog.Features()) {
        area += s2_area(*feature);
    }
    return area;
}

double s2_area(const S2Geography& geog) {
    if (s2_dimension(geog) != 2) {
        return 0;
    }

    auto polygon_geog_ptr = dynamic_cast<const S2GeographyOwningPolygon*>(&geog);
    if (polygon_geog_ptr != nullptr) {
        return s2_area(*polygon_geog_ptr);
    }

    auto collection_geog_ptr = dynamic_cast<const S2GeographyCollection*>(&geog);
    if (collection_geog_ptr != nullptr) {
       return s2_area(*collection_geog_ptr);
    }

    // if custom subclasses are ever a thing, we can go through the builder
    // to build a polygon
    throw S2GeographyException("s2_area() not implemented for custom S2Geography()");
}

double s2_length(const S2Geography& geog) {
    double length = 0;

    if (s2_dimension(geog) == 1) {
        for (int i = 0; i < geog.num_shapes(); i++) {
            std::unique_ptr<S2Shape> shape = geog.Shape(i);
            for (int j = 0; j < shape->num_edges(); j++) {
                S2Shape::Edge e = shape->edge(j);
                S1ChordAngle angle(e.v0, e.v1);
                length += angle.radians();
            }
        }
    }

    return length;
}

double s2_perimeter(const S2Geography& geog) {
    double length = 0;

    if (s2_dimension(geog) == 2) {
        for (int i = 0; i < geog.num_shapes(); i++) {
            std::unique_ptr<S2Shape> shape = geog.Shape(i);
            for (int j = 0; j < shape->num_edges(); j++) {
                S2Shape::Edge e = shape->edge(j);
                S1ChordAngle angle(e.v0, e.v1);
                length += angle.radians();
            }
        }
    }

    return length;
}

double s2_x(const S2Geography& geog) {
    double out = NAN;
    for (int i = 0; i < geog.num_shapes(); i++) {
        std::unique_ptr<S2Shape> shape = geog.Shape(i);
        if (shape->dimension() == 0 && shape->num_edges() == 1 && std::isnan(out)) {
            S2LatLng pt(shape->edge(0).v0);
            out = pt.lng().degrees();
        } else if (shape->dimension() == 0 && shape->num_edges() == 1) {
            return NAN;
        }
    }

    return out;
}

double s2_y(const S2Geography& geog) {
    double out = NAN;
    for (int i = 0; i < geog.num_shapes(); i++) {
        std::unique_ptr<S2Shape> shape = geog.Shape(i);
        if (shape->dimension() == 0 && shape->num_edges() == 1 && std::isnan(out)) {
            S2LatLng pt(shape->edge(0).v0);
            out = pt.lat().degrees();
        } else if (shape->dimension() == 0 && shape->num_edges() == 1) {
            return NAN;
        }
    }

    return out;
}

}
