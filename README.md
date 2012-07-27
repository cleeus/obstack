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

