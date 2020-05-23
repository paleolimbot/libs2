
#ifndef LIBS2_GEOGRAPHY_COLLECTION_H
#define LIBS2_GEOGRAPHY_COLLECTION_H

#include <algorithm>
#include "libs2-geography.h"

// This class handles collections of other LibS2Geography
// objects.
class LibS2GeographyCollection: public LibS2Geography {
public:
  LibS2GeographyCollection(): features(0) {}
  LibS2GeographyCollection(std::vector<std::unique_ptr<LibS2Geography>> features): 
    features(std::move(features)) {}

  bool IsCollection() {
    return this->features.size() > 0;
  }

  int Dimension() {
    int dimension = -1;
    for (size_t i = 0; i < this->features.size(); i++) {
      dimension = std::max<int>(this->features[i]->Dimension(), dimension);
    }

    return dimension;
  }

  int NumPoints() {
    int numPoints = 0;
    for (size_t i = 0; i < this->features.size(); i++) {
      numPoints += this->features[i]->NumPoints();
    }
    return numPoints;
  }

  double Area() {
    double area = 0;
    for (size_t i = 0; i < this->features.size(); i++) {
      area += this->features[i]->Area();
    }
    return area;
  }

  double Length() {
    double length = 0;
    for (size_t i = 0; i < this->features.size(); i++) {
      length += this->features[i]->Length();
    }
    return length;
  }

  double Perimeter() {
    double perimeter = 0;
    for (size_t i = 0; i < this->features.size(); i++) {
      perimeter += this->features[i]->Perimeter();
    }
    return perimeter;
  }

  double X() {
    Rcpp::stop("Can't compute X value of a non-point geography");
  }

  double Y() {
    Rcpp::stop("Can't compute Y value of a non-point geography");
  }

  S2Point Centroid() {
    S2Point cumCentroid(0, 0, 0);
    for (size_t i = 0; i < this->features.size(); i++) {
      S2Point centroid = this->features[i]->Centroid();
      if (centroid.Norm2() > 0) {
        cumCentroid += centroid.Normalize();
      }
    }
    
    return cumCentroid;
  }

  std::unique_ptr<LibS2Geography> Boundary() {
    std::vector<std::unique_ptr<LibS2Geography>> featureBoundaries(this->features.size());
    for (size_t i = 0; i < this->features.size(); i++) {
      featureBoundaries[i] = this->features[i]->Boundary();
    }

    return absl::make_unique<LibS2GeographyCollection>(std::move(featureBoundaries));
  }

  virtual void BuildShapeIndex(MutableS2ShapeIndex* index) {
    for (size_t i = 0; i < this->features.size(); i++) {
      this->features[i]->BuildShapeIndex(index);
    }
  }

  virtual void Export(WKGeometryHandler* handler, uint32_t partId) {
    WKGeometryMeta meta(WKGeometryType::GeometryCollection, false, false, false);
    meta.hasSize = true;
    meta.size = this->features.size();

    handler->nextGeometryStart(meta, partId);
    for (size_t i = 0; i < this->features.size(); i++) {
      this->features[i]->Export(handler, i);
    }
    handler->nextGeometryEnd(meta, partId);
  }

  class Builder: public LibS2GeographyBuilder {
  public:

    Builder(): metaPtr(nullptr), builderPtr(nullptr), builderMetaPtr(nullptr) {}

    virtual void nextGeometryStart(const WKGeometryMeta& meta, uint32_t partId) {
      // if this is the first call, store the meta reference associated with this geometry
      if (this->metaPtr == nullptr) {
        this->metaPtr = (WKGeometryMeta*) &meta;
        return;
      }

      if (!this->builderPtr) {
        // store a reference to the meta associated with this
        // builder so that we know when the corresponding nextGeometryEnd()
        // is called
        this->builderMetaPtr = (WKGeometryMeta*) &meta;

        switch (meta.geometryType) {
        case WKGeometryType::Point:
        case WKGeometryType::MultiPoint:
          this->builderPtr = absl::make_unique<LibS2PointGeography::Builder>();
          break;
        case WKGeometryType::LineString:
        case WKGeometryType::MultiLineString:
          this->builderPtr = absl::make_unique<LibS2PolylineGeography::Builder>();
          break;
        case WKGeometryType::Polygon:
        case WKGeometryType::MultiPolygon:
          this->builderPtr = absl::make_unique<LibS2PolygonGeography::Builder>();
          break;
        case WKGeometryType::GeometryCollection:
          this->builderPtr = absl::make_unique<LibS2GeographyCollection::Builder>();
          break;
        default:
          std::stringstream err;
          err << "Unknown geometry type in geography builder: " << meta.geometryType;
          Rcpp::stop(err.str());
        }
      }

      this->builder()->nextGeometryStart(meta, partId);
    }

    virtual void nextLinearRingStart(const WKGeometryMeta& meta, uint32_t size, uint32_t ringId) {
      this->builder()->nextLinearRingStart(meta, size, ringId);
    }

    virtual void nextCoordinate(const WKGeometryMeta& meta, const WKCoord& coord, uint32_t coordId) {
      this->builder()->nextCoordinate(meta, coord, coordId);
    }

    virtual void nextLinearRingEnd(const WKGeometryMeta& meta, uint32_t size, uint32_t ringId) {
      this->builder()->nextLinearRingEnd(meta, size, ringId);
    }

    virtual void nextGeometryEnd(const WKGeometryMeta& meta, uint32_t partId) {
      // the end of this GEOMETRYCOLLECTION
      if (&meta == this->metaPtr) {
        return;
      }

      this->builder()->nextGeometryEnd(meta, partId);

      if (&meta == this->builderMetaPtr) {
        std::unique_ptr<LibS2Geography> feature = this->builder()->build();
        features.push_back(std::move(feature));
        this->builderPtr = std::unique_ptr<LibS2GeographyBuilder>(nullptr);
        this->builderMetaPtr = nullptr;
      }
    }

    std::unique_ptr<LibS2Geography> build() {
      return absl::make_unique<LibS2GeographyCollection>(std::move(this->features));
    }

  private:
    std::vector<std::unique_ptr<LibS2Geography>> features;
    WKGeometryMeta* metaPtr;
    std::unique_ptr<LibS2GeographyBuilder> builderPtr;
    WKGeometryMeta* builderMetaPtr;

    LibS2GeographyBuilder* builder() {
      if (this->builderPtr) {
        return this->builderPtr.get();
      } else {
        Rcpp::stop("Invalid nesting in geometrycollection (can't find nested builder)");
      }
    }
  };

private:
  std::vector<std::unique_ptr<LibS2Geography>> features;
};

#endif