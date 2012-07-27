#include <iostream>
#include <string>
#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE obstack_test
#include <boost/test/unit_test.hpp>
#include <boost/function.hpp>

#include "obstack.hpp"

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
	boost::obstack::obstack vs(default_size);

	BOOST_CHECK( vs.size() == 0 );
	BOOST_CHECK( vs.capacity() == default_size );
}

BOOST_AUTO_TEST_CASE( obstack_single_push ) {
	boost::obstack::obstack vs(default_size);

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
	
	boost::obstack::obstack vs(default_size);
	Sensor *s = vs.alloc<Sensor>();
	s->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
	
	BOOST_CHECK( vs.size() > 0 );

	vs.dealloc(s);

	BOOST_CHECK_EQUAL( vs.size(), 0 );
	BOOST_CHECK_EQUAL( _num_dtor_calls, 1 );

}

BOOST_AUTO_TEST_CASE( obstack_dtor_called_on_dealloc_all ) {

	_num_dtor_calls = 0;
	
	boost::obstack::obstack vs(default_size);
	Sensor *s = vs.alloc<Sensor>();
	s->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
	
	BOOST_CHECK( vs.size() > 0 );

	vs.dealloc_all();

	BOOST_CHECK_EQUAL( vs.size(), 0 );
	BOOST_CHECK_EQUAL( _num_dtor_calls, 1 );

}

BOOST_AUTO_TEST_CASE( obstack_dtor_called_on_scope_exit ) {

	_num_dtor_calls = 0;
	{
		boost::obstack::obstack vs(default_size);
		Sensor *s = vs.alloc<Sensor>();
		s->set_dtor_callback(&obstack_dtor_called_on_scope_exit_func);
		BOOST_CHECK( vs.size() > 0 );
	}
	BOOST_CHECK_EQUAL( _num_dtor_calls, 1 );

}

BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_1_c ) {
	boost::obstack::obstack vs(default_size);
	CtorManiac *foo = vs.alloc<CtorManiac>("");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_1_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_1_nc ) {
	boost::obstack::obstack vs(default_size);

	std::string a1;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_1_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_2_c_c ) {
	boost::obstack::obstack vs(default_size);

	CtorManiac *foo = vs.alloc<CtorManiac>("","");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_2_C_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_2_nc_c ) {
	boost::obstack::obstack vs(default_size);

	std::string a1;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1,"");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_2_NC_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_2_c_nc ) {
	boost::obstack::obstack vs(default_size);

	std::string a2;
	CtorManiac *foo = vs.alloc<CtorManiac>("",a2);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_2_C_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_2_nc_nc ) {
	boost::obstack::obstack vs(default_size);

	std::string a1;
	std::string a2;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1,a2);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_2_NC_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_c_c_c ) {
	boost::obstack::obstack vs(default_size);

	CtorManiac *foo = vs.alloc<CtorManiac>("","", "");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_C_C_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_nc_c_c ) {
	boost::obstack::obstack vs(default_size);

	std::string a1;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1,"", "");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_NC_C_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_c_nc_c ) {
	boost::obstack::obstack vs(default_size);

	std::string a2;
	CtorManiac *foo = vs.alloc<CtorManiac>("", a2, "");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_C_NC_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_c_c_nc ) {
	boost::obstack::obstack vs(default_size);

	std::string a3;
	CtorManiac *foo = vs.alloc<CtorManiac>("", "", a3);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_C_C_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_c_nc_nc ) {
	boost::obstack::obstack vs(default_size);

	std::string a2;
	std::string a3;
	CtorManiac *foo = vs.alloc<CtorManiac>("", a2, a3);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_C_NC_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_nc_c_nc ) {
	boost::obstack::obstack vs(default_size);

	std::string a1;
	std::string a3;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1, "", a3);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_NC_C_NC);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_nc_nc_c ) {
	boost::obstack::obstack vs(default_size);

	std::string a1;
	std::string a2;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1, a2, "");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_NC_NC_C);
}
BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_3_nc_nc_nc ) {
	boost::obstack::obstack vs(default_size);

	std::string a1;
	std::string a2;
	std::string a3;
	CtorManiac *foo = vs.alloc<CtorManiac>(a1, a2, a3);

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_3_NC_NC_NC);
}

BOOST_AUTO_TEST_CASE( obstack_ctor_fwd_10_all_c ) {
	boost::obstack::obstack vs(default_size);

	CtorManiac *foo = vs.alloc<CtorManiac>("", "", "", "", "", "", "", "", "", "");

	BOOST_REQUIRE( foo != NULL );
	
	BOOST_CHECK_EQUAL(foo->called, CtorManiac::CTOR_10_ALL_C);
}




BOOST_AUTO_TEST_SUITE_END()

