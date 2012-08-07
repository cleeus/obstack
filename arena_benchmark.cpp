#include <cstdlib>

#include <iostream>
#include <vector>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <boost/type_traits/alignment_of.hpp>
#include <boost/bind.hpp>

#include "obstack.hpp"
#include "max_alignment_type.hpp"

using namespace boost::posix_time;

typedef std::vector<size_t> alloc_order_vec;

static alloc_order_vec make_alloc_sequence(const size_t total_memory, const size_t min_alloc_size, const size_t max_alloc_size) {
	boost::mt19937 gen;
	gen.seed(42);
	
	alloc_order_vec out;
	boost::uniform_int<size_t> dist(min_alloc_size, max_alloc_size);

	size_t mem_sum = 0;
	while(mem_sum < total_memory) {
		const size_t s = dist(gen);
		out.push_back(s);
		mem_sum += s;
	}

	//fix last chunk to exactly fill total_memory
	if(mem_sum > total_memory) {
		const size_t last_size = out[out.size()-1];
		out[out.size()-1] = last_size - (mem_sum - total_memory);
		const size_t new_last_size = out[out.size()-1];
		if( new_last_size == 0) {
			out[out.size()-1] = 1;
		}
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

size_t sum_vec(const alloc_order_vec &seq) {
	size_t sum=0;
	for(size_t i=0; i<seq.size(); i++) {
		sum += seq[i];
	}
	return sum;
}

class timing_registry {
private:
	typedef boost::mutex mutex_type;
	typedef boost::lock_guard<mutex_type> lock_type;	
public:
	enum benchmark {
		BENCHMARK_OBSTACK = 0,
		BENCHMARK_MALLOC_FREE = 1,
		BENCHMARK_NEW_DELETE = 2
	};

	void account(benchmark which, const time_duration &how_long) {
		lock_type lock(mutexes[to_index(which)]);
		durations[to_index(which)] += how_long;
	}

	time_duration get(benchmark which) {
		lock_type lock(mutexes[to_index(which)]);
		return durations[to_index(which)];
	}

private:
	size_t to_index(benchmark e) { return static_cast<size_t>(e); }
	
	time_duration durations[3];
	mutex_type mutexes[3];
	
};


//this function keeps the optimizer from optimizing away the allocation benchmarking code
//and does error checking for failed allocations
static inline void check_alloc(volatile char *p, const char *file, int line, const char *func) {
	if(p==NULL) {
		std::cerr << "out of memory in: " << file << ":" << line << " " << func << std::endl;
		exit(1);
	}
}
#define CHECK_ALLOC(p) check_alloc(p, __FILE__, __LINE__, __FUNCTION__);

static void benchmark_malloc(
	boost::barrier &start_allocs,
	const alloc_order_vec &alloc_seq,
	const alloc_order_vec &free_seq,
	const size_t iterations,
	timing_registry &timings
) {
	std::vector<char*> chunks;
	chunks.resize(alloc_seq.size());
	
	start_allocs.wait();

	ptime start(microsec_clock::universal_time());
	
	for(size_t i=0; i<iterations; i++) {
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
	}

	ptime end(microsec_clock::universal_time());

	timings.account(timing_registry::BENCHMARK_MALLOC_FREE, end-start);
}


static void benchmark_obstack(
	boost::barrier &start_allocs,
	const alloc_order_vec &alloc_seq,
	const alloc_order_vec &free_seq,
	const size_t iterations,
	timing_registry &timings
) {
	std::vector<char*> chunks;
	chunks.resize(alloc_seq.size());

	const size_t alloc_sum = sum_vec(alloc_seq);
	const size_t required_size = alloc_sum + boost::arena::obstack::max_overhead(alloc_seq.size());
	boost::arena::obstack obs(required_size);
	
	start_allocs.wait();
	
	ptime start(microsec_clock::universal_time());

	for(size_t i=0; i<iterations; i++) {
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
	}

	ptime end(microsec_clock::universal_time());

	timings.account(timing_registry::BENCHMARK_OBSTACK, end-start);
}

static void benchmark_new_delete(
	boost::barrier &start_allocs,
	const alloc_order_vec &alloc_seq,
	const alloc_order_vec &free_seq,
	const size_t iterations,
	timing_registry &timings
) {

	std::vector<char*> chunks;
	chunks.resize(alloc_seq.size());

	try {
		start_allocs.wait();

		ptime start(microsec_clock::universal_time());

		for(size_t i=0; i<iterations; i++) {
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
		}
		
		ptime end(microsec_clock::universal_time());

		timings.account(timing_registry::BENCHMARK_NEW_DELETE, end-start);

	} catch(std::bad_alloc&) {
		std::cerr << "bad_alloc in new/delete benchmark" << std::endl;
		exit(1);
	}
}

static void benchmark_threaded(
	const size_t num_threads,
	const size_t total_memory,
	const size_t min_alloc_size,
	const size_t max_alloc_size,
	const size_t iterations
) {
	timing_registry timings;

	//create splitted alloc orders
	std::vector<alloc_order_vec> alloc_orders;
	std::vector<alloc_order_vec> free_orders;
	const size_t per_thread_memory = total_memory / num_threads;
	for(size_t i=0; i<num_threads; i++) {
		const alloc_order_vec &alloc_seq = make_alloc_sequence(per_thread_memory, min_alloc_size, max_alloc_size);
		const alloc_order_vec &free_seq = make_free_sequence(alloc_seq);
		alloc_orders.push_back(alloc_seq);
		free_orders.push_back(free_seq);
	}

	const size_t per_thread_iterations = iterations / num_threads;
	
	size_t total_allocs=0;
	for(size_t i=0; i<num_threads; i++) {
		total_allocs += alloc_orders[i].size();
	}
	
	std::cout << "running memory management benchmarks with " << num_threads << " threads" << std::endl;
	std::cout << "             memory per thread: " << per_thread_memory / 1024 << "kB" << std::endl;
	std::cout << "  alloc/dealloc ops per thread: " << (total_allocs * iterations) / num_threads << std::endl;
	std::cout << "       total alloc/dealloc ops: " << total_allocs * iterations << std::endl;

	//start benchmarking
	
	//malloc
	{
		boost::thread_group threads;
		boost::barrier start_allocs(num_threads);
		for(size_t i=0; i<num_threads; i++) {
			threads.create_thread(
				boost::bind(
					benchmark_malloc,
					boost::ref(start_allocs),
					boost::ref(alloc_orders[i]),
					boost::ref(free_orders[i]),
					per_thread_iterations,
					boost::ref(timings)
				)
			);
		}
		threads.join_all();
	}
	
	//new_delete
	{
		boost::thread_group threads;
		boost::barrier start_allocs(num_threads);
		for(size_t i=0; i<num_threads; i++) {
			threads.create_thread(
				boost::bind(
					benchmark_new_delete,
					boost::ref(start_allocs),
					boost::ref(alloc_orders[i]),
					boost::ref(free_orders[i]),
					per_thread_iterations,
					boost::ref(timings)
				)
			);
		}
		threads.join_all();
	}

	//obstack
	{
		boost::thread_group threads;
		boost::barrier start_allocs(num_threads);
		for(size_t i=0; i<num_threads; i++) {
			threads.create_thread(
				boost::bind(
					benchmark_obstack,
					boost::ref(start_allocs),
					boost::ref(alloc_orders[i]),
					boost::ref(free_orders[i]),
					per_thread_iterations,
					boost::ref(timings)
				)
			);
		}
		threads.join_all();
	}
	
	
	//print statistics
	std::cout << "  done!" << std::endl;
	std::cout << "  timings:" << std::endl;
  std::cout << "              malloc/free heap: " <<
		timings.get(timing_registry::BENCHMARK_MALLOC_FREE).total_milliseconds() << "ms" << std::endl;
	std::cout << "               new/delete heap: " <<
		timings.get(timing_registry::BENCHMARK_NEW_DELETE).total_milliseconds() << "ms" << std::endl;
	std::cout << "                 obstack arena: " <<
		timings.get(timing_registry::BENCHMARK_OBSTACK).total_milliseconds() << "ms" << std::endl;
	std::cout << std::endl;
}

int main(int argc, char **argv) {
	const size_t total_memory = 1024*1024* 512;
	const size_t min_alloc_size = 1;
	const size_t max_alloc_size = 1024*1024* 4;
	const size_t iterations = 1000;
	
	const size_t num_cores = boost::thread::hardware_concurrency();
	std::cout << "global parameters:" << std::endl;
	std::cout << "           cpu cores: " << num_cores << std::endl;
	std::cout << "        total memory: " << total_memory / 1024 << "kB" << std::endl;
	std::cout << "  min/max block size: " << min_alloc_size << "B/" << max_alloc_size/1024 << "kB" << std::endl;
	std::cout << std::endl;	

	//allways benchmark with 1 and 2 threads
	benchmark_threaded(1, total_memory, min_alloc_size, max_alloc_size, iterations);
	benchmark_threaded(2, total_memory, min_alloc_size, max_alloc_size, iterations);
	

	//if the machine has more than 1 core, some more runs may be interesting
	const bool has_equal_num_cores_run = (num_cores == 1 || num_cores == 2);
	const bool has_double_num_cores_run = (num_cores*2 == 2);
	if(!has_equal_num_cores_run) {
		benchmark_threaded(num_cores, total_memory, min_alloc_size, max_alloc_size, iterations);
	}
	if(!has_double_num_cores_run) {
		benchmark_threaded(num_cores*2, total_memory, min_alloc_size, max_alloc_size, iterations);
	}


	return 0;
}
