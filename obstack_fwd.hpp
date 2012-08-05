#ifndef BOOST_ARENA_OBSTACK_FWD_HPP
#define BOOST_ARENA_OBSTACK_FWD_HPP

#include <memory>

#include "max_alignment_type.hpp"

namespace boost {
namespace arena {

template<class A = std::allocator<max_align_t> > class basic_obstack;
typedef basic_obstack<> obstack;


} //namespace arena
} //namespace boost

#endif //BOOST_ARENA_OBSTACK_FWD_HPP
