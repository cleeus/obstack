#ifndef _BOOST_OBSTACK_HPP
#define _BOOST_OBSTACK_HPP

#include <cstddef>
#include <memory>

#include <boost/type_traits/alignment_of.hpp>
#include <boost/type_traits/is_pod.hpp>
#include <boost/static_assert.hpp>

namespace boost {
namespace obstack {
namespace detail {
	template<class T>
	void call_dtor(void *p) {
		static_cast<T*>(p)->~T();
	}

	void free_marker_dtor(void*);
	void array_of_primitives_dtor(void*);

	//random cookie used to encrypt function pointers
	extern const void * const fptr_cookie;
}

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
 * |chunk_header  |chunk_header         |chunk_header
 * |  | object    |  | object |         |  | object |
 * ______________________________..._____________________..._
 * |  |           |  |        |         |  |        |       |
 * ------------------------------...---------------------...-
 * ^                                    ^           ^       ^
 * mem                                  top_chunk   tos     end_of_mem
 *
 *
 * TODO alignment
 * TODO support primitive types
 * TODO support arrays of complex types
 *
 * TODO use allocators to reserve raw memory
 * TODO support shared pointers from obstack
 * TODO support obstack nesting
 * TODO C++11 perfect forwarding constructors with refref and variadic templates
 * TODO implement an allocator on top of obstack
 * TODO check pointers in dealloc in DEBUG builds
 * TODO deal with exceptions in dealloc_all and the destructor
 */
class obstack {
public:
	typedef unsigned char byte_type;
	typedef std::size_t size_type;

private:
	//obstack is not copyable / assignable
	obstack(const obstack&);
	void operator=(const obstack&);
	
	typedef void (*dtor_fptr)(void*);

	struct chunk_header {
		chunk_header *prev;
		dtor_fptr dtor;
	};

	struct typed_void {};

public:
	/**
	 * \brief construct an obstack of a given capacity
	 *
	 * The capacity of an obstack is the number of bytes for later use.
	 * When reasoning about the required size, consider the overhead required
	 * for allocating each object on the obstack.
	 *
	 * \see obstack::overhead
	 */
	obstack(size_type capacity)
		: free_marker_dtor_xor(xor_fptr(&detail::free_marker_dtor)),
			array_of_primitives_dtor_xor(xor_fptr(&detail::array_of_primitives_dtor))
	{
		top_chunk = NULL;
		mem = new byte_type[capacity];
		tos = mem;
		end_of_mem = mem + capacity;
	}

	~obstack() {
		dealloc_all();
		delete[] mem;
	}

	/**
	 * \brief Allocate an object of type T on the obstack
	 */
	template<class T>
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


	template<typename T>
	T* alloc_array(size_type size) {
		BOOST_STATIC_ASSERT_MSG( is_pod<T>::value, "T must be a POD type.");
		const size_type array_bytes = sizeof(T)*size;
		if( mem_available(array_bytes) ) {
			allocate(array_bytes, array_of_primitives_dtor_xor);
			return reinterpret_cast<T*>(tos-array_bytes);
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
	bool is_top(const void *obj) const {
		const chunk_header * const chead = to_chunk_header(to_typed_void(obj));
		return chead == top_chunk;
	}

	/**
	 * \brief calculate the overhead in bytes required for allocating a number of elements
	 */
	static size_type overhead(size_type number_of_elements) {
		return sizeof(chunk_header) * number_of_elements;
	}

	///get the number of bytes that are already allocated
	size_type size() const { return static_cast<size_type>(tos-mem); }
	///get the number of bytes that are available in the obstack in total
	size_type capacity() const { return static_cast<size_type>(end_of_mem-mem); }

private:
	static typed_void * to_typed_void(void *obj) {
		return reinterpret_cast<typed_void*>(obj);
	}
	static const typed_void * to_typed_void(const void *obj) {
		return reinterpret_cast<const typed_void*>(obj);
	}
	
	static dtor_fptr xor_fptr(dtor_fptr const fptr) {
		return reinterpret_cast<dtor_fptr>(
			reinterpret_cast<size_t>(fptr) ^ reinterpret_cast<size_t>(detail::fptr_cookie)
		);
	}

	bool mem_available(size_type const size) const {
		return tos + sizeof(chunk_header) + size < end_of_mem;
	}

  template<typename T>
	bool mem_available() const {
		return mem_available(sizeof(T));
	}

	template<typename T>
	T* push() { allocate<T>(); return new(tos-sizeof(T)) T(); }
	template<typename T, typename T1>
	T* push(const T1 &a1) { allocate<T>(); return new(tos-sizeof(T)) T(a1); }
  template<typename T, typename T1>
	T* push(T1 &a1) { allocate<T>(); return new(tos-sizeof(T)) T(a1); }

  template<typename T, typename T1, typename T2>
	T* push(const T1 &a1, const T2 &a2) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2); }
  template<typename T, typename T1, typename T2>
	T* push(T1 &a1, const T2 &a2) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2); }
  template<typename T, typename T1, typename T2>
	T* push(const T1 &a1, T2 &a2) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2); }
  template<typename T, typename T1, typename T2>
	T* push(T1 &a1, T2 &a2) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2); }

  template<typename T, typename T1, typename T2, typename T3>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(T1 &a1, const T2 &a2, const T3 &a3) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(const T1 &a1, T2 &a2, const T3 &a3) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(const T1 &a1, const T2 &a2, T3 &a3) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(const T1 &a1, T2 &a2, T3 &a3) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(T1 &a1, const T2 &a2, T3 &a3) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(T1 &a1, T2 &a2, const T3 &a3) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2, a3); }
  template<typename T, typename T1, typename T2, typename T3>
	T* push(T1 &a1, T2 &a2, T3 &a3) { allocate<T>(); return new(tos-sizeof(T)) T(a1, a2, a3); }


  template<typename T, typename T1, typename T2, typename T3, typename T4>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4) {
		allocate<T>();
		return new(tos-sizeof(T)) T(a1, a2, a3, a4);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5) {
		allocate<T>();
		return new(tos-sizeof(T)) T(a1, a2, a3, a4, a5);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6) {
		allocate<T>();
		return new(tos-sizeof(T)) T(a1, a2, a3, a4, a5, a6);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7) {
		allocate<T>();
		return new(tos-sizeof(T)) T(a1, a2, a3, a4, a5, a6, a7);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7, const T8 &a8) {
		allocate<T>();
		return new(tos-sizeof(T)) T(a1, a2, a3, a4, a5, a6, a7, a8);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7, const T8 &a8, const T9 &a9) {
		allocate<T>();
		return new(tos-sizeof(T)) T(a1, a2, a3, a4, a5, a6, a7, a8, a9);
	}
  template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
	T* push(const T1 &a1, const T2 &a2, const T3 &a3, const T4 &a4, const T5 &a5, const T6 &a6, const T7 &a7, const T8 &a8, const T9 &a9, const T10 &a10) {
		allocate<T>();
		return new(tos-sizeof(T)) T(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
	}
 



	void allocate(size_type const size, dtor_fptr const xored_dtor) {
		chunk_header * const chead = reinterpret_cast<chunk_header*>(tos);
		chead->prev = top_chunk;
		chead->dtor = xored_dtor;
		top_chunk = chead;
		// allocate memory
		tos += sizeof(chunk_header) + size;
	}
	
	template<typename T>
	void allocate() {
		allocate(sizeof(T), xor_fptr(&detail::call_dtor<T>));
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
		return reinterpret_cast<chunk_header*>(reinterpret_cast<byte_type*>(obj) - sizeof(chunk_header));
	}
	static const chunk_header *to_chunk_header(const typed_void * const obj) {
		return reinterpret_cast<const chunk_header*>(reinterpret_cast<const byte_type*>(obj) - sizeof(chunk_header));
	}
	static typed_void *to_object(chunk_header * const chead) {
		return to_typed_void(reinterpret_cast<byte_type*>(chead)+sizeof(chunk_header));
	}


	dtor_fptr mark_as_destructed(chunk_header * const chead) const {
		dtor_fptr const dtor = xor_fptr(chead->dtor);
		chead->dtor = free_marker_dtor_xor;
		return dtor;
	}


	/**
	 * \brief rewind tos and top_chunk pointers as far as possible
	 *
	 * complexity: O(k) where k is the number of consecutive destructed chunks
	 */
	void deallocate_as_possible() {
		while(top_chunk && (top_chunk->dtor == free_marker_dtor_xor)) {
			//deallocate memory
			tos = reinterpret_cast<byte_type*>(top_chunk);
			top_chunk = top_chunk->prev;
		}
	}



private:
	//encrypted address of not_a_dtor, used to mark dtor as called/invalid
	const dtor_fptr free_marker_dtor_xor;
	const dtor_fptr array_of_primitives_dtor_xor;

	//top of stack pointer
	byte_type *tos;
	//points to the chunk_header before the current tos
	chunk_header *top_chunk;
	//reserved memory
	byte_type *mem;
	//end of reserved memory region
	byte_type *end_of_mem;
};

} //namespace obstack
} //namespace boost

#endif //_BOOST_OBSTACK_HPP
