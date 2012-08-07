#ifndef BOOST_OBSTACK_HPP
#define BOOST_OBSTACK_HPP

#include <cstddef>
#include <memory>

#include <boost/utility.hpp>
#include <boost/type_traits/alignment_of.hpp>
#include <boost/type_traits/is_pod.hpp>
#include <boost/static_assert.hpp>

#include "obstack_fwd.hpp"
#include "max_alignment_type.hpp"

namespace boost {
namespace arena {
namespace arena_detail {

template<class T>
void call_dtor(void *p) {
	static_cast<T*>(p)->~T();
}

void free_marker_dtor(void*);
void array_of_primitives_dtor(void*);

typedef void (*dtor_fptr)(void*);
extern dtor_fptr const free_marker_dtor_xor;
extern dtor_fptr const array_of_primitives_dtor_xor;
extern size_t const checksum_cookie;


struct ptr_sec {
private:
	///a random cookie used to "checksum" pointers
	static size_t const _checksum_cookie;
	///a random cookie used to encrypt function pointers
	static void* const _xor_cookie;
	///a value that is guaranteed to be occupied in the address space and cannot be in the heap nor the stack
	static void* const _invalid_addr;

public:
	static void* invalid_addr() { return _invalid_addr; }
	static void* invalid_addr_xor() { return xor_ptr(_invalid_addr); }

	template<typename T>
	static T* xor_ptr(T* const p) {
		return reinterpret_cast<T*>(
			reinterpret_cast<size_t>(p) ^ reinterpret_cast<size_t>(_xor_cookie)
		);
	}
	template<typename T>
	static const T* xor_ptr(const T* const p) {
		return reinterpret_cast<const T*>(
			reinterpret_cast<size_t>(p) ^ reinterpret_cast<size_t>(_xor_cookie)
		);
	}

	static size_t make_checksum(const void * const prev, dtor_fptr const xored_dtor) {
		return
			reinterpret_cast<size_t>(xored_dtor) ^ 
			reinterpret_cast<size_t>(prev) ^
			_checksum_cookie;
	}
	static bool checksum_ok(const void * const prev, dtor_fptr const xored_dtor, size_t const checksum) {
		return checksum == make_checksum(prev, xored_dtor);
	}

};

template<class A>
struct memory_holder {
	typedef A allocator_type;
	typedef typename allocator_type::pointer pointer;
	typedef typename allocator_type::size_type size_type;
	typedef typename allocator_type::value_type value_type;

	memory_holder(pointer mem, size_type capacity, allocator_type const &a) :
		allocator(a),
		memory(mem),
		memory_count(capacity)
	{}

	memory_holder(size_type const capacity, allocator_type const &a) :
		allocator(a),
		memory(allocator.allocate(capacity)),
		memory_count(capacity)
	{
		BOOST_ASSERT_MSG(memory != NULL, "allocate failed");
	}

	~memory_holder() {
		if(memory) {
			allocator.deallocate(memory, memory_count);
		}
	}

	pointer mem() const { return memory; }
	pointer end_of_mem() const { return memory+memory_count; }
	size_type capacity() const { return memory_count; }

private:
	allocator_type allocator;
	pointer const memory;
	size_type const memory_count;
};


template<typename A>
struct octet_holder {
	typedef A allocator_type;
	typedef memory_holder<allocator_type> memory_holder_type;
	typedef unsigned char byte_type;
	typedef typename std::allocator<byte_type>::size_type size_type;
	typedef typename std::allocator<byte_type>::pointer pointer;

	typedef typename allocator_type::size_type alloc_size_type;
	typedef typename allocator_type::value_type alloc_value_type;
	typedef typename allocator_type::pointer alloc_pointer;

	BOOST_STATIC_ASSERT_MSG(
		alignment_of<alloc_value_type>::value == alignment_of<max_align_t>::value,
		"the allocator and memory must be of max_align_t type"
	);

	octet_holder(alloc_pointer mem, alloc_size_type const capacity, allocator_type const &a) :
		mem_holder(mem, capacity, a)
	{
		BOOST_ASSERT_MSG( is_aligned(mem), "memory alignment error");
	}

	octet_holder(size_type const capacity_in_bytes, allocator_type const &a)
		: mem_holder(to_alloc_capacity(capacity_in_bytes), a)
	{}


	pointer mem() const { return to_byte_ptr(mem_holder.mem()); }
	pointer end_of_mem() const { return to_byte_ptr(mem_holder.end_of_mem()); }
	size_type capacity() const { return mem_holder.capacity() * sizeof(alloc_value_type); }
private:
	static alloc_size_type to_alloc_capacity(size_type const capacity_in_bytes) {
		const alloc_size_type num_elements =
			capacity_in_bytes / sizeof(alloc_value_type) + 
			((capacity_in_bytes % sizeof(alloc_value_type)) ? 1 : 0);
		return num_elements;
	}

	static pointer to_byte_ptr(alloc_pointer const p) {
		return reinterpret_cast<pointer>(p);
	}

	static bool is_aligned(void *p) {
		return reinterpret_cast<size_t>(p) % alignment_of<max_align_t>::value == 0;
	}

	memory_holder_type mem_holder;
};


} //namespace arena_detail

/**
 * \class obstack
 * \brief An object stack O(1) memory arena implementation
 * \author Kai Dietrich <mail@cleeus.de>
 *
 * A obstack is an implementation of a stack-like memory allocation strategy:
 * pointer bumping. Upon construction a contigous area of memory is allocated.
 * A pointer points into the memory. Everything behind the pointer is allocated
 * memory, everything in front is free. Allocating a new object on this virtual stack
 * means increasing the pointer, freeing means decreasing it.
 *
 * The result is an allocation strategy where all operations are O(1).
 * Arbitrary size objects can be allocated but memory can only be freed in the
 * reverse order it has been allocated.
 *
 * This implementation provides an additional free operation:
 * Free-requests that are out-of-order will destroy the object under the pointer
 * (call the destructor function) but not actually free the memory.
 * Only when all objects in front of the object
 * are also freed, it's memory will become available.
 * To allow this operation function pointers to the destructors will be placed
 * on the obstack. For security reasons, these function pointers are encrypted
 * with a random cookie which is initialized on startup.
 *
 *
 * The memory layout looks like this:
 *
 *               |padding       |padding       |padding
 * |chunk_header ||chunk_header ||chunk_header ||chunk_header 
 * |  | object   ||  | object   ||  | object   ||  | object   |
 * ____________________________________________________________..._____
 * |  |          ||  |          ||  |          ||  |          |       |
 * ------------------------------------------------------------...-----
 * ^                                            ^             ^       ^
 * mem                                          top_chunk     tos     end_of_mem
 *
 *
 * TODO support arrays of complex types
 * TODO support array Ts in normal alloc
 *
 * TODO support shared pointers from obstack
 * TODO support explicit obstack nesting
 * TODO C++11 perfect forwarding constructors with refref and variadic templates
 * TODO implement an allocator on top of obstack
 * TODO deal with exceptions in dealloc_all and the destructor
 */
template<class A>
class basic_obstack
	: private noncopyable
{
private:
	typedef arena_detail::octet_holder<A> holder_type;
public:
	typedef A allocator_type;
	typedef typename holder_type::size_type size_type;
	typedef typename holder_type::byte_type byte_type;

private:
	typedef arena_detail::dtor_fptr dtor_fptr;

	struct chunk_header {
		chunk_header *prev;
		dtor_fptr dtor;
		size_type checksum;
	};

	template<typename T>
	struct max_aligned_sizeof {
		enum {
			value = sizeof(T) % alignment_of<max_align_t>::value ?
							sizeof(T) + (alignment_of<max_align_t>::value - sizeof(T)%alignment_of<max_align_t>::value)
							: sizeof(T)
		};
	};

	template<typename T>
	struct alignment {
		enum { value =
			alignment_of<T>::value > alignment_of<chunk_header>::value ?
			alignment_of<T>::value : alignment_of<chunk_header>::value
		};
	};

	struct typed_void {};

public:
	/**
	 * \brief construct an obstack of a given capacity on the heap
	 *
	 * Uses the allocator to acquire memory.
	 * The capacity of an obstack is the number of bytes for later use.
	 * When reasoning about the required size, consider the overhead required
	 * for allocating each object on the obstack which consists of the size
	 * of the chunk_header and padding to the global alignment
	 *
	 */
	explicit basic_obstack(size_type const capacity, const allocator_type &a = allocator_type()) :
		top_chunk(NULL),
		memory(capacity, a)
	{
		BOOST_ASSERT_MSG(capacity, "obstack with capacity of 0 requested");
		BOOST_ASSERT_MSG(memory.mem(), "global_malloc_allocator returned NULL");
		tos = memory.mem();
	}

	/**
	 * \brief construct an obstack on the given memory buffer
	 *
	 * buffer must be a block of memory at least the size of the
	 * alignment. The obstack will free the memory using the supplied allocator.
	 */
	basic_obstack(max_align_t *buffer, size_type const buffer_size, const allocator_type &a) :
		top_chunk(NULL),
		memory(
			buffer && buffer_size ? buffer : NULL,
			buffer_size,
			a)
	{
		BOOST_ASSERT_MSG(buffer, "supplied buffer is NULL");
		BOOST_ASSERT_MSG(buffer_size, "supplied buffer_size is 0");
		tos = memory.mem();
	}

	~basic_obstack() {
		dealloc_all();
	}

	/**
	 * \brief Allocate an object of type T on the obstack
	 *
	 * Valid types for T are:
	 *	- integral types like char, int, etc.
	 *	- pointer types
	 *  - complex types like structs and classes
	 * Invalid types for T are:
	 *  - arrays like char[42]
	 *
	 * alloc is templated and overloaded for up to 10 arguments.
	 * These are passed as arguments to the constructor of T.
	 * For up to 3 arguments, the constructors are perfect forwarding.
	 * With more than 3 arguments, only const arguments are supported.
	 */
	template<typename T>
	T* alloc() { return mem_available<T>() ? push<T>() : NULL; }
	template<typename T, typename T1>
	T* alloc(const T1 &a1) { return mem_available<T>() ? push<T>(a1) : NULL; }
	template<typename T, typename T1>
	T* alloc(T1 &a1) { return mem_available<T>() ? push<T>(a1) : NULL; }

	template<typename T, typename T1, typename T2>
	T* alloc(const T1 &a1, const T2 &a2) { return mem_available<T>() ? push<T>(a1, a2) : NULL; }
	template<typename T, typename T1, typename T2>
	T* alloc(T1 &a1, const T2 &a2) { return mem_available<T>() ? push<T>(a1, a2) : NULL; }
	template<typename T, typename T1, typename T2>
	T* alloc(const T1 &a1, T2 &a2) { return mem_available<T>() ? push<T>(a1, a2) : NULL; }
	template<typename T, typename T1, typename T2>
	T* alloc(T1 &a1, T2 &a2) { return mem_available<T>() ? push<T>(a1, a2) : NULL; }

	template<typename T, typename T1, typename T2, typename T3>
	T* alloc(const T1 &a1, const T2 &a2, const T3 &a3) { return mem_available<T>() ? push<T>(a1, a2, a3) : NULL; }
	template<typename T, typename T1, typename T2, typename T3>
	T* alloc(T1 &a1, const T2 &a2, const T3 &a3) { return mem_available<T>() ? push<T>(a1, a2, a3) : NULL; }
	template<typename T, typename T1, typename T2, typename T3>
	T* alloc(const T1 &a1, T2 &a2, const T3 &a3) { return mem_available<T>() ? push<T>(a1, a2, a3) : NULL; }
	template<typename T, typename T1, typename T2, typename T3>
	T* alloc(const T1 &a1, const T2 &a2, T3 &a3) { return mem_available<T>() ? push<T>(a1, a2, a3) : NULL; }
	template<typename T, typename T1, typename T2, typename T3>
	T* alloc(const T1 &a1, T2 &a2, T3 &a3) { return mem_available<T>() ? push<T>(a1, a2, a3) : NULL; }
	template<typename T, typename T1, typename T2, typename T3>
	T* alloc(T1 &a1, const T2 &a2, T3 &a3) { return mem_available<T>() ? push<T>(a1, a2, a3) : NULL; }
	template<typename T, typename T1, typename T2, typename T3>
	T* alloc(T1 &a1, T2 &a2, const T3 &a3) { return mem_available<T>() ? push<T>(a1, a2, a3) : NULL; }
	template<typename T, typename T1, typename T2, typename T3>
	T* alloc(T1 &a1, T2 &a2, T3 &a3) { return mem_available<T>() ? push<T>(a1, a2, a3) : NULL; }

	//with more then 3 arguments, binomial explosion really sets in, so we can just support const
	
	template<typename T, typename T1, typename T2, typename T3, typename T4>
	T* alloc(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4) {
		return mem_available<T>() ? push<T>(a1, a2, a3, a4) : NULL;
	}
	template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>
	T* alloc(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5) {
		return mem_available<T>() ? push<T>(a1, a2, a3, a4, a5) : NULL;
	}
	template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
	T* alloc(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6) {
		return mem_available<T>() ? push<T>(a1, a2, a3, a4, a5, a6) : NULL;
	}
	template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
	T* alloc(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7) {
		return mem_available<T>() ? push<T>(a1, a2, a3, a4, a5, a6, a7) : NULL;
	}
	template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
	T* alloc(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7, const T8 &a8) {
		return mem_available<T>() ? push<T>(a1, a2, a3, a4, a5, a6, a7, a8) : NULL;
	}
	template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
	T* alloc(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7, const T8 &a8, const T9 &a9) {
		return mem_available<T>() ? push<T>(a1, a2, a3, a4, a5, a6, a7, a8, a9) : NULL;
	}
	template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
	T* alloc(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7, const T8 &a8, const T9 &a9, const T10 &a10) {
		return mem_available<T>() ? push<T>(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) : NULL;
	}


	/**
	 * \brief Allocate a linear packed array of elements.
	 *
	 * T must be a POD type since there is no cunstructor called.
	 * There will be no padding between the elements of the array.
	 */
	template<typename T>
	T* alloc_array(size_type num_elements) {
		BOOST_STATIC_ASSERT_MSG( is_pod<T>::value, "T must be a POD type.");
		const size_type array_bytes = sizeof(T)*num_elements;
		const size_type align_to = alignment<T>::value; 
		if( mem_available<T>(num_elements) ) {
			allocate(align_to, array_bytes, arena_detail::array_of_primitives_dtor_xor);
			return reinterpret_cast<T*>(top_object());
		} else {
			return NULL;
		}
	}

	/**
	 * \brief destruct an object on the obstack and reclaim memory if possible
	 *
	 * When the given object is on the top of the stack,
	 * it will be destructed and its memory will be available emediatly.
	 * Otherwise it will only be destructed but its memory will be blocked
	 * by objects in front of it.
	 *
	 * When the given obj pointer is not a valid object on this obstack
	 * the behaviour is undefined. In most cases it will result in a crash.
	 */
	void dealloc(void * const obj) {
		if(obj) {
			typed_void * const typed_obj = to_typed_void(obj);
			if(is_top(typed_obj)) {
				pop(typed_obj);
			} else {
				destruct(typed_obj);
			}
		}
	}

	/**
	 * \brief destruct and reclaim memory of all objects on the obstack
	 */
	void dealloc_all() {
		while(top_chunk) {
			pop(top_chunk);
		}
	}

	/**
	 * \brief check whether a given object is on the top of the obstack
	 */
	bool is_top(void * const obj) const {
		const chunk_header * const chead = to_chunk_header(to_typed_void(obj));
		return chead == top_chunk;
	}

	/**
	 * \brief check if a given pointer is inside this arena and is a valid pointer to an object.
	 */
	bool is_valid(const void * const obj) const {
		return is_valid(to_chunk_header(obj));
	}

	/**
	 * \brief calculate the maximum possible overhead in bytes for allocating a number of elements
	 *
	 * Overhead may be less, depending on the alignment and the allocated types, but never more
	 */
	static size_type max_overhead(const size_type num_elements) {
		const size_type max_alignment = alignment<max_align_t>::value;
		return (max_aligned_sizeof<chunk_header>::value+max_alignment)*num_elements;
	}

	///get the number of bytes that are already allocated
	size_type size() const { return static_cast<size_type>(tos-memory.mem()); }
	///get the number of bytes that are available in the obstack in total
	size_type capacity() const { return memory.capacity(); }

private:
	static typed_void * to_typed_void(void *obj) {
		return reinterpret_cast<typed_void*>(obj);
	}
	static const typed_void * to_typed_void(const void *obj) {
		return reinterpret_cast<const typed_void*>(obj);
	}
	
	static dtor_fptr xor_fptr(dtor_fptr const fptr) {
		return arena_detail::ptr_sec::xor_ptr(fptr);
	}

	/**
	 * \brief calculate the required padding bytes to the next fully aligned pointer
	 */
	static size_type offset_to_alignment(const void * const p, const size_type align_to) {
		const size_type address = reinterpret_cast<size_type>(p);
		return address % align_to ? (align_to - address%align_to): 0;
	}

  template<typename T>
	bool mem_available() const {
		const size_type padding = offset_to_alignment(tos,alignment<T>::value);
		const bool is_available = (tos + padding + max_aligned_sizeof<chunk_header>::value + sizeof(T)) < memory.end_of_mem();
		return is_available;
	}
  template<typename T>
	bool mem_available(const size_type num_elements) const {
		const size_type padding = offset_to_alignment(tos,alignment<T>::value);
		return tos + padding + max_aligned_sizeof<chunk_header>::value + sizeof(T)*num_elements < memory.end_of_mem();
	}

	byte_type* top_object() const {
		return reinterpret_cast<byte_type*>(top_chunk) + max_aligned_sizeof<chunk_header>::value;
	}

	template<typename T>
	T* push() { allocate<T>(); return new(top_object()) T(); }
	template<typename T, typename T1>
	T* push(const T1 &a1) { allocate<T>(); return new(top_object()) T(a1); }
  template<typename T, typename T1>
	T* push(T1 &a1) { allocate<T>(); return new(top_object()) T(a1); }

  template<typename T, typename T1, typename T2>
	T* push(const T1 &a1, const T2 &a2) { allocate<T>(); return new(top_object()) T(a1, a2); }
  template<typename T, typename T1, typename T2>
	T* push(T1 &a1, const T2 &a2) { allocate<T>(); return new(top_object()) T(a1, a2); }
  template<typename T, typename T1, typename T2>
	T* push(const T1 &a1, T2 &a2) { allocate<T>(); return new(top_object()) T(a1, a2); }
  template<typename T, typename T1, typename T2>
	T* push(T1 &a1, T2 &a2) { allocate<T>(); return new(top_object()) T(a1, a2); }

  template<typename T, typename T1, typename T2, typename T3>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3) { allocate<T>(); return new(top_object()) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(T1 &a1, const T2 &a2, const T3 &a3) { allocate<T>(); return new(top_object()) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(const T1 &a1, T2 &a2, const T3 &a3) { allocate<T>(); return new(top_object()) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(const T1 &a1, const T2 &a2, T3 &a3) { allocate<T>(); return new(top_object()) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(const T1 &a1, T2 &a2, T3 &a3) { allocate<T>(); return new(top_object()) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(T1 &a1, const T2 &a2, T3 &a3) { allocate<T>(); return new(top_object()) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(T1 &a1, T2 &a2, const T3 &a3) { allocate<T>(); return new(top_object()) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(T1 &a1, T2 &a2, T3 &a3) { allocate<T>(); return new(top_object()) T(a1, a2, a3); }


  template<typename T, typename T1, typename T2, typename T3, typename T4>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4) {
		allocate<T>();
		return new(top_object()) T(a1, a2, a3, a4);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5) {
		allocate<T>();
		return new(top_object()) T(a1, a2, a3, a4, a5);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6) {
		allocate<T>();
		return new(top_object()) T(a1, a2, a3, a4, a5, a6);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7) {
		allocate<T>();
		return new(top_object()) T(a1, a2, a3, a4, a5, a6, a7);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7, const T8 &a8) {
		allocate<T>();
		return new(top_object()) T(a1, a2, a3, a4, a5, a6, a7, a8);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7, const T8 &a8, const T9 &a9) {
		allocate<T>();
		return new(top_object()) T(a1, a2, a3, a4, a5, a6, a7, a8, a9);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7, const T8 &a8, const T9 &a9, const T10 &a10) {
		allocate<T>();
		return new(top_object()) T(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
	}
 
	bool is_valid(const chunk_header * const chead) const {
		bool const is_inside_arena =
			reinterpret_cast<const byte_type*>(chead) >= memory.mem() &&
			reinterpret_cast<const byte_type*>(chead) <= memory.end_of_mem();
		return is_inside_arena && arena_detail::ptr_sec::checksum_ok(chead->prev, chead->dtor, chead->checksum);
	}

	void allocate(size_type const align_to, size_type const size, dtor_fptr const xored_dtor) {
		tos += offset_to_alignment(tos, align_to);
		chunk_header * const chead = reinterpret_cast<chunk_header*>(tos);
		chead->prev = top_chunk;
		chead->dtor = xored_dtor;
		chead->checksum = arena_detail::ptr_sec::make_checksum(chead->prev, chead->dtor);
		top_chunk = chead;
		// allocate memory
		tos += max_aligned_sizeof<chunk_header>::value + size;
	}

	template<typename T>
	void allocate() {
		allocate(alignment<T>::value, sizeof(T), xor_fptr(&arena_detail::call_dtor<T>));
	}
	
	
	void pop(chunk_header * const chead) {
		typed_void * const obj = to_object(chead);
		pop(chead, obj);
	}

	void pop(typed_void * const obj) {
		chunk_header * const chead = to_chunk_header(obj);
		pop(chead, obj);
	}

	void pop(chunk_header * const chead, typed_void * const obj) {
		dtor_fptr dtor = mark_as_destructed(chead);	
		deallocate_as_possible();
		//might throw
		dtor(obj);
	}

	void destruct(typed_void *obj) {
		chunk_header * const chead = to_chunk_header(obj);
		destruct(chead, obj);
	}

	void destruct(chunk_header * const chead) {
		typed_void * const obj = to_object(chead);
		destruct(chead, obj);
	}

	void destruct(chunk_header * const chead, typed_void * const obj) {
		dtor_fptr dtor = mark_as_destructed(chead);
		//might throw
		dtor(obj);
	}

	static chunk_header *to_chunk_header(typed_void * const obj) {
		return reinterpret_cast<chunk_header*>(reinterpret_cast<byte_type*>(obj) - max_aligned_sizeof<chunk_header>::value);
	}
	static const chunk_header *to_chunk_header(const typed_void * const obj) {
		return reinterpret_cast<const chunk_header*>(reinterpret_cast<const byte_type*>(obj) - max_aligned_sizeof<chunk_header>::value);
	}
	static typed_void *to_object(chunk_header * const chead) {
		return to_typed_void(reinterpret_cast<byte_type*>(chead)+max_aligned_sizeof<chunk_header>::value);
	}

	/**
	 * \brief mark an item on the obstack as free and decrypt the dtor pointer
	 */
	dtor_fptr mark_as_destructed(chunk_header * const chead) const {
		BOOST_ASSERT_MSG(is_valid(chead), "invalid destruction detected");
		if(is_valid(chead)) {
			dtor_fptr const dtor = xor_fptr(chead->dtor);
			chead->dtor = arena_detail::free_marker_dtor_xor;
			return dtor;
		} else {
			return NULL;
		}
	}


	/**
	 * \brief rewind tos and top_chunk pointers as far as possible
	 *
	 * complexity: O(k) where k is the number of consecutive destructed chunks
	 */
	void deallocate_as_possible() {
		while(top_chunk && (top_chunk->dtor == arena_detail::free_marker_dtor_xor)) {
			//deallocate memory
			tos = reinterpret_cast<byte_type*>(top_chunk);
			top_chunk = top_chunk->prev;
		}
	}

private:
	///points to the chunk_header before the current tos
	chunk_header* top_chunk;
	///top of stack pointer
	byte_type* tos;
	//assures deallocation of memory upon destruction
	//detail::memory_guard mem_guard;
	holder_type memory;
};



} //namespace arena
} //namespace boost

#endif //BOOST_OBSTACK_HPP
