/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

// Description : Common vector class



#include "CryEndian.h"
#include <AzCore/Math/Vector3.h>
#include "Cry_Vector2.h"   // CryCommon->AzCore migration: for VEC_EPSILON + Vec2_tpl interop

template<typename T>
struct VecPrecisionValues
{
    ILINE static bool CheckGreater(const T value)
    {
        return value > 0;
    }
};

template<>
struct VecPrecisionValues<float>
{
    ILINE static bool CheckGreater(const float value)
    {
        return value > FLT_EPSILON;
    }
};


template <typename F>
struct Vec3s_tpl
{
    F x, y, z;

    ILINE Vec3s_tpl(F vx, F vy, F vz)
        : x(vx)
        , y(vy)
        , z(vz){}
    ILINE F& operator [] (int32 index)        { assert(index >= 0 && index <= 2); return ((F*)this)[index]; }
    ILINE F operator [] (int32 index) const { assert(index >= 0 && index <= 2); return ((F*)this)[index]; }
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// class Vec3_tpl
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <typename F>
struct Vec3_tpl
{
    typedef F value_type;
    enum
    {
        component_count = 3
    };

    F x, y, z;

#if defined(_DEBUG)
    ILINE Vec3_tpl()
    {
        if constexpr (sizeof(F) == 4)
        {
            uint32* p = alias_cast<uint32*>(&x);
            p[0] = F32NAN;
            p[1] = F32NAN;
            p[2] = F32NAN;
        }
        if constexpr (sizeof(F) == 8)
        {
            uint64* p = alias_cast<uint64*>(&x);
            p[0] = F64NAN;
            p[1] = F64NAN;
            p[2] = F64NAN;
        }
    }
#else
    ILINE Vec3_tpl()    {};
#endif

    /*!
    * template specialization to initialize a vector
    *
    * Example:
    *  Vec3 v0=Vec3(ZERO);
    *  Vec3 v1=Vec3(MIN);
    *  Vec3 v2=Vec3(MAX);
    */
    Vec3_tpl(type_zero)
        : x(0)
        , y(0)
        , z(0) {}
    Vec3_tpl(type_min);
    Vec3_tpl(type_max);

    /*!
    * constructors and bracket-operator to initialize a vector
    *
    * Example:
    *  Vec3 v0=Vec3(1,2,3);
    *  Vec3 v1(1,2,3);
    *  v2.Set(1,2,3);
    */
    ILINE Vec3_tpl(F vx, F vy, F vz)
        : x(vx)
        , y(vy)
        , z(vz){ assert(this->IsValid()); }
    ILINE void operator () (F vx, F vy, F vz) { x = vx; y = vy; z = vz; assert(this->IsValid()); }
    ILINE Vec3_tpl<F>& Set(const F xval, const F yval, const F zval) { x = xval; y = yval; z = zval; assert(this->IsValid()); return *this; }

    explicit ILINE Vec3_tpl(F f)
        : x(f)
        , y(f)
        , z(f) { assert(this->IsValid()); }

    // CryCommon->AzCore migration: implicit both ways so Vec3_tpl<F> still interops with
    // AZ::Vector3 (which `Vec3` now aliases). REMOVE with the Vec3_tpl template in Wave 3.
    ILINE Vec3_tpl(const AZ::Vector3& v)
    {
        x = static_cast<F>(v.GetX());
        y = static_cast<F>(v.GetY());
        z = static_cast<F>(v.GetZ());
    }

    ILINE operator AZ::Vector3() const
    {
        return AZ::Vector3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }

    /*!
    * the copy/casting/assignement constructor
    *
    * Example:
    *  Vec3 v0=v1;
    *  Vec3 v0=Vec3(angle);
    *  Vec3 v0=Vec3(vector4);
    */
    ILINE Vec3_tpl(const Vec3_tpl& v)   {   x = v.x; y = v.y; z = v.z; }
    template<class F1>
    ILINE  Vec3_tpl<F>(const Vec3_tpl<F1>&v)  {
        x = F(v.x);
        y = F(v.y);
        z = F(v.z);
        assert(IsValid());
    }

    ILINE Vec3_tpl<F>(const Vec2_tpl<F>&v) {
        x = v.x;
        y = v.y;
        z = 0;
        assert(IsValid());
    }
    template<class T>
    ILINE Vec3_tpl<F>(const Vec2_tpl<T>&v) {
        x = F(v.x);
        y = F(v.y);
        z = 0;
        assert(IsValid());
    }

    /*!
    * overloaded arithmetic operator
    *
    * Example:
    *  Vec3 v0=v1*4;
    */
    ILINE Vec3_tpl<F> operator * (F k) const
    {
        const Vec3_tpl<F> v = *this;
        return Vec3_tpl<F>(v.x * k, v.y * k, v.z * k);
    }
    ILINE Vec3_tpl<F> operator / (F k) const
    {
        k = (F)1.0 / k;
        return Vec3_tpl<F>(x * k, y * k, z * k);
    }
    ILINE friend Vec3_tpl<F> operator * (F f, const Vec3_tpl& vec)
    {
        return Vec3_tpl((F)(f * vec.x), (F)(f * vec.y), (F)(f * vec.z));
    }

    ILINE Vec3_tpl<F>& operator *= (F k)
    {
        x *= k;
        y *= k;
        z *= k;
        return *this;
    }
    ILINE Vec3_tpl<F>& operator /= (F k)
    {
        k = (F)1.0 / k;
        x *= k;
        y *= k;
        z *= k;
        return *this;
    }

    ILINE Vec3_tpl<F> operator - (void) const
    {
        return Vec3_tpl<F>(-x, -y, -z);
    }
    ILINE Vec3_tpl<F>& Flip()
    {
        x = -x;
        y = -y;
        z = -z;
        return *this;
    }


    /*!
    * bracked-operator
    *
    * Example:
    *  uint32 t=v[0];
    */
    ILINE F& operator [] (int32 index)        { assert(index >= 0 && index <= 2); return ((F*)this)[index]; }
    ILINE F operator [] (int32 index) const { assert(index >= 0 && index <= 2); return ((F*)this)[index]; }


    /*!
    * functions and operators to compare vector
    *
    * Example:
    *  if (v0==v1) dosomething;
    */
    ILINE bool operator==(const Vec3_tpl<F>& vec)
    {
        return x == vec.x && y == vec.y && z == vec.z;
    }
    ILINE bool operator!=(const Vec3_tpl<F>& vec)
    {
        return !(*this == vec);
    }

    ILINE friend bool operator ==(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1)
    {
        return ((v0.x == v1.x) && (v0.y == v1.y) && (v0.z == v1.z));
    }
    ILINE friend bool operator !=(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1)
    {
        return !(v0 == v1);
    }

    ILINE bool IsZero(F e = (F) 0.0) const
    {
        return (AZStd::abs(x) <= e) && (AZStd::abs(y) <= e) && (AZStd::abs(z) <= e);
    }

    ILINE bool IsZeroFast(F e = (F) 0.0003) const
    {
        return (AZStd::abs(x) + AZStd::abs(y) + AZStd::abs(z)) <= e;
    }

    // Chebyshev distance (axis aligned)
    ILINE bool IsEquivalent(const Vec3_tpl<F>& v1, F epsilon = VEC_EPSILON) const
    {
        assert(v1.IsValid());
        assert(this->IsValid());
        return  ((AZStd::abs(x - v1.x) <= epsilon) &&   (AZStd::abs(y - v1.y) <= epsilon) && (AZStd::abs(z - v1.z) <= epsilon));
    }
    ILINE static bool IsEquivalent(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, F epsilon = VEC_EPSILON)
    {
        assert(v0.IsValid());
        assert(v1.IsValid());
        return  ((AZStd::abs(v0.x - v1.x) <= epsilon) &&    (AZStd::abs(v0.y - v1.y) <= epsilon) &&  (AZStd::abs(v0.z - v1.z) <= epsilon));
    }

    // Euclidean distance L2
    ILINE bool IsEquivalentL2(const Vec3_tpl<F>& v1, F epsilon = VEC_EPSILON) const
    {
        assert(v1.IsValid());
        assert(this->IsValid());
        return (*this - v1).GetLengthSquared() <= (epsilon * epsilon);
    }
    ILINE static bool IsEquivalentL2(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, F epsilon = VEC_EPSILON)
    {
        assert(v0.IsValid());
        assert(v1.IsValid());
        return (v0 - v1).GetLengthSquared() <= (epsilon * epsilon);
    }

    ILINE bool IsUnit(F epsilon = VEC_EPSILON) const
    {
        return (AZStd::abs(1 - GetLengthSquared()) <= epsilon);
    }

    bool IsValid() const
    {
        if (!NumberValid(x))
        {
            return false;
        }
        if (!NumberValid(y))
        {
            return false;
        }
        if (!NumberValid(z))
        {
            return false;
        }
        return true;
    }

    //! force vector length by normalizing it
    ILINE void SetLength(F fLen)
    {
        F fLenMe = GetLengthSquared();
        if (fLenMe < 0.00001f * 0.00001f)
        {
            return;
        }
        fLenMe = fLen * AZ::InvSqrt(fLenMe);
        x *= fLenMe;
        y *= fLenMe;
        z *= fLenMe;
    }

    ILINE void ClampLength(F maxLength)
    {
        F sqrLength = GetLengthSquared();
        if (sqrLength > (maxLength * maxLength))
        {
            F scale = maxLength * AZ::InvSqrt(sqrLength);
            x *= scale;
            y *= scale;
            z *= scale;
        }
    }

    //! calculate the length of the vector
    ILINE F GetLength() const
    {
        return AZStd::sqrt(x * x + y * y + z * z);
    }

    ILINE F GetLengthFloat() const
    {
        return GetLength();
    }

    ILINE F GetLengthFast() const
    {
        return AZStd::sqrt(x * x + y * y + z * z);
    }

    //! calculate the squared length of the vector
    ILINE F GetLengthSquared() const
    {
        return x * x + y * y + z * z;
    }

    ILINE F GetLengthSquaredFloat() const
    {
        return GetLengthSquared();
    }

    //! calculate the length of the vector ignoring the z component
    ILINE F GetLength2D() const
    {
        return AZStd::sqrt(x * x + y * y);
    }

    //! calculate the squared length of the vector ignoring the z component
    ILINE F GetLengthSquared2D() const
    {
        return x * x + y * y;
    }

    ILINE F GetDistance(const Vec3_tpl<F>& vec1) const
    {
        return AZStd::sqrt((x - vec1.x) * (x - vec1.x) + (y - vec1.y) * (y - vec1.y) + (z - vec1.z) * (z - vec1.z));
    }
    ILINE F GetSquaredDistance (const Vec3_tpl<F>& v) const
    {
        return (x - v.x) * (x - v.x) + (y - v.y) * (y - v.y) + (z - v.z) * (z - v.z);
    }
    ILINE F GetSquaredDistance2D (const Vec3_tpl<F>& v) const
    {
        return (x - v.x) * (x - v.x) + (y - v.y) * (y - v.y);
    }

    //! Normalize the vector.
    // The default Normalize function is in fact "safe". 0 vectors remain unchanged.
    ILINE void  Normalize()
    {
        assert(this->IsValid());
        F fInvLen = AZ::InvSqrt(x * x + y * y + z * z);
        x *= fInvLen;
        y *= fInvLen;
        z *= fInvLen;
    }

    //! May be faster and less accurate.
    ILINE void NormalizeFast()
    {
        assert(this->IsValid());
        F fInvLen = AZ::InvSqrt(x * x + y * y + z * z);
        x *= fInvLen;
        y *= fInvLen;
        z *= fInvLen;
    }

    //! Normalize the vector to a scale.
    ILINE void Normalize(F scale)
    {
        assert(this->IsValid());
        F fInvLen = AZ::InvSqrt(x * x + y * y + z * z) * scale;
        x *= fInvLen;
        y *= fInvLen;
        z *= fInvLen;
    }
    ILINE void NormalizeFast(F scale)
    {
        assert(this->IsValid());
        F fInvLen = AZ::InvSqrt(x * x + y * y + z * z) * scale;
        x *= fInvLen;
        y *= fInvLen;
        z *= fInvLen;
    }

    //! Normalize the vector.
    // check for null vector - set to the passed in vector (which should be normalised!) if it is null vector
    // returns the original length of the vector
    ILINE F NormalizeSafe(const struct Vec3_tpl<F>& safe = Vec3_tpl<F>(0, 0, 0))
    {
        assert(this->IsValid());
        F fLen2 = x * x + y * y + z * z;
        IF (VecPrecisionValues<F>::CheckGreater(fLen2), 1)
        {
            F fInvLen = AZ::InvSqrt(fLen2);
            x *= fInvLen;
            y *= fInvLen;
            z *= fInvLen;
            return F(1) / fInvLen;
        }
        else
        {
            *this = safe;
            return F(0);
        }
    }

    ILINE Vec3_tpl GetNormalizedFloat() const
    {
        return GetNormalized();
    }

    //! Return a normalized vector.
    ILINE Vec3_tpl GetNormalized() const
    {
        F fInvLen = AZ::InvSqrt(x * x + y * y + z * z);
        return *this * fInvLen;
    }

    //! Return a normalized vector.
    ILINE Vec3_tpl GetNormalizedFast() const
    {
        F fInvLen = AZ::InvSqrt(x * x + y * y + z * z);
        return *this * fInvLen;
    }

    //! Return a safely normalized vector - returns safe vector (should be normalised) if original is zero length.
    ILINE Vec3_tpl GetNormalizedSafe(const struct Vec3_tpl<F>& safe = Vec3_tpl<F>(1, 0, 0)) const
    {
        F fLen2 = x * x + y * y + z * z;
        IF (VecPrecisionValues<F>::CheckGreater(fLen2), 1)
        {
            F fInvLen = AZ::InvSqrt(fLen2);
            return *this * fInvLen;
        }
        else
        {
            return safe;
        }
    }

    //! Return a safely normalized vector - returns safe vector (should be normalised) if original is zero length.
    ILINE Vec3_tpl GetNormalizedSafeFloat(const struct Vec3_tpl<F>& safe = Vec3_tpl<F>(1, 0, 0)) const
    {
        return GetNormalizedSafe(safe);
    }

    //! Return a normalized and scaled vector.
    ILINE Vec3_tpl GetNormalized(F scale) const
    {
        F fInvLen = AZ::InvSqrt(x * x + y * y + z * z);
        return *this * (fInvLen * scale);
    }
    ILINE Vec3_tpl GetNormalizedFast(F scale) const
    {
        F fInvLen = AZ::InvSqrt(x * x + y * y + z * z);
        return *this * (fInvLen * scale);
    }

    //! Permutate coordinates so that z goes to new_z slot.
    ILINE Vec3_tpl GetPermutated(int new_z) const
    {
        static constexpr int inc_mod3[3] = { 1, 2, 0 };
        static constexpr int dec_mod3[3] = { 2, 0, 1 };
        return Vec3_tpl(*(&x + inc_mod3[new_z]), *(&x + dec_mod3[new_z]), *(&x + new_z));
    }

    //! Returns volume of a box with this vector as diagonal.
    ILINE F GetVolume() const { return x * y * z; }

    //! Returns a vector that consists of absolute values of this one's coordinates.
    ILINE Vec3_tpl<F> abs() const
    {
        return Vec3_tpl(AZStd::abs(x), AZStd::abs(y), AZStd::abs(z));
    }

    //! Check for min bounds.
    ILINE void CheckMin(const Vec3_tpl<F> other)
    {
        x = AZ::GetMin(other.x, x);
        y = AZ::GetMin(other.y, y);
        z = AZ::GetMin(other.z, z);
    }
    //! Check for max bounds.
    ILINE void CheckMax(const Vec3_tpl<F> other)
    {
        x = AZ::GetMax(other.x, x);
        y = AZ::GetMax(other.y, y);
        z = AZ::GetMax(other.z, z);
    }



    /*!
    * Sets a vector orthogonal to the input vector.
    *
    * Example:
    *  Vec3 v;
    *  v.SetOrthogonal( i );
    */
    ILINE void SetOrthogonal(const Vec3_tpl<F>& v)
    {
        (F(0.9) * F(0.9)) * (v | v) - v.x * v.x < 0 ? (x = -v.z, y = 0, z = v.x) : (x = 0, y = v.z, z = -v.y);
    }
    //! Returns a vector orthogonal to this one.
    ILINE Vec3_tpl GetOrthogonal() const
    {
        return (F(0.9) * F(0.9)) * (x * x + y * y + z * z) - x * x < 0 ? Vec3_tpl<F>(-z, 0, x) : Vec3_tpl<F>(0, z, -y);
    }

    /*!
    * Project a point/vector on a (virtual) plane
    * Consider we have a plane going through the origin.
    * Because d=0 we need just the normal. The vector n is assumed to be a unit-vector.
    *
    * Example:
    *  Vec3 result=Vec3::CreateProjection( incident, normal );
    */
    ILINE void SetProjection(const Vec3_tpl& i, const Vec3_tpl& n)
    {
        *this = i - n * (n | i);
    }
    ILINE static Vec3_tpl<F> CreateProjection(const Vec3_tpl& i, const Vec3_tpl& n)
    {
        return i - n * (n | i);
    }

    /*!
    * Calculate a reflection vector. Vec3 n is assumed to be a unit-vector.
    *
    * Example:
    *  Vec3 result=Vec3::CreateReflection( incident, normal );
    */
    ILINE void SetReflection(const Vec3_tpl<F>& i, const Vec3_tpl<F>& n)
    {
        *this = (n * (i | n) * 2) - i;
    }
    ILINE static Vec3_tpl<F> CreateReflection(const Vec3_tpl<F>& i, const Vec3_tpl<F>& n)
    {
        return (n * (i | n) * 2) - i;
    }

    /*!
    * Linear-Interpolation between Vec3 (lerp)
    *
    * Example:
    *  Vec3 r=Vec3::CreateLerp( p, q, 0.345f );
    */
    ILINE void SetLerp(const Vec3_tpl<F>& p, const Vec3_tpl<F>& q, F t)
    {
        const Vec3_tpl<F> diff = q - p;
        *this = p + (diff * t);
    }
    ILINE static Vec3_tpl<F> CreateLerp(const Vec3_tpl<F>& p, const Vec3_tpl<F>& q, F t)
    {
        const Vec3_tpl<F> diff = q - p;
        return p + (diff * t);
    }


    /*!
    * Spherical-Interpolation between 3d-vectors (geometrical slerp)
    * both vectors are assumed to be normalized.
    *
    * Example:
    *  Vec3 r=Vec3::CreateSlerp( p, q, 0.5674f );
    */
    void SetSlerp(const Vec3_tpl<F>& p, const Vec3_tpl<F>& q, F t)
    {
        assert(p.IsUnit(0.005f));
        assert(q.IsUnit(0.005f));
        // calculate cosine using the "inner product" between two vectors: p*q=cos(radiant)
        F cosine = AZ::GetClamp((p | q), F(-1), F(1));
        //we explore the special cases where the both vectors are very close together,
        //in which case we approximate using the more economical LERP and avoid "divisions by zero" since sin(Angle) = 0  as   Angle=0
        if (cosine >= (F)0.99)
        {
            SetLerp(p, q, t); //perform LERP:
            Normalize();
        }
        else
        {
            //perform SLERP: because of the LERP-check above, a "division by zero" is not possible
            F rad               = AZStd::acos(cosine);
            F scale_0   = AZStd::sin((1 - t) * rad);
            F scale_1   = AZStd::sin(t * rad);
            *this = (p * scale_0 + q * scale_1) / AZStd::sin(rad);
            Normalize();
        }
    }
    ILINE static Vec3_tpl<F> CreateSlerp(const Vec3_tpl<F>& p, const Vec3_tpl<F>& q, F t)
    {
        Vec3_tpl<F> v;
        v.SetSlerp(p, q, t);
        return v;
    }




    /*!
        * Quadratic-Interpolation between vectors v0,v1,v2.
        * This is repeated linear interpolation from 3 points and the resulting curve is a parabola.
        * If t is in the range [0...1], then the curve goes only through v0 and v2.
        *
        * Example:
        *  Vec3 ip; ip.SetQuadraticCurve( v0,v1,v2, 0.345f );
        */
    ILINE void SetQuadraticCurve(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, F t1)
    {
        F t0 = 1.0f - t1;
        *this = t0 * t0 * v0 + t0 * t1 * 2.0f * v1 + t1 * t1 * v2;
    }
    ILINE static Vec3_tpl<F> CreateQuadraticCurve(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, F t)
    {
        Vec3_tpl<F> ip;
        ip.SetQuadraticCurve(v0, v1, v2, t);
        return ip;
    }

    /*!
    * Cubic-Interpolation between vectors v0,v1,v2,v3.
    * This is repeated linear interpolation from 4 points.
    * If t is in the range [0...1], then the curve goes only through v0 and v3.
    *
    * Example:
    *  Vec3 ip; ip.SetCubicCurve( v0,v1,v2,v3, 0.345f );
    */
    ILINE void SetCubicCurve(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, const Vec3_tpl<F>& v3, F t1)
    {
        F t0 = 1.0f - t1;
        *this = t0 * t0 * t0 * v0 + 3 * t0 * t0 * t1 * v1 + 3 * t0 * t1 * t1 * v2 + t1 * t1 * t1 * v3;
    }
    ILINE static Vec3_tpl<F> CreateCubicCurve(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, const Vec3_tpl<F>& v3, F t)
    {
        Vec3_tpl<F> ip;
        ip.SetCubicCurve(v0, v1, v2, v3, t);
        return ip;
    }

    /*!
       * Spline-Interpolation between vectors v0,v1,v2.
       * This is a variation of a quadratic curve.
       * If t is in the range [0...1], then the spline goes through all 3 points
       *
       * Example:
       *  Vec3 ip; ip.SetSplineInterpolation( v0,v1,v2, 0.345f );
       */
    ILINE void SetQuadraticSpline(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, F t)
    {
        SetQuadraticCurve(v0, v1 - (v0 * 0.5f + v1 + v2 * 0.5f - v1 * 2.0f), v2, t);
    }
    ILINE static Vec3_tpl<F> CreateQuadraticSpline(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, const Vec3_tpl<F>& v2, F t)
    {
        Vec3_tpl<F> ip;
        ip.SetQuadraticSpline(v0, v1, v2, t);
        return ip;
    }

    /*!
    * Rotate a vector using angle&axis.
    *
    * Example:
    *  Vec3 r=v.GetRotated(axis,angle);
    */
    ILINE Vec3_tpl<F> GetRotated(const Vec3_tpl<F>& axis, F angle) const
    {
        return GetRotated(axis, AZStd::cos(angle), AZStd::sin(angle));
    }
    ILINE Vec3_tpl<F> GetRotated(const Vec3_tpl<F>& axis, F cosa, F sina) const
    {
        Vec3_tpl<F> zax = axis * (*this | axis);
        Vec3_tpl<F> xax = *this - zax;
        Vec3_tpl<F> yax = axis % xax;
        return xax * cosa + yax * sina + zax;
    }

    /*!
    * Rotate a vector around a center using angle&axis.
    *
    * Example:
    *  Vec3 r=v.GetRotated(axis,angle);
    */
    ILINE Vec3_tpl<F> GetRotated(const Vec3_tpl& center, const Vec3_tpl<F>& axis, F angle) const
    {
        return center + (*this - center).GetRotated(axis, angle);
    }
    ILINE Vec3_tpl<F> GetRotated(const Vec3_tpl<F>& center, const Vec3_tpl<F>& axis, F cosa, F sina) const
    {
        return center + (*this - center).GetRotated(axis, cosa, sina);
    }

    /*!
    * Component wise multiplication of two vectors.
    */
    ILINE Vec3_tpl CompMul(const Vec3_tpl<F> rhs) const
    {
        return(Vec3_tpl(x * rhs.x, y * rhs.y, z * rhs.z));
    }

    //! Three methods for a "dot-product" operation.
    ILINE F Dot (const Vec3_tpl<F> v)   const
    {
        return x * v.x + y * v.y + z * v.z;
    }
    //! Two methods for a "cross-product" operation.
    ILINE Vec3_tpl<F> Cross (const Vec3_tpl<F> vec2) const
    {
        return Vec3_tpl<F>(y * vec2.z  -  z * vec2.y,     z * vec2.x -    x * vec2.z,   x * vec2.y  -  y * vec2.x);
    }

    //f32* fptr=vec;
    operator F* ()                   { return (F*)this; }
    template <class T>
    explicit Vec3_tpl(const T* src) { x = F(src[0]); y = F(src[1]); z = F(src[2]); }

    ILINE Vec3_tpl& zero() { x = y = z = 0; return *this; }
    ILINE F len() const
    {
        return AZStd::sqrt(x * x + y * y + z * z);
    }
    ILINE F len2() const
    {
        return x * x + y * y + z * z;
    }

    ILINE Vec3_tpl& normalize()
    {
        F len2 = x * x + y * y + z * z;
        if (len2 > (F)1e-20f)
        {
            F rlen = AZ::InvSqrt(len2);
            x *= rlen;
            y *= rlen;
            z *= rlen;
        }
        else
        {
            Set(0, 0, 1);
        }
        return *this;
    }
    ILINE Vec3_tpl normalized() const
    {
        F len2 = x * x + y * y + z * z;
        if (len2 > (F)1e-20f)
        {
            F rlen = AZ::InvSqrt(len2);
            return Vec3_tpl(x * rlen, y * rlen, z * rlen);
        }
        else
        {
            return Vec3_tpl(0, 0, 1);
        }
    }

    //vector subtraction
    template<class F1>
    ILINE Vec3_tpl<F1> sub(const Vec3_tpl<F1>& v) const
    {
        return Vec3_tpl<F1>(x - v.x, y - v.y, z - v.z);
    }
    //vector scale
    template<class F1>
    ILINE Vec3_tpl<F1> scale(const F1 k) const
    {
        return Vec3_tpl<F>(x * k, y * k, z * k);
    }

    //vector dot product
    template<class F1>
    ILINE F1 dot(const Vec3_tpl<F1>& v) const
    {
        return (F1)(x * v.x + y * v.y + z * v.z);
    }
    //vector cross product
    template<class F1>
    ILINE Vec3_tpl<F1> cross(const Vec3_tpl<F1>& v) const
    {
        return Vec3_tpl<F1>(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }
};

// CryCommon->AzCore migration: `Vec3i` removed (no users).

// dot product (2 versions)
template<class F1, class F2>
ILINE F1 operator * (const Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    return v0.Dot(v1);
}
template<class F1, class F2>
ILINE F1 operator | (const Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    return v0.Dot(v1);
}
// cross product (2 versions)
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator ^ (const Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    return v0.Cross(v1);
}
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator % (const Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    return v0.Cross(v1);
}


//vector addition
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator + (const Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    return Vec3_tpl<F1>(static_cast<F1>(v0.x + v1.x), static_cast<F1>(v0.y + v1.y), static_cast<F1>(v0.z + v1.z));
}
//vector addition
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator + (const Vec2_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    return Vec3_tpl<F1>(v0.x + v1.x, v0.y + v1.y, v1.z);
}
//vector addition
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator + (const Vec3_tpl<F1>& v0, const Vec2_tpl<F2>& v1)
{
    return Vec3_tpl<F1>(v0.x + v1.x, v0.y + v1.y, v0.z);
}

//vector subtraction
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator - (const Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    return Vec3_tpl<F1>((F1)(v0.x - v1.x), (F1)(v0.y - v1.y), (F1)(v0.z - v1.z));
}
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator - (const Vec2_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    return Vec3_tpl<F1>(v0.x - v1.x, v0.y - v1.y, 0.0f - v1.z);
}
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator - (const Vec3_tpl<F1>& v0, const Vec2_tpl<F2>& v1)
{
    return Vec3_tpl<F1>(v0.x - v1.x, v0.y - v1.y, v0.z);
}


//---------------------------------------------------------------------------


//vector self-addition
template<class F1, class F2>
ILINE Vec3_tpl<F1>& operator += (Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    v0 = v0 + v1;
    return v0;
}
//vector self-subtraction
template<class F1, class F2>
ILINE Vec3_tpl<F1>& operator -= (Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    v0 = v0 - v1;
    return v0;
}
template<class F1, class F2>
ILINE Vec3_tpl<F1> operator / (const Vec3_tpl<F1>& v0, const Vec3_tpl<F2>& v1)
{
    return Vec3_tpl<F1>(v0.x / v1.x, v0.y / v1.y, v0.z / v1.z);
}

template <class F>
ILINE bool IsEquivalent(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, f32 epsilon = VEC_EPSILON)
{
    return  ((AZStd::abs(v0.x - v1.x) <= epsilon) &&    (AZStd::abs(v0.y - v1.y) <= epsilon) &&  (AZStd::abs(v0.z - v1.z) <= epsilon));
}


///////////////////////////////////////////////////////////////////////////////
// Typedefs                                                                  //
///////////////////////////////////////////////////////////////////////////////

// ###################################################################################
// ##  !!! TEMPORARY CRYCOMMON -> AZCORE MIGRATION SHIM -- REMOVE IN WAVE 3 !!!       ##
// ##                                                                                ##
// ##  `Vec3` is now a TEMPORARY alias of AZ::Vector3 (was Vec3_tpl<f32>). The        ##
// ##  Cry-only free operators below are compat shims so existing call sites still    ##
// ##  build; they MUST be removed once call sites use AzCore APIs directly. The       ##
// ##  generic Vec3_tpl<F> template above is retained only for non-float uses          ##
// ##  (e.g. Vec3i).                                                                  ##
// ##                                                                                ##
// ##  SEMANTIC HAZARD: Cry overloaded `Vec3 * Vec3` to mean DOT product, but          ##
// ##  AZ::Vector3::operator* is COMPONENT-WISE multiply. That difference is           ##
// ##  intentionally NOT shimmed -- such sites must be fixed to use operator| /        ##
// ##  .Dot() during migration.                                                       ##
// ###################################################################################
// CryCommon->AzCore migration: the `Vec3` alias was removed once all callers moved to AZ::Vector3.

template<>
inline Vec3_tpl<f32>::Vec3_tpl(type_min) { x = y = z = -3.3E38f; }
template<>
inline Vec3_tpl<f32>::Vec3_tpl(type_max) { x = y = z = 3.3E38f; }


// CryCommon->AzCore migration: the Ang3_tpl template (Euler angles) was removed; use AZ::Vector3.
// CryCommon->AzCore migration: the dead AngleAxis_tpl and Plane_tpl templates were removed
// (no users). Use AZ::Quaternion::CreateFromAxisAngle / AZ::Plane instead.


// declare common constants.  Must be done after the class for compiler conformance
// (msvc and clang handle instantiation differently)
const Vec3_tpl<float> Vec3_Zero(0, 0, 0);
const Vec3_tpl<float> Vec3_OneX(1, 0, 0);
const Vec3_tpl<float> Vec3_OneY(0, 1, 0);
const Vec3_tpl<float> Vec3_OneZ(0, 0, 1);
const Vec3_tpl<float> Vec3_One(1, 1, 1);

// NOTE (CryCommon->AzCore migration): `Vec3` is now AZ::Vector3, which already carries
// its own AZ_TYPE_INFO ({8379EB7D-...}). The legacy Vec3 type id
// {DFA993FB-4E92-4A13-BDB3-4E9285A5346F} is migrated to AZ::Vector3 via a ClassDeprecate
// converter registered in Wave 2 (see MathReflection). Do NOT re-specialize type info here.
