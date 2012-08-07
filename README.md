obstack
=======

A C++ memory arena implementation with O(1) complexity.

Obstack is a memory arena which can allocate objects in
a preallocated memory region of fixed size (the arena).
It organizes memory in a stack of objects.
All memory operations are O(1). This is achieved
with a memory allocation strategy known as pointer bumping.

Obstack is not (yet) included in the boost library,
but hopefully will be when it's ready.


Copyright Kai Dietrich <mail@cleeus.de> 2012.
Distributed under the Boost Software License, Version 1.0.
See accompanying file LICENSE_1_0.txt or get a copy at
http://www.boost.org/LICENSE_1_0.txt.


Rationale
=========

General purpose memory allocators like the usual
malloc/free implementations (dlmalloc, jemalloc, etc.)
provide great flexibility to the programmer and in 98%
of all use cases they are well suited to solve the problem.
But there are some corner cases of software enginering where
general purpose memory allocation algorithms may suffer from
some problems. Notably some of these are:
* heap fragmentation
* time complexity of O(n)
* poor scalability and lock contention
* space overhead

Some of these problems can in special cases, be adressed with
custom memory allocators with restricted semantics.

General Purpose Memory Allocators
---------------------------------

General purpose memory allocators request memory from the operating
system and provide it to the programmer. When the programmer frees
a block of memory, the allocator is free to return it to the OS or
to keep it for later reuse. For performance reasons, the allocator
will probably keep it and organize these free chunks in linked lists,
so called free lists. When the programmer requests a new chunk,
the allocator first looks through the free lists before before requesting
new memory from the OS.

Heap Fragmentation
------------------

Heap fragmentation occurs when the heap address space of a programm
is scattered with small chunks of memory that are in use. Between
these small chunks there may be lots of free memory but not in
contigous blocks. This problem is similar to fragmentation in
harddisk filesystems. Now when the programmer tries to allocate
a reasonably large block of contigous memory, the memory
allocator may not be able to fullfill the request, neither be reusing
a free block nor by requesting new memory from the operation system
because there is no more free region in the address space of the program.
This often means a fatal out of memory exception.

Heap fragmentation can be reduced with a few techniques:
* Using a large enough address space (e.g. 64bit).
* Using fewer objects.
* Using the stack instead of the heap.
* Using memory blocks of the same size.
* Using memory arenas for certain subsystems.

A memory arena is a block of memory that is requested from the
general purpose allocator and then used in a subsystem
of the program. The arena allows the programmer to allocate
memory from the previously reserved memory block.
While the arena implementation may provide different allocation/deallocation
semantics, all arena implementations have one thing in common:
When no more objects from the arena are needed, the whole block of
memory can be freed at once and returned to the general
purpose allocator.

Obstack is such a memory arena. It allocates an internal
block of memory of a given size and then provides
allocate and deallocate methods that will only allocate
objects in this internal memory block.

O(n) Runtime Complexity
-----------------------

Organizing free blocks of memory in linked lists, leads to
a runtime complexity class of O(n). Which means that
alloc/free operations get slower the more elements
there are in these free lists. Most of the time this
run time overhead can be neglected, but in some
parts like parsers or other subsystems which allocate
and deallocate large numbers of objects it cannot.

Obstack uses a technique called pointer bumping to
create new objects. That means it behaves like a stack.
There is a top-of-stack pointer which is advanced with every
newly allocated object. When deallocating an object,
there are two cases:
When the object is not at the top of the stack, it's destructor
is called and the memory is marked as free but cannot yet be reused.
When the object is at the top of the stack, it's destructor
is called and the memory is marked as free.
Then free memory is reclaimed as far as possible which means
moving the top-of-stack pointer back as far as objects are already
freed. 

This leads to an average O(1) complexity but comes at the cost of
increased memory usage.

Scalability
-----------

When writing multithreaded code, the heap and the general purpose
memory allocator are a shared ressource. That means all threads
have to synchronise on the internal structures of the allocator.
When multiple threads do large amounts of alloc/free operations,
the heap and its locks can become a serious bottleneck
limiting the parallelity of the program.

Using a specific memory arena instance like an obstack in a
certain subsystem of the program, most of the time
also means that every thread can use it's own arena instance.
This means the heap is only a shared ressource uppon initialization.
After that, there is no more sharing and
no more lock contention which increases scalability.

Overhead
--------
For each chunk a general purpose allocator allocates, it most likely
also needs a small chunk header. So when the programmer allocates
one byte, the allocator has to add an overhead. Additionally to the
chunk header/trailer, the allocator also has to add some padding
to a platform specific alignment border.
Modern allocators have several optimization techniques for
efficient allocation of small objects.

Memory overhead usage can be reduce with using pool allocators.
These are allocators which basically allocate densly packed arrays
of objects of the same size and can mark them as free with an
overlayed free list. The disadvantage of pool allocators is,
that they can only serve objects of the same size.

Obstack cannot help much here. To the contrary: although
carefully optimized for packing as densly as possible while
still keeping objects aligned, there has to be an additional
chunk header per object. Highly optimized general purpose
allocators may be able to beat obstack here.
Also obstack, by design, can only free memory in the reverse
order it was allocated.

Memory Alignment
================

When implementing custom memory allocators, one has to be aware of
the concept of alignment. Alignment means, that the platform
requires an object of a specific type to be located at a
memory address that is a multiple of a type-specific alignment value.
E.g. an int value on x86-32 should be aligned to 4byte boundaries.
On x86, all but a few specific SSE instructions don't require
the alignment to function, but are much faster when the alignment
is correct. Other platforms may fail when the alignment is not correct.

The general purpose memory allocator with the malloc/free interface
has no information about the type the programmer is about to
allocate. That means it cannot know the alignment requirement
for the memory block it is about to allocate. To still provide
correct alignment, the malloc call is guaranteed to return
a pointer that is suitable for any object.
It does so by aligning the memory to the highest possible
alignment value which is the alignment of "long double" on most
platforms (often 16 bytes). C++11 provides a new type
std::max\_align\_t which is the type with the highest required alignment.
The alignment requirement of a compound type (struct, class)
is the maximum of the alignment requirements of its element types.

The obstack interface is template function alloc\<T\>(...).
That means at the time of allocation, obstack has knowledge about
the alignment requirements of an object. It uses this knowledge to
pack objects in memory as densly as possible while still aligning
them correctly.

Security
========
Memory management is not only a problem of effifiency but also
one of correctness and thus security.
When the free operation is supplied
an invalid pointer that is either not been allocated using
allocator or already been freed, the behaviour of
many traditional general purpose allocators is undefined.
When attackers can force a heap corruption, they can often
get an arbitrary write into the address space of the program.
This is often enough to circumvent critical security
mechanisms and can lead to arbitrary code execution.
To make the life of attackers a little harder, obstack implements
a few countermeasures.

Pointer Encryption
------------------
To be able to properly destroy objects without using the delete
operator and freeing their memory, obstack has to store a
function pointer to the destructor for each object.
This function pointer is an ideal target for an arbitrary write
since an attacker could get immediate code execution.
To prevent this, obstack encrypts (XORs) the destructor pointers
with a random cookie value that is initialized on program start.

Pointer Checking
----------------
To prevent operating on non-arena memory in the first place,
obstack checks the validity of a pointer before attempting
to delete it.
This is done by a both, a simple range check if the pointer
is inside the arena and a fast XOR based checksum on
the chunk header and a second random cookie.


Obstack in Comparison
=====================

Boost Pool
----------

The boost software libraries already provide pool memory allocators.

Obstack Performance
-------------------
There is a little benchmark application included.
It simply generates one stable sequence of random allocation sizes and
then repeatedly allocates/deallocates them on malloc/free, new/delete
and obstack::alloc\_array/obstack::dealloc.
In total about 1GB will be allocated in chunks between 1B and 4MB.
When using multiple threads, the alloc sequence is split between the threads.
Every allocator implementation allocates the same sequence of chunks.

The obstack benchmark does not include the time required to allocate
the arena from the heap since this could also be done
from the stack or only once per program run.
Also each thread has it's own obstack arena since
this is exactly what an arena should be used for.

Here are some preliminary results run on a tiny Intel Atom N450 1.6GHz
(single core, hyperthreaded) netbook.
The system runs a linux 3.4 kernel with gcc-4.5.3 (CFLAGS: -O2) and glibc-2.14.1.
~~~~~~~~
$ ./arena\_benchmark
global parameters:
           cpu cores: 2
        total memory: 1048576kB
  min/max block size: 1B/4096kB

running memory management benchmarks with 1 threads
             memory per thread: 1048576kB
  alloc/dealloc ops per thread: 514000
       total alloc/dealloc ops: 514000
  done!
  timings:
              malloc/free heap: 26882ms
               new/delete heap: 27654ms
                 obstack arena: 272ms

running memory management benchmarks with 2 threads
             memory per thread: 524288kB
  alloc/dealloc ops per thread: 269000
       total alloc/dealloc ops: 538000
  done!
  timings:
              malloc/free heap: 73787ms
               new/delete heap: 90000ms
                 obstack arena: 162ms

running memory management benchmarks with 4 threads
             memory per thread: 262144kB
  alloc/dealloc ops per thread: 135000
       total alloc/dealloc ops: 540000
  done!
  timings:
              malloc/free heap: 108245ms
               new/delete heap: 108809ms
                 obstack arena: 125ms
~~~~~~~~

As you can see, an obstack memory arena
is orders of magnitudes faster than the linux
default allocator. You can also see that the default
linux glibc allocator seriously suffers from lock
contention when run in multithreaded environments.
Other allocaters especially tcmalloc or jemalloc might
scale better. Obstack arenas scale well with the number
of threads.


Literature
==========
To get an overview of the problem domain, the following literature may be helpful:

* "Reconsidering custom memory allocation", Emery D. Berger, Benjamin G. Zorn and Kathryn S. McKinley,
  in Proceedings of the 17th ACM SIGPLAN conference / OOPSLA '02,
  [http://doi.acm.org/10.1145/582419.582421](http://doi.acm.org/10.1145/582419.582421),
  [http://people.cs.umass.edu/~emery/pubs/berger-oopsla2002.pdf](http://people.cs.umass.edu/~emery/pubs/berger-oopsla2002.pdf)
* "The Scoped Allocator Model (Rev 2)", Pablo Halpern, N2554=08-0064 in the C++11 commitee WG21 archives on open-std.org,
  [http://www.open-std.org/jtc1/sc22/WG21/docs/papers/2008/n2554.pdf](http://www.open-std.org/jtc1/sc22/WG21/docs/papers/2008/n2554.pdf)
* Boost.Pool Documentation,
  [http://www.boost.org/doc/libs/1_50_0/libs/pool/doc/html/boost_pool/pool.html](http://www.boost.org/doc/libs/1_50_0/libs/pool/doc/html/boost_pool/pool.html)
* "Custom Memory Allocation in C++", bitsquid blog,
  [http://bitsquid.blogspot.de/2010/09/custom-memory-allocation-in-c.html](http://bitsquid.blogspot.de/2010/09/custom-memory-allocation-in-c.html)
* TCMalloc,
  [http://goog-perftools.sourceforge.net/doc/tcmalloc.html](http://goog-perftools.sourceforge.net/doc/tcmalloc.html)
