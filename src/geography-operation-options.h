
#include <sstream>
#include <Rcpp.h>
#include "s2/s2boolean_operation.h"

// This class wraps several concepts in the S2BooleanOperation,
// and S2Layer, parameterized such that these can be specified from R
class GeographyOperationOptions {
public:
  int polygonModel;
  int polylineModel;
  int snapLevel;

  // deaults: use S2 defaults
  GeographyOperationOptions(): polygonModel(-1), polylineModel(-1), snapLevel(-1) {}

  // 0 = open, 1 = semi_open, 2 = closed
  void setPolygonModel(int model) {
    this->polygonModel = model;
  }

  // 0 = open, 1 = semi_open, 2 = closed
  void setPolylineModel(int model) {
    this->polylineModel = model;
  }

  // this refers to the cell level, not the rounding
  // precision
  void setSnapLevel(int snapLevel) {
    this->snapLevel = snapLevel;
  }

  // build options for passing this to the S2BooleanOperation
  S2BooleanOperation::Options booleanOperationOptions() {
    S2BooleanOperation::Options options;
    if (this->polygonModel >= 0) {
      options.set_polygon_model(getPolygonModel(this->polygonModel));
    }

    if (this->polylineModel >= 0) {
      options.set_polyline_model(getPolylineModel(this->polylineModel));
    }

    if (this->snapLevel > 0) {
      options.set_snap_function(s2builderutil::S2CellIdSnapFunction(this->snapLevel));
    }

    return options;
  }

  static S2BooleanOperation::PolygonModel getPolygonModel(int model) {
    switch (model) {
      case 0: return S2BooleanOperation::PolygonModel::OPEN;
      case 1: return S2BooleanOperation::PolygonModel::SEMI_OPEN;
      case 2: return S2BooleanOperation::PolygonModel::CLOSED;
      default:
        std::stringstream err;
        err << "Invalid value for polygon model: " << model;
        Rcpp::stop(err.str());
    }
  }

  static S2BooleanOperation::PolylineModel getPolylineModel(int model) {
    switch (model) {
      case 0: return S2BooleanOperation::PolylineModel::OPEN;
      case 1: return S2BooleanOperation::PolylineModel::SEMI_OPEN;
      case 2: return S2BooleanOperation::PolylineModel::CLOSED;
      default:
        std::stringstream err;
        err << "Invalid value for polyline model: " << model;
        Rcpp::stop(err.str());
    }
  }
};