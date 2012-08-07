#include <limits>
#include <cstdlib>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>


#include "obstack.hpp"

namespace boost {
namespace arena {
namespace arena_detail {

void free_marker_dtor(void*) {
}
void array_of_primitives_dtor(void*) {
}

static size_t seed_from_heap_memory() {
	size_t const len = 64*1024 / sizeof(size_t);
	size_t * const mem = new size_t[len];
	size_t seed = reinterpret_cast<size_t>(mem);
	
	for(size_t i=0; i<len; i++) {
		seed ^= mem[i];
	}
	
	delete[] mem;
	return seed;
}

static size_t make_strong_seed() {
	size_t seed = 0;
	seed ^= seed_from_heap_memory();
	//TODO add more seed sources	
	return seed;
}

static size_t init_cookie() {
	boost::mt19937 gen;
	static size_t seed = 0;
	if(!seed) {
		seed = make_strong_seed();
	} else {
		seed++;
	}

	gen.seed(seed);
	boost::uniform_int<size_t> dist(0, std::numeric_limits<size_t>::max());

	size_t const cookie = dist(gen);
	
	return cookie;
}

static int invalid_addr_reference;
size_t const ptr_sec::_checksum_cookie = init_cookie();
void * const ptr_sec::_xor_cookie = reinterpret_cast<void*>(init_cookie());
void * const ptr_sec::_invalid_addr = &invalid_addr_reference;
//warning: initialization order is important here
dtor_fptr const free_marker_dtor_xor = ptr_sec::xor_ptr(&free_marker_dtor);
dtor_fptr const array_of_primitives_dtor_xor = ptr_sec::xor_ptr(&array_of_primitives_dtor);



} //namespace arena_detail
} //namespace arena
} //namespace boost

