#ifndef MATH_H
#define MATH_H

namespace MultiPLAY
{
	#define LOG2 0.6931471805599453

	extern double lg(double x);
	extern double p2(double x);

	inline double bilinear(double left, double right, double offset)
	{
		return left * (1.0 - offset) + right * offset; // bilinear interpolation
	}

	extern int req_digits(int max);
}

#endif // MATH_H
