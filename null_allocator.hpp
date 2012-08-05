#ifndef BOOST_ARENA_NULL_ALLOCATOR_HPP
#define BOOST_ARENA_NULL_ALLOCATOR_HPP

#include <boost/limits.hpp>
#include <boost/utility.hpp>

namespace boost {
namespace arena {

template<typename T>
struct null_allocator {
	typedef T         value_type;
	typedef T*        pointer;
	typedef T&        reference;
	typedef const T*  const_pointer;
	typedef const T&  const_reference;
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;

	pointer        address(reference x) const { return &x; }
	const_pointer  address(const_reference x) const { return &x; }
	pointer        allocate(size_type n, const void *hint=0) { return NULL; }
	void           deallocate(pointer p, size_type n) {}
	size_type      max_size() const throw() { return std::numeric_limits<size_type>::max(); }
	void           construct(pointer p, const_reference val) { new((void*)p) T(val); }
	void           destroy(pointer p) { ((T*)p)->~T(); }
};

} //namespace arena
} //namespace boost

#endif //BOOST_ARENA_NULL_ALLOCATOR_HPP

