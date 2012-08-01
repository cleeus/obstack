#include <cstdlib>

#include <iostream>
#include <vector>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "obstack.hpp"

using namespace boost::posix_time;

typedef std::vector<size_t> alloc_order_vec;

static alloc_order_vec make_alloc_sequence() {
	boost::mt19937 gen;
	gen.seed(42);
	
	const size_t min_alloc_size = 1;
	const size_t max_alloc_size = 2 * 1024 * 1024;
	const size_t num_allocs = 512*1024*1024 / ((min_alloc_size+max_alloc_size)/2); //alloc about 512MB
	
	alloc_order_vec out;
	out.resize(num_allocs);
	boost::uniform_int<size_t> dist(min_alloc_size, max_alloc_size);
	
	for(size_t i=0; i<num_allocs; i++) {
		out[i] = dist(gen);
	}

	return out;
}

static alloc_order_vec make_free_sequence(const alloc_order_vec &alloc_seq) {
	alloc_order_vec out;
	out.resize(alloc_seq.size());

	boost::mt19937 gen;
	gen.seed(42);

	alloc_order_vec positions;
	positions.resize(alloc_seq.size());
	for(size_t i=0; i<positions.size(); i++) {
		positions[i] = i;
	}

	for(size_t i=0; i<out.size(); i++) {
		boost::uniform_int<size_t> dist(0, positions.size()-1);
		const size_t k = dist(gen);
		
		out[i] = positions[k];

		positions.erase(positions.begin()+k);
	}

	return out;
}

static inline void CHECK_ALLOC(void *p, const char *file, int line, const char *func) {
	if(p==NULL) {
		std::cout << "out of memory in: " << file << ":" << line << " " << func << std::endl;
		exit(1);
	}
}
#define CHECK_ALLOC(p) CHECK_ALLOC(p, __FILE__, __LINE__, __FUNCTION__);

static time_duration benchmark_malloc(const alloc_order_vec &alloc_seq, const alloc_order_vec &free_seq) {

	std::vector<char*> chunks;
	chunks.resize(alloc_seq.size());

	ptime start(microsec_clock::universal_time());

	//1. in order alloc/free
	for(size_t i=0; i<alloc_seq.size(); i++) {
		const size_t s = alloc_seq[i];
		chunks[i] = (char*)malloc(s);
		CHECK_ALLOC(chunks[i]);
	}
	for(size_t i=0; i<chunks.size(); i++) {
		char* const p = chunks[i];
		free(p);
	}

	//2. reverse order alloc/free
	for(size_t i=0; i<alloc_seq.size(); i++) {
		const size_t s = alloc_seq[i];
		chunks[i] = (char*)malloc(s);
		CHECK_ALLOC(chunks[i]);
	}
	for(size_t i=0; i<chunks.size(); i++) {
		char* const p = chunks[chunks.size()-1-i];
		free(p);
	}

	//3. random order alloc/free
	for(size_t i=0; i<alloc_seq.size(); i++) {
		const size_t s = alloc_seq[i];
		chunks[i] = (char*)malloc(s);
		CHECK_ALLOC(chunks[i]);
	}
	for(size_t i=0; i<free_seq.size(); i++) {
		char* const p = chunks[free_seq[i]];
		free(p);
	}

	ptime end(microsec_clock::universal_time());

	return end-start;
}


static time_duration benchmark_obstack(const alloc_order_vec &alloc_seq, const alloc_order_vec &free_seq) {

	std::vector<char*> chunks;
	chunks.resize(alloc_seq.size());

	ptime start(microsec_clock::universal_time());

	const size_t required_size = 512*1024*1024 + 32*1024*1024;
	boost::arena::obstack obs(required_size);

	//1. in order alloc/free
	for(size_t i=0; i<alloc_seq.size(); i++) {
		const size_t s = alloc_seq[i];
		chunks[i] = obs.alloc_array<char>(s);
		CHECK_ALLOC(chunks[i]);
	}
	for(size_t i=0; i<chunks.size(); i++) {
		char* const p = chunks[i];
		obs.dealloc(p);
	}

	//2. reverse order malloc/free
	for(size_t i=0; i<alloc_seq.size(); i++) {
		const size_t s = alloc_seq[i];
		chunks[i] = obs.alloc_array<char>(s);
		CHECK_ALLOC(chunks[i]);
	}
	for(size_t i=0; i<chunks.size(); i++) {
		char* const p = chunks[chunks.size()-1-i];
		obs.dealloc(p);
	}

	//3. random order malloc/free
	for(size_t i=0; i<alloc_seq.size(); i++) {
		const size_t s = alloc_seq[i];
		chunks[i] = obs.alloc_array<char>(s);
		CHECK_ALLOC(chunks[i]);
	}
	for(size_t i=0; i<free_seq.size(); i++) {
		char* const p = chunks[free_seq[i]];
		obs.dealloc(p);
	}

	ptime end(microsec_clock::universal_time());

	return end-start;
}

static time_duration benchmark_new_delete(const alloc_order_vec &alloc_seq, const alloc_order_vec &free_seq) {

	std::vector<char*> chunks;
	chunks.resize(alloc_seq.size());

	ptime start(microsec_clock::universal_time());

	//1. in order alloc/free
	for(size_t i=0; i<alloc_seq.size(); i++) {
		const size_t s = alloc_seq[i];
		chunks[i] = new char[s];
		CHECK_ALLOC(chunks[i]);
	}
	for(size_t i=0; i<chunks.size(); i++) {
		char* const p = chunks[i];
		delete[] p;
	}

	//2. reverse order malloc/free
	for(size_t i=0; i<alloc_seq.size(); i++) {
		const size_t s = alloc_seq[i];
		chunks[i] = new char[s];
		CHECK_ALLOC(chunks[i]);
	}
	for(size_t i=0; i<chunks.size(); i++) {
		char* const p = chunks[chunks.size()-1-i];
		delete[] p;
	}

	//3. random order malloc/free
	for(size_t i=0; i<alloc_seq.size(); i++) {
		const size_t s = alloc_seq[i];
		chunks[i] = new char[s];
		CHECK_ALLOC(chunks[i]);
	}
	for(size_t i=0; i<free_seq.size(); i++) {
		char* const p = chunks[free_seq[i]];
		delete[] p;
	}

	ptime end(microsec_clock::universal_time());

	return end-start;
}


size_t sum_vec(const alloc_order_vec &seq) {
	size_t sum=0;
	for(size_t i=0; i<seq.size(); i++) {
		sum += seq[i];
	}
	return sum;
}

int main(int argc, char **argv) {
	const alloc_order_vec &alloc_seq = make_alloc_sequence();
	const alloc_order_vec &free_seq = make_free_sequence(alloc_seq);
	const size_t iterations = 1000;
	const size_t alloc_sum = sum_vec(alloc_seq);

	std::cout << "running single threaded allocation benchmarks" << std::endl;
	std::cout << "  allocs/deallocs per round: " << alloc_seq.size() * 3 << std::endl;
	std::cout << "           memory per round: " << alloc_sum / 1024 << "kB" << std::endl;
	std::cout << "                     rounds: " << iterations << std::endl;

	time_duration malloc_time;
	time_duration obstack_time;
	time_duration new_delete_time;
	for(size_t i=0; i<iterations; i++) {
		malloc_time += benchmark_malloc(alloc_seq, free_seq);
		obstack_time += benchmark_obstack(alloc_seq, free_seq);
		new_delete_time += benchmark_new_delete(alloc_seq, free_seq);
	}

	std::cout << "done, timings:" << std::endl;
	std::cout << "malloc/free heap: " << malloc_time.total_milliseconds() << "ms" << std::endl;
	std::cout << " new/delete heap: " << new_delete_time.total_milliseconds() << "ms" << std::endl;
	std::cout << "   obstack arena: " << obstack_time.total_milliseconds() << "ms" << std::endl;

}
