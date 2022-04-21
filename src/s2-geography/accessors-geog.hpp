
#pragma once

#include "s2/s2convex_hull_query.h"

#include "geography.hpp"
#include "aggregator.hpp"

namespace s2geography {

S2Point s2_centroid(const S2Geography& geog);
std::unique_ptr<S2Geography> s2_boundary(const S2Geography& geog);
std::unique_ptr<S2Geography> s2_convex_hull(const S2Geography& geog);

class S2CentroidAggregator: public S2Aggregator<S2Point> {
public:
    void Add(const S2Geography& geog);
    void Merge(const S2CentroidAggregator& other);
    S2Point Finalize();

private:
    S2Point centroid_;
};

class S2ConvexHullAggregator: public S2Aggregator<std::unique_ptr<S2GeographyOwningPolygon>> {
public:
    void Add(const S2Geography& geog);
    std::unique_ptr<S2GeographyOwningPolygon> Finalize();

private:
    S2ConvexHullQuery query_;
    std::vector<std::unique_ptr<S2Geography>> keep_alive_;
};

}