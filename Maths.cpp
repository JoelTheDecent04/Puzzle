#include <math.h>
#include <stdlib.h>
#include <stdint.h>


struct v2 { f32 X, Y; };
struct v2i { i32 X, Y; };

struct v4
{
    union
    {
        struct
        {
            f32 X, Y, Z, W;
        };
        struct
        {
            f32 R, G, B, A;
        };
    };
};

inline v2 V2(f32 X, f32 Y)
{
	v2 Result = {X, Y};
	return Result;
}

inline v2 V2(i32 X, i32 Y)
{
	v2 Result = {(f32)X, (f32)Y};
	return Result;
}

inline v4 V4(f32 X, f32 Y, f32 Z, f32 W)
{
    v4 Result = {X, Y, Z, W};
    return Result;
}

inline f32
Length(v2 Vec)
{
	return sqrtf(Vec.X * Vec.X + Vec.Y * Vec.Y);
}

inline f32 
LengthSq(v2 Vec)
{
    return Vec.X * Vec.X + Vec.Y * Vec.Y;
}

float DotProduct(v2 A, v2 B)
{
	return A.X * B.X + A.Y * B.Y;
}

inline v2 UnitV(v2 Vec)
{
	v2 Res;
	float Norm = Length(Vec);
	if (Norm)
	{
		Res.X = Vec.X / Norm;
		Res.Y = Vec.Y / Norm;
	}
	else
	{
		Res.X = 0.0f;
		Res.Y = 0.0f;
	}
	return Res;
}

inline v2 operator+(v2 A, v2 B)
{
	v2 Res = { A.X + B.X, A.Y + B.Y };
	return Res;
}

inline v2 operator-(v2 A, v2 B)
{
	v2 Res = { A.X - B.X, A.Y - B.Y };
	return Res;
}

inline v2 operator*(float Scalar, v2 V)
{
	v2 Res = { Scalar * V.X, Scalar * V.Y };
	return Res;
}

inline v2 operator*(v2 V, float Scalar)
{
	v2 Res = Scalar * V;
	return Res;
}

inline v2& operator+=(v2& A, v2 B)
{
	A = A + B;
	return A;
}

inline v2& operator-=(v2& A, v2 B)
{
	A = A - B;
	return A;
}

bool operator==(v2 A, v2 B)
{
	return A.X == B.X && A.Y == B.Y;
}

inline v2 V2FromInt(int X, int Y)
{
	v2 Res = { (float)X, (float)Y };
	return Res;
}

v2 Hadamard(v2 A, v2 B)
{
	v2 Res = { A.X * B.X, A.Y * B.Y };
	return Res;
}

float VectorAngle(v2 Vec)
{
	return atan2f(Vec.Y, Vec.X);
}

static inline v2
Perp(v2 A)
{
    v2 Result = {-A.Y, A.X};
    return Result;
}

struct rect
{
	v2 MinCorner;
	v2 MaxCorner;
};

//
//----------Rectangles----------
//

static bool
RectanglesCollide(rect A, rect B)
{
    bool Result = (A.MinCorner.X < B.MaxCorner.X &&
                   B.MinCorner.X < A.MaxCorner.X &&
                   A.MinCorner.Y < B.MaxCorner.Y &&
                   B.MinCorner.Y < A.MaxCorner.Y);
    return Result;
}

//
//----------Lines----------
//

struct line_segment
{
	v2 Start;
	v2 End;
};

struct intersection_time
{
	float Time;
	bool DidHit;
};

//
//----------Arithmetic----------
//

inline float Square(float a)
{
	return a * a;
}

inline int Floor(float a)
{
	return (int)floorf(a);
}

inline int Ceil(float a)
{
	return (int)ceilf(a);
}

inline int Round(float a)
{
	return lroundf(a);
}

inline float Abs(float A)
{
	return fabsf(A);
}

inline float Sign(float a)
{
	if (a > 0.0f)
		return 1.0f;
	if (a < 0.0f)
		return -1.0f;
	return 0.0f;
}

inline int RoundDownToMultipleOf(int Multiple, int Value)
{
	return (Value / Multiple) * Multiple;
}

inline int RoundDownToMultipleOf(int Multiple, float Value)
{
	return ((int)Value / Multiple) * Multiple;
}
inline int RoundUpToMultipleOf(int Multiple, int Value)
{
	return ((Value + Multiple - 1) / Multiple) * Multiple;
}

inline int RoundUpToMultipleOf(int Multiple, float Value)
{
	return (((int)Value + Multiple - 1) / Multiple) * Multiple;
}

inline f32 RoundUpToMultipleOf(f32 Multiple, f32 Value)
{
    return Ceil(Value / Multiple) * Multiple;
}

//
//Other
//
inline float Random()
{
	return (float)rand() / (float)RAND_MAX;
}

static inline f32
RandomBetween(f32 Min, f32 MaX)
{
	return Min + Random() * (MaX - Min);
}

static inline f32 
LinearInterpolate(f32 X0, f32 X1, f32 T)
{
	return T * X1 + (1 - T) * X0;
}

static inline v2 
LinearInterpolate(v2 X0, v2 X1, float T)
{
	return T * X1 + (1 - T) * X0;
}

static inline bool
CompareEpsilon(f32 A, f32 B, f32 Epsilon)
{
    bool Result = Abs(A - B) <= Epsilon;
    return Result;
}

static inline bool
CompareEpsilon(v2 A, v2 B, f32 Epsilon)
{
    bool Result = CompareEpsilon(A.X, B.X, Epsilon) && CompareEpsilon(A.Y, B.Y, Epsilon);
    return Result;
}

//
//Trigonometry
//

inline float 
RadiansToDegrees(float Radians)
{
	return Radians * 57.2958f;
}

f32 const Pi = 3.1415926f;

static v2
RotateAround(v2 Center, v2 Point, float Turns)
{
    f32 Radians = 2 * Pi * Turns;
    v2 Vec = Point - Center;
    
    f32 NewVecX = Vec.X * cosf(Radians) - Vec.Y * sinf(Radians);
    f32 NewVecY = Vec.X * sinf(Radians) + Vec.Y * cosf(Radians);
    
    v2 Result = Center + V2(NewVecX, NewVecY);
    return Result;
}

static inline f32
Max(f32 A, f32 B)
{
	f32 Result = A > B ? A : B;
	return Result;
}

static inline f32
Max(f32 A, f32 B, f32 C)
{
	f32 Result = Max(A, Max(B, C));
	return Result;
}

static inline f32
Max(f32 A, f32 B, f32 C, f32 D)
{
	f32 Result = Max(A, B, Max(C, D));
	return Result;
}

static inline f32
Min(f32 A, f32 B)
{
	f32 Result = A < B ? A : B;
	return Result;
}

static inline f32
Min(f32 A, f32 B, f32 C)
{
    f32 Result = Min(A, Min(B, C));
    return Result;
}

static inline f32
Min(f32 A, f32 B, f32 C, f32 D)
{
    f32 Result = Min(A, B, Min(C, D));
    return Result;
}

static inline i32
Min(i32 A, i32 B)
{
    i32 Result = A < B ? A : B;
    return Result;
}

static inline i32
Max(i32 A, i32 B)
{
    i32 Result = A > B ? A : B;
    return Result;
}

static inline f32
Clamp(f32 Val, f32 Min, f32 Max)
{
    Assert(Max >= Min);
    
    f32 Result = Val;
    if (Val < Min)
        Result = Min;
    if (Val > Max)
        Result = Max;
    return Result;
}


static inline i32
Clamp(i32 Val, i32 Min, i32 Max)
{
    Assert(Max >= Min);
    
    i32 Result = Val;
    if (Val < Min)
        Result = Min;
    if (Val > Max)
        Result = Max;
    return Result;
}

struct m2x2
{
	union
	{
		struct { f32 _11, _12, _21, _22; };
		struct { f32 A, B, C, D; };
	};
};

m2x2 M2x2(v2 A, v2 B)
{
	m2x2 Result = { A.X, B.X, A.Y, B.Y };
	return Result;
}

v2 operator*(m2x2 A, v2 B)
{
	v2 Result = { A._11 * B.X + A._12 * B.Y, A._21 * B.X + A._22 * B.Y };
	return Result;
}

static inline f32 
Det(m2x2 M)
{
	return M.A * M.D - M.B * M.C;
}

static inline m2x2 
Inverse(m2x2 M)
{
	f32 det = Det(M);
	m2x2 Result;
	Result.A = M.D / det;
	Result.B = -M.B / det;
	Result.C = -M.C / det;
	Result.D = M.A / det;
	return Result;
}

static inline bool
PointInRect(v2 MinCorner, v2 MaxCorner, v2 Point)
{
	return (Point.X > MinCorner.X && Point.X < MaxCorner.X&& Point.Y > MinCorner.Y && Point.Y < MaxCorner.Y);
}

static inline bool
PointInRect(rect Rect, v2 Point)
{
	return PointInRect(Rect.MinCorner, Rect.MaxCorner, Point);
}
