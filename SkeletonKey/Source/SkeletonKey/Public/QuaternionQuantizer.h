#pragma once
//this is based on the work of Marc Reynolds
// Public Domain under http://unlicense.org, see link for details.
// originally from:
// http://marc-b-reynolds.github.io/quaternions/2017/05/02/QuatQuantPart1.html
// http://marc-b-reynolds.github.io/quaternions/

//I've made an effort to improve the code quality, but this isn't currently live code
//and as a result, it hasn't had a ton of testing. I can tell you that there does seem
//to be a bit of a wobble to the quaternions after a round-trip, but it's likely due to either
//a bug in my usage or a bug in my transcription. the fundamental algorithm and mathematical accuracy
//are both solid, and Marc's implementation is excellent given that he calls it "toy code."
//--JMK 2/17/26
template<unsigned BITS>
class QuaternionQuantizer
{ 
public:
	using quat_t = FQuat4f;
	using quat_wt = FQuat4d;
	using vec3_t = FVector3f;
	static inline float sgn(float x) { return copysignf(1.f,x); }
	protected:
static inline constexpr float RPI   =  0.3183098733425140380859375f;
#define QUAT_BO(OP) \
r->X = AX OP BX;  \
r->Y = AY OP BY;  \
r->Z = AZ OP BZ;  \
r->W = AW OP BW;

#define QUAT_UO(OP) \
r->X = OP AX;     \
r->Y = OP AY;     \
r->Z = OP AZ;     \
r->W = OP AW;
	// another temp hack
#define AX a->X
#define AY a->Y
#define AZ a->Z
#define AW a->W
#define BX b->X
#define BY b->Y
#define BZ b->Z
#define BW b->W
#define QSCALE_(X) ((float)(1<<(X)))
#define QSCALE(X)   QSCALE_(X)
#define DSCALE(X)   (1.f/QSCALE_(X))
static inline constexpr unsigned BITS_E = ((BITS-2)/3);
static inline  constexpr unsigned BITS_M = ((1<<BITS_E)-1);
static inline constexpr unsigned BITS_X = (BITS/3);
static inline constexpr unsigned  BITS_Y  = ((BITS-BITS_X)/2);
static inline constexpr unsigned  BITS_Z = (BITS-BITS_X-BITS_Y);
static inline constexpr unsigned BITS_MX = ((1<<BITS_X)-1);
static inline constexpr unsigned  BITS_MY = ((1<<BITS_Y)-1);
static inline constexpr unsigned BITS_MZ = ((1<<BITS_Z)-1);
	static inline uint64_t quant(float f, float s)
	{
		uint64_t i = (uint64_t)(f*s);
		if (i > s-1.f) i = (uint64_t)(s-1.f);
		return i;
	}

	static inline float dequant(uint64_t b, float s) { return (b+0.5f)*s; }

	static void vsdecode(vec3_t* v, uint64_t b)
	{
		v->X = 2.f*dequant(b & BITS_M, DSCALE(BITS_E))-1.f; b >>= BITS_E;
		v->Y = 2.f*dequant(b & BITS_M, DSCALE(BITS_E))-1.f; b >>= BITS_E;
		v->Z = 2.f*dequant(b & BITS_M, DSCALE(BITS_E))-1.f;
	}

	static inline void quat_add(quat_t* r, quat_t* a, quat_t* b) { QUAT_BO(+) }
	static inline void quat_sub(quat_t* r, quat_t* a, quat_t* b) { QUAT_BO(-) }

	static inline void quat_neg(quat_t* r, quat_t* a)            { QUAT_UO(-) }
	static void quat_to_zxz(vec3_t* v, quat_t* q)
	{
		double x=q->X, y=q->Y, z=q->Z, w=q->W;

		double s2 = x*x+y*y;      // sin(beta)^2
		double c2 = w*w+z*z;      // cos(beta)^2
		double s  = atan(z/w);    // (gamma+alpha)/2
		double d  = atan2(y,x);   // (gamma-alpha)/2
		v->Y = s-d;               // alpha
		v->Z = s+d;               // gamma

		// this is a hack I can't be bothered to fix.
		// if 'x' is negative we don't have a min range
		// for gamma or alpha. The proper fix is to
		// convert computing 'd' by single parameter
		// atan and pull-out the arg reduction and
		// constants. Could fold with 's' arg reduction
		// as well.
		if (x < 0) {
			if (fabs(v->Y) > PI) v->Y -= sgn(v->Y)*2.f*PI;
			if (fabs(v->Z) > PI) v->Z -= sgn(v->Z)*2.f*PI;
		}
  
		if (c2 != 0.0)
			v->X = 2.0*atan(sqrt(s2/c2));
		else
			v->X = (0.5 > s2) ? 0 : PI;
	}

	static void zxz_to_quat(quat_t* q, vec3_t* v)
	{
		float a = 0.5f*v->Y;
		float b = 0.5f*v->X;
		float c = 0.5f*v->Z;

		float scma = sinf(c-a), ccma = cosf(c-a);
		float scpa = sinf(c+a), ccpa = cosf(c+a);
		float sb   = sinf(b),   cb   = cosf(b);

		q->W = cb*ccpa;
		q->X = sb*ccma;
		q->Y = sb*scma;
		q->Z = cb*scpa;

		// below here temp. hacks for error measures
		if (q->W >= 0.f) return;
		quat_neg(q,q);
	}

public:
	static uint64_t fzxz(quat_t* q)
	{
		vec3_t v;
		quat_to_zxz(&v, q);

		v.X = v.X*RPI;
		v.Y = v.Y*RPI/2.f+.5f;
		v.Z = v.Z*RPI/2.f+.5f;

		uint64_t b;

		b  = quant(v.X, QSCALE(BITS_X));
		b |= quant(v.Y, QSCALE(BITS_Y)) << BITS_X;
		b |= quant(v.Z, QSCALE(BITS_Z)) << (BITS_Y+BITS_X);
  
		return b;
	}

	static void izxz(quat_t* q, uint64_t b)
	{
		vec3_t v;

		v.X = dequant(b & BITS_MX, DSCALE(BITS_X)); b >>= BITS_X;
		v.Y = dequant(b & BITS_MY, DSCALE(BITS_Y)); b >>= BITS_Y;
		v.Z = dequant(b & BITS_MZ, DSCALE(BITS_Z));

		v.X = PI*v.X;
		v.Y = 2.f*PI*(v.Y-0.5f);
		v.Z = 2.f*PI*(v.Z-0.5f);

		zxz_to_quat(q, &v);
	}
	//all of these throw-away methods need to be replaced by their actual versions
	//since i'm pretty sure jolt already uses all of them.
	static inline void vec3_set(vec3_t* v, float x, float y, float z)
	{
		v->X=x; v->Y=y; v->Z=z;
	}
	
	static inline void quat_put_bv(vec3_t* v, quat_t* q, float s)
	{
		vec3_set_scale(v, &(q), s);
	}  
	
	static inline void vec3_set_scale(vec3_t* a, vec3_t* b, float s)
	{
		vec3_set(a, s*BX, s*BY, s*BZ);
	}
	
	static inline void quat_bv_set_scale(quat_t* r, vec3_t* v, float s)
	{
		vec3_set_scale(&(r), v, s);
	}
	

	//we ended up not using this at the moment but I'm pretty sure there's a small bug in the treatment of the W
	//component in these functions. i'm not going to track it down atm because again, this isn't live code.
	// half-angle packing of a quat, no quantization.
	#define QQHalfAngleMACRO(vMACRO /*vector ref*/, qMACRO /*quat ref*/) 		float dMACRO = 1.f + qMACRO->W; float sMACRO = sqrtf(1.f/dMACRO); vMACRO->X=sMACRO*qMACRO->X; vMACRO->Y=sMACRO*qMACRO->Y; vMACRO->Z=sMACRO*qMACRO->Z;
	#define QQHalfAnglRValueMACRO(vMACRO /*vector ref*/, qMACRO /*quat*/) 	float dMACRO = 1.f + qMACRO.W; float sMACRO = sqrtf(1.f/dMACRO); vMACRO->X=sMACRO*qMACRO.X; vMACRO->Y=sMACRO*qMACRO.Y; vMACRO->Z=sMACRO*qMACRO.Z;
	
	static void InverseHalfAngle(quat_t* q, vec3_t* v)
	{
		float d = v->Dot(*v);
		float s = sqrtf(2.f-d); 
		q->X = s*v->X; q->Y = s*v->Y; q->Z = s*v->Z;
		q->W = 1.f-d;
	}
	static void InverseHalfAngle(quat_wt* q, vec3_t* v)
	{
		float d = v->Dot(*v);
		float s = sqrtf(2.f-d); 
		q->X = s*v->X; q->Y = s*v->Y; q->Z = s*v->Z;
		q->W = 1.f-d;
	}
#undef QSCALE_
#undef QSCALE
#undef DSCALE
#undef RPI
#undef AX
#undef AY
#undef AZ
#undef AW
#undef BX
#undef BY
#undef BZ
#undef BW
};
