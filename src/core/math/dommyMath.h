#ifndef DOMMYMATH_H
#define DOMMYMATH_H

#include <algorithm>
#include <climits>
#include <cmath>
#include <ostream>
#include <vector>

#include <maya/MVector.h>
#include <maya/MPoint.h>

namespace dommy {
namespace math {

double degreesToRadians(double degrees);
double radiansToDegrees(double radians);

void aimY(const MVector& sourceVector, double& xRotation, double& zRotation);

double distance(const MVector& first, const MVector& second);
double distance(const MPoint& first, const MPoint& second);

// Returns the first point on the finite segment that lies inside the inclusive
// axis-aligned box. Reversed box bounds are accepted. The output is unchanged
// when the inputs are non-finite or the segment misses the box.
bool intersectSegmentAabb(const MPoint& firstBound,
                          const MPoint& secondBound,
                          const MVector& segmentStart,
                          const MVector& segmentEnd,
                          MVector& intersection);


template <class T>
inline void orderBounds(T& minimum, T& maximum)
{
	if (maximum < minimum) {
		std::swap(minimum, maximum);
	}
}


inline bool isInsideUnitCircle(double x, double y)
{
	return x * x + y * y < 1.0;
}


template<class T>
inline T sqr(const T &x)
{ return x*x; }

template<class T>
inline T min(T a1, T a2, T a3)
{ return std::min(a1, std::min(a2, a3)); }

template<class T>
inline T min(T a1, T a2, T a3, T a4)
{ return std::min(std::min(a1, a2), std::min(a3, a4)); }

template<class T>
inline T min(T a1, T a2, T a3, T a4, T a5)
{ return min(std::min(a1, a2), std::min(a3, a4), a5); }

template<class T>
inline T min(T a1, T a2, T a3, T a4, T a5, T a6)
{ return min(std::min(a1, a2), std::min(a3, a4), std::min(a5, a6)); }

template<class T>
inline T max(T a1, T a2, T a3)
{ return std::max(a1, std::max(a2, a3)); }

template<class T>
inline T max(T a1, T a2, T a3, T a4)
{ return std::max(std::max(a1, a2), std::max(a3, a4)); }

template<class T>
inline T max(T a1, T a2, T a3, T a4, T a5)
{ return max(std::max(a1, a2), std::max(a3, a4), a5); }

template<class T>
inline T max(T a1, T a2, T a3, T a4, T a5, T a6)
{ return max(std::max(a1, a2), std::max(a3, a4), std::max(a5, a6)); }

template<class T>
inline void minmax(T a1, T a2, T &amin, T &amax)
{
	if(a1<a2){
		amin=a1;
		amax=a2;
	}else{
		amin=a2;
		amax=a1;
	}
}

template<class T>
inline void minmax(T a1, T a2, T a3, T &amin, T &amax)
{
	if(a1<a2){
		if(a1<a3){
			amin=a1;
			if(a2<a3) amax=a3;
			else amax=a2;
		}else{
			amin=a3;
			if(a1<a2) amax=a2;
			else amax=a1;
		}
	}else{
		if(a2<a3){
			amin=a2;
			if(a1<a3) amax=a3;
			else amax=a1;
		}else{
			amin=a3;
			amax=a1;
		}
	}
}

template<class T>
inline void minmax(T a1, T a2, T a3, T a4, T &amin, T &amax)
{
	if(a1<a2){
		if(a3<a4){
			amin=std::min(a1,a3);
			amax=std::max(a2,a4);
		}else{
			amin=std::min(a1,a4);
			amax=std::max(a2,a3);
		}
	}else{
		if(a3<a4){
			amin=std::min(a2,a3);
			amax=std::max(a1,a4);
		}else{
			amin=std::min(a2,a4);
			amax=std::max(a1,a3);
		}
	}
}

template<class T>
inline void minmax(T a1, T a2, T a3, T a4, T a5, T &amin, T &amax)
{
	//@@@ the logic could be shortcircuited a lot!
	amin=min(a1,a2,a3,a4,a5);
	amax=max(a1,a2,a3,a4,a5);
}

template<class T>
inline void minmax(T a1, T a2, T a3, T a4, T a5, T a6, T &amin, T &amax)
{
	//@@@ the logic could be shortcircuited a lot!
	amin=min(a1,a2,a3,a4,a5,a6);
	amax=max(a1,a2,a3,a4,a5,a6);
}

template<class T>
inline T clamp(T a, T lower, T upper)
{
	if(a<lower) return lower;
	else if(a>upper) return upper;
	else return a;
}

inline unsigned int round_up_to_power_of_two(unsigned int n)
{
	if(n == 0) return 0;
	--n;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1; // 0 indicates that the result overflowed unsigned int.
}

inline unsigned int round_down_to_power_of_two(unsigned int n)
{
	if(n == 0) return 0;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n - (n >> 1);
}

// transforms even the sequence 0,1,2,3,... into reasonably good random numbers
// challenge: improve on this in speed and "randomness"!
inline unsigned int randhash(unsigned int seed)
{
	unsigned int i=(seed^12345391u)*2654435769u;
	i^=(i<<6)^(i>>26);
	i*=2654435769u;
	i+=(i<<5)^(i>>12);
	return i;
}

inline int intlog2(int x)
{
	if(x <= 0) return -1;
	unsigned int value = static_cast<unsigned int>(x);
	int exp=-1;
	while(value){
		value>>=1;
		++exp;
	}
	return exp;
}

template<class T>
void zero(std::vector<T> &v)
{ for(int i=(int)v.size()-1; i>=0; --i) v[i]=0; }

template<class T>
T abs_max(const std::vector<T> &v)
{
	T m=0;
	for(int i=(int)v.size()-1; i>=0; --i){
		if(std::fabs(v[i])>m)
			m=std::fabs(v[i]);
	}
	return m;
}

template<class T>
bool contains(const std::vector<T> &a, T e)
{
	for(unsigned int i=0; i<a.size(); ++i)
		if(a[i]==e) return true;
	return false;
}

template<class T>
void add_unique(std::vector<T> &a, T e)
{
	for(unsigned int i=0; i<a.size(); ++i)
		if(a[i]==e) return;
	a.push_back(e);
}

template<class T>
void insert(std::vector<T> &a, unsigned int index, T e)
{
	if(index > a.size()) return;
	a.insert(a.begin()+index, e);
}

template<class T>
void erase(std::vector<T> &a, unsigned int index)
{
	if(index >= a.size()) return;
	a.erase(a.begin()+index);
}

template<class T>
void erase_swap(std::vector<T> &a, unsigned int index)
{
	if(index >= a.size()) return;
	for(unsigned int i=index; i<a.size()-1; ++i)
		std::swap(a[i], a[i+1]);
	a.pop_back();
}

template<class T>
void erase_unordered(std::vector<T> &a, unsigned int index)
{
	if(index >= a.size()) return;
	a[index]=a.back();
	a.pop_back();
}

template<class T>
void erase_unordered_swap(std::vector<T> &a, unsigned int index)
{
	if(index >= a.size()) return;
	std::swap(a[index], a.back());
	a.pop_back();
}

template<class T>
void find_and_erase_unordered(std::vector<T> &a, const T &doomed_element)
{
	for(unsigned int i=0; i<a.size(); ++i)
		if(a[i]==doomed_element){
			erase_unordered(a, i);
			return;
		}
}

template<class T>
void replace_once(std::vector<T> &a, const T &old_element, const T &new_element)
{
	for(unsigned int i=0; i<a.size(); ++i)
		if(a[i]==old_element){
			a[i]=new_element;
			return;
		}
}

template<class T>
void write_matlab(std::ostream &output, const std::vector<T> &a, const char *variable_name, bool column_vector=true)
{
	output<<variable_name<<"=[";
	for(unsigned int i=0; i<a.size(); ++i){
		output<<a[i]<<" ";
	}
	output<<"]";
	if(column_vector)
		output<<"'";
	output<<";"<<std::endl;
}

} // namespace math
} // namespace dommy

#endif
