
#include "s2/s2latlng.h"
#include "s2/s2polyline.h"
#include "s2/s2polygon.h"
#include "s2/s2point_vector_shape.h"
#include "wk/rcpp-io.h"

#include "wk/wkb-reader.h"
#include "wk/wkt-reader.h"
#include "wk/geometry-formatter.h"
#include "wk/geometry-handler.h"

#include "libs2-s2geography.h"
#include <Rcpp.h>
using namespace Rcpp;


// keeping these here for now to keep from recompiling all the time
class LibS2PointGeography: public LibS2Geography {
public:
  LibS2PointGeography(): isEmpty(true) {}
  LibS2PointGeography(S2LatLng point): isEmpty(false), point(point) {}

  bool IsCollection() {
    return false;
  }

  int Dimension() {
    return  0;
  }

  int NumPoints() {
    return !isEmpty;
  }

  double Area() {
    return 0;
  }

  double Length() {
    return 0;
  }

  double Perimeter() {
    return 0;
  }

  double X() {
    if (this->isEmpty) {
      return NA_REAL;
    } else {
      return point.lng().degrees();
    }
  }

  double Y() {
    if (this->isEmpty) {
      return NA_REAL;
    } else {
      return point.lat().degrees();
    }
  }

  virtual void BuildShapeIndex(MutableS2ShapeIndex* index) {
    if (!this->isEmpty) {
      std::vector<S2Point> points(1);
      points[0] = S2Point(this->point);
      index->Add(std::unique_ptr<S2PointVectorShape>(new S2PointVectorShape(std::move(points))));
    }
  }

  virtual void Export(WKGeometryHandler* handler, uint32_t partId) {
    WKGeometryMeta meta(WKGeometryType::Point, false, false, false);
    meta.hasSize = true;
    meta.size = !this->isEmpty;

    handler->nextGeometryStart(meta, partId);
    if (!this->isEmpty) {
      handler->nextCoordinate(meta, WKCoord::xy(point.lng().degrees(), point.lat().degrees()), 0);
    }
    handler->nextGeometryEnd(meta, partId);
  }

private:
  bool isEmpty;
  S2LatLng point;
};



class WKLibS2GeographyWriter: public WKGeometryHandler {
public:
  List s2geography;
  R_xlen_t featureId;

  WKLibS2GeographyWriter(R_xlen_t size): s2geography(size) {}

  void nextFeatureStart(size_t featureId) {
    this->featureId = featureId;
  }

  void nextNull(size_t featureId) {
    this->s2geography[featureId] = R_NilValue;
  }

  void nextGeometryStart(const WKGeometryMeta& meta, uint32_t partId) {
    if (meta.geometryType != WKGeometryType::Point) {
      stop("Can't create a geography that is not a point (yet!!)");
    } else if(meta.size == 0) {
      s2geography[this->featureId] = XPtr<LibS2PointGeography>(new LibS2PointGeography());
    }
  }

  void nextCoordinate(const WKGeometryMeta& meta, const WKCoord& coord, uint32_t coordId) {
    S2LatLng feature = S2LatLng::FromDegrees(coord.y, coord.x);
    s2geography[this->featureId] = XPtr<LibS2PointGeography>(new LibS2PointGeography(feature));
  }
};

// [[Rcpp::export]]
List s2geography_from_wkb(List wkb) {
  WKRawVectorListProvider provider(wkb);
  WKLibS2GeographyWriter writer(wkb.size());
  WKBReader reader(provider);
  reader.setHandler(&writer);

  while (reader.hasNextFeature()) {
    reader.iterateFeature();
  }

  return writer.s2geography;
}

// [[Rcpp::export]]
List s2geography_from_wkt(CharacterVector wkt) {
  WKCharacterVectorProvider provider(wkt);
  WKLibS2GeographyWriter writer(wkt.size());
  WKTReader reader(provider);
  reader.setHandler(&writer);

  while (reader.hasNextFeature()) {
    reader.iterateFeature();
  }

  return writer.s2geography;
}

// this should be added to wk
class WKListProvider: public WKProvider {
public:
  List& input;
  R_xlen_t index;

  WKListProvider(List& input): input(input), index(-1) {}

  SEXP feature() {
    return this->input[this->index];
  }

  bool seekNextFeature() {
    this->index++;
    return this->index < input.size();
  }

  bool featureIsNull() {
    return this->input[this->index] == R_NilValue;
  }

  size_t nFeatures() {
    return input.size();
  }

  void reset() {
    this->index = -1;
  }
};

class WKLibS2GeographyReader: public WKReader {
public:

  WKLibS2GeographyReader(WKListProvider& provider):
  WKReader(provider), provider(provider) {}

  void readFeature(size_t featureId) {
    this->handler->nextFeatureStart(featureId);

    if (this->provider.featureIsNull()) {
      this->handler->nextNull(featureId);
    } else {
      XPtr<LibS2Geography> geography(this->provider.feature());
      geography->Export(handler, WKReader::PART_ID_NONE);
    }

    this->handler->nextFeatureEnd(featureId);
  }

  virtual void readItem(SEXP item) {
    WKGeometryMeta meta(WKGeometryType::Point, false, false, false);
    meta.hasSize = true;
    meta.size = 1;

    this->handler->nextGeometryStart(meta, PART_ID_NONE);

    XPtr<S2LatLng> ptr(item);
    const WKCoord coord = WKCoord::xy(ptr->lng().degrees(), ptr->lat().degrees());
    this->handler->nextCoordinate(meta, coord, 0);
    this->handler->nextGeometryEnd(meta, PART_ID_NONE);
  }

private:
  WKListProvider& provider;
};

// [[Rcpp::export]]
CharacterVector s2geography_format(List s2geography, int maxCoords) {
  WKListProvider provider(s2geography);
  WKCharacterVectorExporter exporter(s2geography.size());
  WKGeometryFormatter formatter(exporter, maxCoords);

  WKLibS2GeographyReader reader(provider);
  reader.setHandler(&formatter);

  while (reader.hasNextFeature()) {
    reader.iterateFeature();
  }

  return exporter.output;
}