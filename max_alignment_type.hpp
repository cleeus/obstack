#ifndef BOOST_MAXALIGNMENT_TYPE_HPP
#define BOOST_MAXALIGNMENT_TYPE_HPP

#include <boost/cstdint.hpp>

namespace boost {
namespace arena {
namespace detail {

template<typename T1, typename T2, bool T1gtT2>
struct max_sizeof_impl {
	typedef T1 type;
	enum { value = sizeof(type) };
};

//partial specialization on false
template<typename T1, typename T2>
struct max_sizeof_impl<T1,T2,false> {
	typedef T2 type;
	enum { value = sizeof(type) };
};

struct null_type {};

template<
	typename T1,
	typename T2 = null_type,
	typename T3 = null_type,
	typename T4 = null_type,
	typename T5 = null_type,
	typename T6 = null_type,
	typename T7 = null_type,
	typename T8 = null_type,
	typename T9 = null_type,
	typename T10 = null_type
>
struct max_sizeof {
private:
	typedef typename max_sizeof<T2,T3,T4,T5,T6,T7,T8,T9,T10>::type end_max_type;
	typedef max_sizeof<T1,end_max_type> total_max_sizeof;
	
	typedef typename total_max_sizeof::type max_type;
	enum { max_value = total_max_sizeof::value };
public:
	typedef max_type type;
	enum { value = max_value };
};


template<typename T1>
struct max_sizeof<
	T1,
	null_type,
	null_type,
	null_type,
	null_type,
	null_type,
	null_type,
	null_type,
	null_type,
	null_type
>
{
	typedef T1 type;
	enum { value = sizeof(type) };
};


template<typename T1, typename T2>
struct max_sizeof<
	T1,
	T2,
	null_type,
	null_type,
	null_type,
	null_type,
	null_type,
	null_type,
	null_type,
	null_type
>
{
private:
	typedef max_sizeof_impl<T1,T2, (sizeof(T1)>sizeof(T2)) > total_max_sizeof;
	typedef typename total_max_sizeof::type max_type;
	enum { max_value = total_max_sizeof::value };
public:
	typedef max_type type;
	enum { value = max_value };
};


} //namespace detail


typedef detail::max_sizeof<
	char,
	short,
	int,
	bool,
	long,
	//long long,
	::boost::int64_t,
	float,
	double,
	long double,
	void*
>::type max_align_t;

} //namespace arena
} //namespace boost

#endif /* BOOST_MAXALIGNMENT_TYPE_HPP */

