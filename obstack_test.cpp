#include <iostream>
#include <string>
#include <vector>

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE obstack_test
#include <boost/test/unit_test.hpp>
#include <boost/function.hpp>

#include "obstack.hpp"


using boost::obstack::obstack;

BOOST_AUTO_TEST_SUITE(obstack_all)

class Sensor {
public:
	typedef boost::function0<void> dtor_callback_type;

	Sensor()
		: this_ptr(this)
	{
	}

	~Sensor() {
		if(dtor_callback) {
			dtor_callback();
		}
	}

	const void * get_this_ptr() const { return this_ptr; }
	void set_dtor_callback(const dtor_callback_type &dtor_callback) {
		this->dtor_callback = dtor_callback;
	}

private:
	dtor_callback_type dtor_callback;
	const void * const this_ptr;
};


class CtorManiac {
public:
	enum CTOR_TYPE {
		CTOR_0,
		CTOR_1_C,
		CTOR_1_NC,
		CTOR_2_C_C,
		CTOR_2_NC_C,
		CTOR_2_C_NC,
		CTOR_2_NC_NC,
		CTOR_3_C_C_C,
		CTOR_3_NC_C_C,
		CTOR_3_C_NC_C,
		CTOR_3_C_C_NC,
		CTOR_3_C_NC_NC,
		CTOR_3_NC_C_NC,
		CTOR_3_NC_NC_C,
		CTOR_3_NC_NC_NC,
		CTOR_10_ALL_C
	};

	CTOR_TYPE called;

	CtorManiac() : called(CTOR_0) {}
	CtorManiac(const std::string &) : called(CTOR_1_C) {}
	CtorManiac(std::string &) : called(CTOR_1_NC) {}
	CtorManiac(const std::string &, const std::string &) : called(CTOR_2_C_C) {}
	CtorManiac(std::string &, const std::string &) : called(CTOR_2_NC_C) {}
	CtorManiac(const std::string &, std::string &) : called(CTOR_2_C_NC) {}
	CtorManiac(std::string &, std::string &) : called(CTOR_2_NC_NC) {}
	
	CtorManiac(const std::string &, const std::string &, const std::string &) : called(CTOR_3_C_C_C) {}
	CtorManiac(std::string &, const std::string &, const std::string &) : called(CTOR_3_NC_C_C) {}
	CtorManiac(const std::string &, std::string &, const std::string &) : called(CTOR_3_C_NC_C) {}
	CtorManiac(const std::string &, const std::string &, std::string &) : called(CTOR_3_C_C_NC) {}
	CtorManiac(const std::string &, std::string &, std::string &) : called(CTOR_3_C_NC_NC) {}
	CtorManiac(std::string &, const std::string &, std::string &) : called(CTOR_3_NC_C_NC) {}
	CtorManiac(std::string &, std::string &, const std::string &) : called(CTOR_3_NC_NC_C) {}
	CtorManiac(std::string &, std::string &, std::string &) : called(CTOR_3_NC_NC_NC) {}

	CtorManiac(
		const std::string &,
		const std::string &,
		const std::string &,
		const std::string &,
		const std::string &,
		const std::string &,
		const std::string &,
		const std::string &,
		const std::string &,
		const std::string &
	) : called(CTOR_10_ALL_C) {}


};

static const size_t default_size = 64*1024;

BOOST_AUTO_TEST_CASE( obstack_size_capacity) {
	obstack vs(default_size);

	BOOST_CHECK( vs.size() == 0 );
	BOOST_CHECK( vs.capacity() == default_size );
}

BOOST_AUTO_TEST_CASE( obstack_single_push ) {
	obstack vs(default_size);

	Sensor *s = vs.alloc<Sensor>();

	BOOST_REQUIRE( s != NULL );

	BOOST_CHECK_EQUAL( s->get_this_ptr() , s );
}


int _num_dtor_calls;
void obstack_dtor_called_on_scope_exit_func() {
	_num_dtor_calls++;
}

BOOST_AUTO_TEST_CASE( obstack_dtor_called_on_delete ) {

	_num_dtor_calls = 0;
	
	obstack vs(default_size);
	Sensor *s = vs.alloc<Sensor>();
	s->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
	
	BOOST_CHECK( vs.size() > 0 );

	vs.dealloc(s);

	BOOST_CHECK_EQUAL( vs.size(), 0 );
	BOOST_CHECK_EQUAL( _num_dtor_calls, 1 );

}

BOOST_AUTO_TEST_CASE( obstack_dtor_called_on_dealloc_all ) {

	_num_dtor_calls = 0;
	
	obstack vs(default_size);
	Sensor *s = vs.alloc<Sensor>();
	s->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
	
	BOOST_CHECK( vs.size() > 0 );

	vs.dealloc_all();

	BOOST_CHECK_EQUAL( vs.size(), 0 );
	BOOST_CHECK_EQUAL( _num_dtor_calls, 1 );
}

BOOST_AUTO_TEST_CASE( obstack_dtor_delete_all_chain ) {

	_num_dtor_calls = 0;

	obstack vs(default_size);
	for(int i=0; i<10; i++) {
		Sensor *s = vs.alloc<Sensor>();
		s->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
	}
	vs.dealloc_all();

	BOOST_CHECK_EQUAL( _num_dtor_calls, 10 );

}

BOOST_AUTO_TEST_CASE( obstack_dtor_dealloc_reverse ) {

	_num_dtor_calls = 0;

	std::vector<Sensor*> sensors;
	obstack vs(default_size);
	for(int i=0; i<10; i++) {
		Sensor *s = vs.alloc<Sensor>();
		s->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
		sensors.push_back(s);
	}

	for(size_t i=1; i<=10; i++) {
		Sensor *s = sensors[sensors.size()-i];
		vs.dealloc(s);
		BOOST_CHECK_EQUAL( _num_dtor_calls, i );
	}

	BOOST_CHECK_EQUAL( _num_dtor_calls, 10 );
}

BOOST_AUTO_TEST_CASE( obstack_dtor_dealloc_forward ) {

	_num_dtor_calls = 0;

	std::vector<Sensor*> sensors;
	obstack vs(default_size);
	for(int i=0; i<10; i++) {
		Sensor *s = vs.alloc<Sensor>();
		s->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
		sensors.push_back(s);
	}

	for(size_t i=1; i<=10; i++) {
		Sensor *s = sensors[i-1];
		vs.dealloc(s);
		BOOST_CHECK_EQUAL( _num_dtor_calls, i );
	}

	BOOST_CHECK_EQUAL( _num_dtor_calls, 10 );
}



BOOST_AUTO_TEST_CASE( obstack_dtor_called_on_scope_exit ) {

	_num_dtor_calls = 0;
	{
		obstack vs(default_size);
		Sensor *s = vs.alloc<Sensor>();
		s->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
		BOOST_CHECK( vs.size() > 0 );
	}
	BOOST_CHECK_EQUAL( _num_dtor_calls, 1 );

}

BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_1_c ) {
	obstack vs(default_size);
	CtorManiac *foo = vs.alloc<CtorManiac>("");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_1_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_1_nc ) {
	obstack vs(default_size);

	std::string a1;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_1_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_2_c_c ) {
	obstack vs(default_size);

	CtorManiac *foo = vs.alloc<CtorManiac>("","");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_2_C_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_2_nc_c ) {
	obstack vs(default_size);

	std::string a1;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1,"");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_2_NC_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_2_c_nc ) {
	obstack vs(default_size);

	std::string a2;
	CtorManiac *foo = vs.alloc<CtorManiac>("",a2);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_2_C_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_2_nc_nc ) {
	obstack vs(default_size);

	std::string a1;
	std::string a2;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1,a2);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_2_NC_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_c_c_c ) {
	obstack vs(default_size);

	CtorManiac *foo = vs.alloc<CtorManiac>("","", "");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_C_C_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_nc_c_c ) {
	obstack vs(default_size);

	std::string a1;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1,"", "");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_NC_C_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_c_nc_c ) {
	obstack vs(default_size);

	std::string a2;
	CtorManiac *foo = vs.alloc<CtorManiac>("", a2, "");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_C_NC_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_c_c_nc ) {
	obstack vs(default_size);

	std::string a3;
	CtorManiac *foo = vs.alloc<CtorManiac>("", "", a3);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_C_C_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_c_nc_nc ) {
	obstack vs(default_size);

	std::string a2;
	std::string a3;
	CtorManiac *foo = vs.alloc<CtorManiac>("", a2, a3);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_C_NC_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_nc_c_nc ) {
	obstack vs(default_size);

	std::string a1;
	std::string a3;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1, "", a3);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_NC_C_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_nc_nc_c ) {
	obstack vs(default_size);

	std::string a1;
	std::string a2;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1, a2, "");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_NC_NC_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_nc_nc_nc ) {
	obstack vs(default_size);

	std::string a1;
	std::string a2;
	std::string a3;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1, a2, a3);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_NC_NC_NC);
}

BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_10_all_c ) {
	obstack vs(default_size);

	CtorManiac *foo = vs.alloc<CtorManiac>("", "", "", "", "", "", "", "", "", "");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_10_ALL_C);
}


BOOST_AUTO_TEST_CASE( obstack_is_top_one_elem) {
	obstack vs(default_size);

	Sensor *s = vs.alloc<Sensor>();
	BOOST_CHECK( vs.is_top(s) );
}

BOOST_AUTO_TEST_CASE( obstack_is_top_two_elems) {
	obstack vs(default_size);

	Sensor *s1 = vs.alloc<Sensor>();
	Sensor *s2 = vs.alloc<Sensor>();

	BOOST_CHECK( vs.is_top(s2) );
	BOOST_CHECK( !vs.is_top(s1) );
}


BOOST_AUTO_TEST_CASE( obstack_nesting ) {

	_num_dtor_calls = 0;

	{
		obstack l1(default_size);
		obstack *l2 = l1.alloc<obstack>(default_size);
		BOOST_REQUIRE( l2 != NULL );
		obstack *l3 = l2->alloc<obstack>(default_size);
		BOOST_REQUIRE( l3 != NULL );
		
		
		Sensor *s1 = l1.alloc<Sensor>();
		BOOST_REQUIRE( s1 != NULL);
		Sensor *s2 = l3->alloc<Sensor>();
		BOOST_REQUIRE( s2 != NULL);

		s1->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
		s2->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
	}


	BOOST_CHECK_EQUAL( _num_dtor_calls, 2 );
}



BOOST_AUTO_TEST_SUITE_END()

