#ifndef _BOOST_OBSTACK_HPP
#define _BOOST_OBSTACK_HPP

#include <cstddef>
#include <memory>

namespace boost {
namespace obstack {
namespace detail {
	template<class T>
	void call_dtor(void *p) {
		static_cast<T*>(p)->~T();
	}

	void not_a_dtor(void*);

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
 * TODO use allocators to reserve raw memory
 * TODO support shared pointers from obstack
 * TODO support arrays on obstack
 * TODO perfect forwarding constructors with refref and variadic templates
 * TODO implement an allocator on top of obstack
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
		chunk_header *prior;
		size_type object_size;
		dtor_fptr dtor;
	};

	struct typed_void {};

public:
	obstack(size_type size)
		: not_a_dtor(crypt_fptr(&detail::not_a_dtor))
	{
		prior = NULL;
		mem = new byte_type[size];
		tos = mem;
		end_of_mem = mem + size;
	}

	~obstack() {
		dealloc_all();
		delete[] mem;
	}

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











	void dealloc(void * const obj) {
		typed_void * const typed_obj = to_typed_void(obj);
		if(typed_obj) {
			if(is_top(typed_obj)) {
				pop(typed_obj);
			} else {
				destruct(typed_obj);
			}
		}
	}

	void dealloc_all() {
		while(tos>mem && prior) {
			pop(prior);
		}
	}

	bool is_top(void *obj) const {
		const chunk_header * const chead = find_chunk_header(to_typed_void(obj));
		return (static_cast<byte_type*>(obj) + chead->object_size) == tos;
	}

	size_type overhead(unsigned int number_of_elements) {
		return sizeof(chunk_header) * number_of_elements;
	}

	size_type size() const { return static_cast<size_type>(tos-mem); }
	size_type capacity() const { return static_cast<size_type>(end_of_mem-mem); }

private:
	typed_void * to_typed_void(void *obj) const {
		return reinterpret_cast<typed_void*>(obj);
	}
	
	dtor_fptr crypt_fptr(dtor_fptr const fptr) {
		return reinterpret_cast<dtor_fptr>(
			reinterpret_cast<size_t>(fptr) ^ reinterpret_cast<size_t>(detail::fptr_cookie)
		);
	}

	template<typename T>
	bool mem_available() {
		return tos + sizeof(chunk_header) + sizeof(T) < end_of_mem;
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
 









	template<typename T>
	void allocate() {
		chunk_header * const chead = new(tos) chunk_header();	
		chead->prior = prior;
		chead->object_size = sizeof(T);
		chead->dtor = crypt_fptr(&detail::call_dtor<T>);
		prior = chead;
		
		// allocate memory
		tos += sizeof(chunk_header) + sizeof(T);
	}
	
	void pop(chunk_header * const chead) {
		typed_void * const obj = find_object(chead);
		pop(chead, obj);
	}

	void pop(typed_void * const obj) {
		chunk_header * const chead = find_chunk_header(obj);
		pop(chead, obj);
	}

	void pop(chunk_header * const chead, typed_void * const obj) {
		dtor_fptr dtor = mark_as_destructed(chead);	
	
		deallocate_as_possible(chead);

		//might throw
		dtor(obj);
	}

	void destruct(typed_void *obj) {
		chunk_header * const chead = find_chunk_header(obj);
		destruct(chead, obj);
	}

	void destruct(chunk_header * const chead) {
		typed_void * const obj = find_object(chead);
		destruct(chead, obj);
	}

	void destruct(chunk_header * const chead, typed_void * const obj) {
		dtor_fptr dtor = mark_as_destructed(chead);
		//might throw
		dtor(obj);
	}

	chunk_header *find_chunk_header(typed_void * const obj) const {
		return reinterpret_cast<chunk_header*>(reinterpret_cast<byte_type*>(obj) - sizeof(chunk_header));
	}

	typed_void *find_object(chunk_header * const chead) const {
		return to_typed_void(reinterpret_cast<byte_type*>(chead)+sizeof(chunk_header));
	}

	dtor_fptr mark_as_destructed(chunk_header * const chead) {
		dtor_fptr const dtor = crypt_fptr(chead->dtor);
		chead->dtor = not_a_dtor;
		return dtor;
	}

	dtor_fptr mark_as_destructed(typed_void *p) {
		chunk_header * const chead = find_chunk_header(p);
		return mark_as_destructed(chead);
	}

	void deallocate_as_possible(chunk_header *chead) {
		while(chead && (chead->dtor == not_a_dtor)) {
			//deallocate memory
			tos = (byte_type*)chead;
			chead = chead->prior;
		}

		//update prior pointer for next allocation
		prior = chead ? chead->prior : NULL;

	}



private:
	//encrypted address of invalid_function_fptr, used to mark dtor as called/invalid
	const dtor_fptr not_a_dtor;
	//top of stack pointer
	byte_type *tos;
	//reserved memory
	byte_type *mem;
	//end of reserved memory region
	byte_type *end_of_mem;
	//points to the chunk before the current tos
	chunk_header *prior;
};

} //namespace obstack
} //namespace boost

#endif //_BOOST_OBSTACK_HPP
