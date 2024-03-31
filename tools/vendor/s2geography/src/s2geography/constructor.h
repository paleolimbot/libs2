
#pragma once

#include <sstream>

#include <s2/s2edge_tessellator.h>

#include "geoarrow-imports.h"
#include "geography.h"

namespace s2geography {

namespace util {

class Constructor : public Handler {
 public:
  class Options {
   public:
    Options() : oriented_(false), check_(true), tessellate_tolerance_(S1Angle::Infinity()) {}
    bool oriented() const { return oriented_; }
    void set_oriented(bool oriented) { oriented_ = oriented; }
    bool check() const { return check_; }
    void set_check(bool check) { check_ = check; }
    S2::Projection* projection() const { return projection_; }
    void set_projection(S2::Projection* projection) { projection_ = projection; }
    S1Angle tessellate_tolerance() const { return tessellate_tolerance_; }
    void set_tessellate_tolerance(S1Angle tessellate_tolerance) {
      tessellate_tolerance_ = tessellate_tolerance;
    }

   private:
    bool oriented_;
    bool check_;
    S2::Projection* projection_;
    S1Angle tessellate_tolerance_;
  };

  Constructor(const Options& options) : options_(options) {
    if (options.projection() != nullptr) {
      this->tessellator_ = absl::make_unique<S2EdgeTessellator>(options.projection(), options.tessellate_tolerance());
    }
  }

  virtual ~Constructor() {}

  virtual Result coords(const double* coord, int64_t n, int32_t coord_size) {
    if (coord_size == 3) {
      for (int64_t i = 0; i < n; i++) {
        input_points_.push_back(
          S2Point(
            coord[i * coord_size],
            coord[i * coord_size + 1],
            coord[i * coord_size + 2]
          )
        );
      }
    } else {
      for (int64_t i = 0; i < n; i++) {
        input_points_.push_back(S2Point(coord[i * coord_size], coord[i * coord_size + 1], 0));
      }
    }

    return Result::CONTINUE;
  }

  virtual std::unique_ptr<Geography> finish() = 0;

 protected:
  std::vector<S2Point> input_points_;
  std::vector<S2Point> points_;
  Options options_;
  std::unique_ptr<S2EdgeTessellator> tessellator_;

  void finish_points() {
    points_.clear();
    points_.reserve(input_points_.size());

    if (options_.projection() == nullptr) {
      for (const auto& pt: input_points_) {
        points_.push_back(pt);
      }
    } else if (options_.tessellate_tolerance() != S1Angle::Infinity()) {
      for (size_t i = 1; i < input_points_.size(); i++) {
        const S2Point& pt0(input_points_[i - 1]);
        const S2Point& pt1(input_points_[i]);
        tessellator_->AppendUnprojected(R2Point(pt0.x(), pt0.y()), R2Point(pt1.x(), pt1.y()), &points_);
      }
    } else {
      for (const auto& pt: input_points_) {
        points_.push_back(options_.projection()->Unproject(R2Point(pt.x(), pt.y())));
      }
    }

    input_points_.clear();
  }
};

class PointConstructor : public Constructor {
 public:
  PointConstructor(const Options& options) : Constructor(options) {}

  Result geom_start(util::GeometryType geometry_type, int64_t size) {
    if (size != 0 && geometry_type != util::GeometryType::POINT &&
        geometry_type != util::GeometryType::MULTIPOINT &&
        geometry_type != util::GeometryType::GEOMETRYCOLLECTION) {
      throw Exception(
          "PointConstructor input must be empty, point, multipoint, or "
          "collection");
    }

    if (size > 0) {
      points_.reserve(points_.size() + size);
    }

    return Result::CONTINUE;
  }

  Result coords(const double* coord, int64_t n, int32_t coord_size) {
    for (int64_t i = 0; i < n; i++) {
      if (coord_empty(coord + (i * coord_size), coord_size)) {
        continue;
      }

      if (options_.projection() == nullptr) {
        S2Point pt(coord[i * coord_size], coord[i * coord_size + 1], coord[i * coord_size + 2]);
        points_.push_back(pt);
      } else {
        R2Point pt(coord[i * coord_size], coord[i * coord_size + 1]);
        points_.push_back(options_.projection()->Unproject(pt));
      }
    }

    return Result::CONTINUE;
  }

  std::unique_ptr<Geography> finish() {
    auto result = absl::make_unique<PointGeography>(std::move(points_));
    points_.clear();
    return std::unique_ptr<Geography>(result.release());
  }

 private:
  bool coord_empty(const double* coord, int32_t coord_size) {
    for (int32_t i = 0; i < coord_size; i++) {
      if (!std::isnan(coord[i])) {
        return false;
      }
    }

    return true;
  }
};

class PolylineConstructor : public Constructor {
 public:
  PolylineConstructor(const Options& options) : Constructor(options) {}

  Result geom_start(util::GeometryType geometry_type, int64_t size) {
    if (size != 0 && geometry_type != util::GeometryType::LINESTRING &&
        geometry_type != util::GeometryType::MULTILINESTRING &&
        geometry_type != util::GeometryType::GEOMETRYCOLLECTION) {
      throw Exception(
          "PolylineConstructor input must be empty, linestring, "
          "multilinestring, or collection");
    }

    if (size > 0 && geometry_type == util::GeometryType::LINESTRING) {
      input_points_.reserve(size);
    }

    return Result::CONTINUE;
  }

  Result geom_end() {
    finish_points();

    if (!points_.empty()) {
      auto polyline = absl::make_unique<S2Polyline>();
      polyline->Init(std::move(points_));

      // Previous version of s2 didn't check for this, so in
      // this check is temporarily disabled to avoid mayhem in
      // reverse dependency checks.
      // if (options_.check() && !polyline->IsValid()) {
      //   polyline->FindValidationError(&error_);
      //   throw Exception(error_.text());
      // }

      polylines_.push_back(std::move(polyline));
    }

    return Result::CONTINUE;
  }

  std::unique_ptr<Geography> finish() {
    std::unique_ptr<PolylineGeography> result;

    if (polylines_.empty()) {
      result = absl::make_unique<PolylineGeography>();
    } else {
      result = absl::make_unique<PolylineGeography>(std::move(polylines_));
      polylines_.clear();
    }

    return std::unique_ptr<Geography>(result.release());
  }

 private:
  std::vector<std::unique_ptr<S2Polyline>> polylines_;
  S2Error error_;
};

class PolygonConstructor : public Constructor {
 public:
  PolygonConstructor(const Options& options) : Constructor(options) {}

  Result ring_start(int64_t size) {
    input_points_.clear();
    if (size > 0) {
      input_points_.reserve(size);
    }

    return Result::CONTINUE;
  }

  Result ring_end() {
    finish_points();

    if (points_.empty()) {
      return Result::CONTINUE;
    }

    points_.pop_back();
    auto loop = absl::make_unique<S2Loop>();
    loop->set_s2debug_override(S2Debug::DISABLE);
    loop->Init(std::move(points_));

    if (!options_.oriented()) {
      loop->Normalize();
    }

    if (options_.check() && !loop->IsValid()) {
      std::stringstream err;
      err << "Loop " << (loops_.size()) << " is not valid: ";
      loop->FindValidationError(&error_);
      err << error_.text();
      throw Exception(err.str());
    }

    loops_.push_back(std::move(loop));
    points_.clear();
    return Result::CONTINUE;
  }

  std::unique_ptr<Geography> finish() {
    auto polygon = absl::make_unique<S2Polygon>();
    polygon->set_s2debug_override(S2Debug::DISABLE);
    if (options_.oriented()) {
      polygon->InitOriented(std::move(loops_));
    } else {
      polygon->InitNested(std::move(loops_));
    }

    loops_.clear();

    if (options_.check() && !polygon->IsValid()) {
      polygon->FindValidationError(&error_);
      throw Exception(error_.text());
    }

    auto result = absl::make_unique<PolygonGeography>(std::move(polygon));
    return std::unique_ptr<Geography>(result.release());
  }

 private:
  std::vector<std::unique_ptr<S2Loop>> loops_;
  S2Error error_;
};

class CollectionConstructor : public Constructor {
 public:
  CollectionConstructor(const Options& options)
      : Constructor(options),
        point_constructor_(options),
        polyline_constructor_(options),
        polygon_constructor_(options),
        collection_constructor_(nullptr),
        level_(0) {}

  Result geom_start(util::GeometryType geometry_type, int64_t size) {
    level_++;
    if (level_ == 1 &&
        geometry_type == util::GeometryType::GEOMETRYCOLLECTION) {
      active_constructor_ = nullptr;
      return Result::CONTINUE;
    }

    if (active_constructor_ != nullptr) {
      active_constructor_->geom_start(geometry_type, size);
      return Result::CONTINUE;
    }

    switch (geometry_type) {
      case util::GeometryType::POINT:
      case util::GeometryType::MULTIPOINT:
        active_constructor_ = &point_constructor_;
        break;
      case util::GeometryType::LINESTRING:
      case util::GeometryType::MULTILINESTRING:
        active_constructor_ = &polyline_constructor_;
        break;
      case util::GeometryType::POLYGON:
      case util::GeometryType::MULTIPOLYGON:
        active_constructor_ = &polygon_constructor_;
        break;
      case util::GeometryType::GEOMETRYCOLLECTION:
        this->collection_constructor_ =
            absl::make_unique<CollectionConstructor>(options_);
        this->active_constructor_ = this->collection_constructor_.get();
        break;
      default:
        throw Exception("CollectionConstructor: unsupported geometry type");
    }

    active_constructor_->geom_start(geometry_type, size);
    return Result::CONTINUE;
  }

  Result ring_start(int64_t size) {
    active_constructor_->ring_start(size);
    return Result::CONTINUE;
  }

  Result coords(const double* coord, int64_t n, int32_t coord_size) {
    active_constructor_->coords(coord, n, coord_size);
    return Result::CONTINUE;
  }

  Result ring_end() {
    active_constructor_->ring_end();
    return Result::CONTINUE;
  }

  Result geom_end() {
    level_--;

    if (level_ >= 1) {
      active_constructor_->geom_end();
    }

    if (level_ == 1) {
      auto feature = active_constructor_->finish();
      features_.push_back(std::move(feature));
      active_constructor_ = nullptr;
    }

    return Result::CONTINUE;
  }

  std::unique_ptr<Geography> finish() {
    auto result =
        absl::make_unique<GeographyCollection>(std::move(features_));
    features_.clear();
    return std::unique_ptr<Geography>(result.release());
  }

 private:
  PointConstructor point_constructor_;
  PolylineConstructor polyline_constructor_;
  PolygonConstructor polygon_constructor_;
  std::unique_ptr<CollectionConstructor> collection_constructor_;

 protected:
  Constructor* active_constructor_;
  int level_;
  std::vector<std::unique_ptr<Geography>> features_;
};

class FeatureConstructor : public CollectionConstructor {
 public:
  FeatureConstructor(const Options& options) : CollectionConstructor(options) {}

  Result feat_start() {
    active_constructor_ = nullptr;
    level_ = 0;
    features_.clear();
    geom_start(util::GeometryType::GEOMETRYCOLLECTION, 1);
    return Result::CONTINUE;
  }

  std::unique_ptr<Geography> finish_feature() {
    geom_end();

    if (features_.empty()) {
      return absl::make_unique<GeographyCollection>();
    } else {
      std::unique_ptr<Geography> feature = std::move(features_.back());
      if (feature == nullptr) {
        throw Exception("finish_feature() generated nullptr");
      }

      features_.pop_back();
      return feature;
    }
  }
};

}  // namespace util

}  // namespace s2geography
