#include "Utility.h"

Position operator*(const Mtx44& lhs, const Position& rhs)
{
	float b[4];
	for(int i = 0; i < 4; i++)
		b[i] = lhs.a[0 * 4 + i] * rhs.x + lhs.a[1 * 4 + i] * rhs.y + lhs.a[2 * 4 + i] * rhs.z + lhs.a[3 * 4 + i] * 1;
	return Position(b[0], b[1], b[2]);
}

void solveQuadratic(double a, double b, double c, double* result1, double* result2)
{
	double root = sqrt(b * b - 4.0 * a * c);
	*result1 = (-b + root) / (2.0 * a);
	*result2 = (-b - root) / (2.0 * a);
}