#pragma once
#include "YshMath.h"

template <class T>
class Quat_t
{
public:
	// For constructors, we adopt the convention of AXIS first and ANGLE second
	Quat_t();
	Quat_t(T x_, T y_, T z_, T w_);
	Quat_t(const Vec3_t<T>& axis, T angle);
	Quat_t(const Mat33_t<T>& R);
	virtual ~Quat_t();

	Vec3_t<T> Transform(const Vec3_t<T>& v) const;

	Quat_t<T> operator * (const Quat_t<T>& q) const;
	Quat_t<T> operator - () const; // conjugate http://mathworld.wolfram.com/QuaternionConjugate.html
	
	static Quat_t<T> Identity();

	///////////////
	// VARIABLES //
	///////////////

	T x, y, z;
	T w;
};
