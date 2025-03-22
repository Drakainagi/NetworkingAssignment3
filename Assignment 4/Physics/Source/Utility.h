#ifndef UTILITY_H
#define UTILITY_H

#include "Mtx44.h"
#include "Vertex.h"

Position operator*(const Mtx44& lhs, const Position& rhs);

void solveQuadratic(double a, double b, double c, double* result1, double* result2);
#endif