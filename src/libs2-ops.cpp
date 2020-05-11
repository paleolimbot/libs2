#include "s2/s2polygon.h"
#include <Rcpp.h>

using namespace Rcpp;

//' Geometry operators for s2 geometries
//' 
//' @param x list with S2Polygons or S2Polyline pointers
//' @param y list with S2Polygons or S2Polyline pointers
//' @name s2ops
//' @export
//[[Rcpp::export]]
List s2polygon_intersects(List x, List y) {
	std::vector<S2Polygon *> xp(x.size());
	for (int i = 0; i < x.size(); i++) {
		SEXP s = x[i];
		xp[i] = (S2Polygon *) R_ExternalPtrAddr(s);
	}
	std::vector<S2Polygon *> yp(y.size());
	for (int i = 0; i < y.size(); i++) {
		SEXP s = y[i];
		yp[i] = (S2Polygon *) R_ExternalPtrAddr(s);
	}
	for (int i = 0; i < x.size(); i++) {
		IntegerVector ret(0);
		for (int j = 0; j < y.size(); j++)
			if (xp[i]->Intersects(yp[j]))
				ret.push_back(j+1); // R: 1-based index
		x[i] = ret;
	}
	return x;
}

//' @name s2ops
//' @export
//[[Rcpp::export]]
List s2polyline_intersects(List x, List y) {
	std::vector<S2Polyline *> xp(x.size());
	for (int i = 0; i < x.size(); i++) {
		SEXP s = x[i];
		xp[i] = (S2Polyline *) R_ExternalPtrAddr(s);
	}
	std::vector<S2Polyline *> yp(y.size());
	for (int i = 0; i < y.size(); i++) {
		SEXP s = y[i];
		yp[i] = (S2Polyline *) R_ExternalPtrAddr(s);
	}
	for (int i = 0; i < x.size(); i++) {
		IntegerVector ret(0);
		for (int j = 0; j < y.size(); j++)
			if (xp[i]->Intersects(yp[j]))
				ret.push_back(j+1); // R: 1-based index
		x[i] = ret;
	}
	return x;
}

//' @export
//' @name s2ops
//' @param ptrs list of S2Polygon or S2Polyline pointers
//[[Rcpp::export]]
LogicalVector s2polygon_is_valid(List ptrs) {
	LogicalVector ret(ptrs.size());
	for (int i = 0; i < ptrs.size(); i++) {
		SEXP s = ptrs[i];
		S2Polygon *p = (S2Polygon *) R_ExternalPtrAddr(s);
		ret[i] = p->IsValid();
	}
	return ret;
}

//' @export
//' @name s2ops
//' @param ptrs list of S2Polygon or S2Polyline pointers
//[[Rcpp::export]]
LogicalVector s2polyline_is_valid(List ptrs) {
	LogicalVector ret(ptrs.size());
	for (int i = 0; i < ptrs.size(); i++) {
		SEXP s = ptrs[i];
		S2Polyline *l = (S2Polyline *) R_ExternalPtrAddr(s);
		ret[i] = l->IsValid();
	}
	return ret;
}
