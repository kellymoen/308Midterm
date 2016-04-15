//---------------------------------------------------------------------------
//
// Copyright (c) 2016 Taehyun Rhee, Joshua Scott, Ben Allen
//
// This software is provided 'as-is' for assignment of COMP308 in ECS,
// Victoria University of Wellington, without any express or implied warranty. 
// In no event will the authors be held liable for any damages arising from
// the use of this software.
//
// The contents of this file may not be copied or duplicated in any form
// without the prior permission of its owner.
//
//----------------------------------------------------------------------------


#include "cgra_math.hpp"

namespace cgra {

	class quat {
	public:
		float w, x, y, z;

		quat() : w(1), x(0), y(0), z(0) { }
		quat(float _w, float _x, float _y, float _z) :  w(_w), x(_x), y(_y), z(_z) { }

		// eular angle constructor (degrees)
		// i = rotation around x, j = y, k = z
		quat(float i, float j, float k) {
			//convert into radians
			float heading = radians(i);
			float attitude = radians(j);
			float bank = radians(k);

			float c1 = std::cos(heading / 2);
			float c2 = std::cos(attitude / 2);
			float c3 = std::cos(bank / 2);
			float s1 = std::sin(heading / 2);
			float s2 = std::sin(attitude / 2);
			float s3 = std::sin(bank / 2);

			w = c1 * c2 * c3 - s1 * s2 * s3;
			x = s1 * s2 * c3 + c1 * c2 * s3;
			y = s1 * c2 * c3 + c1 * s2 * s3;
			z = c1 * s2 * c3 - s1 * c2 * s3;
		}
		
		// angle axis constructor
		quat(float angle, const vec3& p) {
			float ang = radians(angle);
			w = std::cos(ang);
			float s = std::sin(ang);
			x = s * p.x;
			y = s * p.y;
			z = s * p.z;
		}

		// rotation between 2 vectors
		quat(const vec3 &p1, const vec3& p2) {
			w = dot(normalize(p1), normalize(p2));
			float s = std::sin(std::acos(w));
			vec3 c = normalize(cross(p1, p2));
			x = s * c.x;
			y = s * c.y;
			z = s * c.z;
		}

		quat(const quat& q) : x(q.x), y(q.y), z(q.z), w(q.w) { }

		~quat() { }

		static quat checknan(const quat &v) {
			float sum = v.x + v.y + v.z + v.w;
			assert(sum == sum);
			return v;
		}

		const float & operator[](size_t i) const {
			assert(i < 4);
			return *(&w + i);
		}

		float & operator[](size_t i) {
			assert(i < 4);
			return *(&w + i);
		}

		// stream insertion
		inline friend std::ostream & operator<<(std::ostream &out, const quat &q) {
			return out << '(' << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ')';
		}

		explicit operator mat4() const {
			mat4 m;

			m[0][0]  = w * w + x * x - y * y - z * z;
			m[0][1]  = 2 * x * y + 2 * w * z;
			m[0][2]  = 2 * x * z - 2 * w * y;
			m[0][3]  = 0;

			m[1][0]  = 2 * x * y - 2 * w * z;
			m[1][1]  = w * w - x * x + y * y - z * z;
			m[1][2]  = 2 * y * z + 2 * w * x;
			m[1][3]  = 0;

			m[2][0]  = 2 * x * z + 2 * w * y;
			m[2][1]  = 2 * y * z - 2 * w * x;
			m[2][2] = w * w - x * x - y * y + z * z;
			m[2][3] = 0;

			m[3][0] = 0;
			m[3][1] = 0;
			m[3][2] = 0;
			m[3][3] = w * w + x * x + y * y + z * z;

			return m;
		}


		// add-assign
		inline quat & operator+=(const quat &rhs) {
			w += rhs.w;
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			return *this;
		}

		// subtract-assign
		inline quat & operator-=(const quat &rhs) {
			w -= rhs.w;
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;
			return *this;
		}

		// mulitply-assign
		inline quat & operator*=(const quat &rhs) {
			float _w = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;
			float _x = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
			float _y = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x;
			float _z = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w;
			w = _w; x = _x; y = _y; z = _z;
			return *this;
		}

		// mulitply-assign
		inline quat & operator*=(float rhs) {
			w *= rhs;
			x *= rhs;
			y *= rhs;
			z *= rhs;
			return *this;
		}

		// divide-assign with quaternions must be outside of the class

		// divide-assign
		inline quat & operator/=(float rhs) {
			w /= rhs;
			x /= rhs;
			y /= rhs;
			z /= rhs;
			return *this;
		}
	};

	// dot product
	inline float dot(const quat &lhs, const quat &rhs) {
		return lhs.w * rhs.w + lhs.x * rhs.x +  lhs.y * rhs.y + lhs.z * rhs.z;
	}

	// length/magnitude of vector
	inline float length(const quat &q) {
		return std::sqrt(dot(q, q));
	}

	// inverse rotation
	inline quat conjugate(const quat& q) {
		return quat(q.w, -q.x, -q.y, -q.z);
	}

	inline quat multiplicativeInverse(const quat& q) {
		float len = length(q);
		len = len * len;
		quat c = conjugate(q);
		c /= len;
		return c;
	}

	// divide-assign (needs to be declared after multiplicativeInverse)
	inline quat & operator/=(const quat &lhs, const quat &rhs) {
		quat q = lhs;
		quat inv = multiplicativeInverse(rhs);
		return q *= inv;
	}

	// negate
	inline quat operator-(const quat &q) {
		return conjugate(q);
	}

	// add
	inline quat operator+(const quat &lhs, const quat &rhs) {
		quat q = lhs;
		return q += rhs;
	}

	// subtract
	inline quat operator-(const quat &lhs, const quat &rhs) {
		quat q = lhs;
		return q -= rhs;
	}

	// multiply
	inline quat operator*(const quat &lhs, const quat &rhs) {
		quat q = lhs;
		return q *= rhs;
	}

	// multiply
	inline quat operator*(const quat &lhs, float rhs) {
		quat q = lhs;
		return q *= rhs;
	}

	// multiply
	inline quat operator*(float lhs, const quat &rhs) {
		quat q = rhs;
		return q *= lhs;
	}

	// divide
	inline quat operator/(const quat &lhs, const quat &rhs) {
		quat q = lhs;
		return q /= rhs;
	}

	// divide
	inline quat operator/(const quat &lhs, float rhs) {
		quat q = lhs;
		return q /= rhs;
	}

	// returns unit quaternion
	inline quat normalize(const quat &q) {
		return q / length(q);
	}

	// spherical linear interpolation
	inline quat slerp(const quat& p1, const quat& q1, float t) {
		quat q = normalize(q1);
		quat p = normalize(p1);
		float epsilon = 0.0001;
		if (dot(p, q) < 0) {
			q = q * -1;
		}
		float dpq = dot(p, q);

		if ((1.0 - dpq) > epsilon) {
			float w = std::acos(dpq);
			return ((std::sin((1 - t) * w) * p) + (std::sin(t * w) * q)) / std::sin(w);
		}
		
		return (1 - t) * p + t * q;
	}

}