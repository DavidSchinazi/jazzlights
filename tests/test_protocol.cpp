#include "catch.hpp"
#include "DFSParks_Protocol.h"

using namespace dfsparks;

TEST_CASE( "Solid frame: encoding matches decoding", "[protocol]" ) {
    char buf[dfsparks::frame::max_size];
    uint32_t clr;

    REQUIRE( frame::solid::encode(buf, sizeof(buf), 12345) > 0);
    REQUIRE( frame::decode_type(buf, sizeof(buf)) == frame::Type::SOLID );
    REQUIRE( frame::solid::decode(buf, sizeof(buf), &clr) > 0);
    REQUIRE( clr == 12345 );
}

TEST_CASE( "Rainbow frame: encoding matches decoding", "[protocol]" ) {
    char buf[dfsparks::frame::max_size];
    double h1, h2, s, l;

    
    REQUIRE( frame::rainbow::encode(buf, sizeof(buf), 1, 1, 1, 1) > 0);
    REQUIRE( frame::rainbow::decode(buf, sizeof(buf), &h1, &h2, &s, &l) > 0);
    REQUIRE( h1 == Approx(1) );
    REQUIRE( h2 == Approx(1) );
    REQUIRE( s == Approx(1) );
    REQUIRE( l == Approx(1) );

    REQUIRE( frame::rainbow::encode(buf, sizeof(buf), 0.1, 0.2, 0.3, 0.4) > 0);
    REQUIRE( frame::rainbow::decode(buf, sizeof(buf), &h1, &h2, &s, &l) > 0);
    REQUIRE( h1 == Approx(0.1) );
    REQUIRE( h2 == Approx(0.2) );
    REQUIRE( s == Approx(0.3) );
    REQUIRE( l == Approx(0.4) );
}

TEST_CASE( "Can't decode incompatible version", "[protocol]" ) {
    char buf[dfsparks::frame::max_size];
    uint32_t clr;

    REQUIRE( frame::solid::encode(buf, sizeof(buf), 12345) > 0);
    buf[4] = 123;
    REQUIRE( frame::solid::decode(buf, sizeof(buf), &clr) == 0);
}

TEST_CASE( "Can decode compatible version", "[protocol]" ) {
    char buf[dfsparks::frame::max_size];
    uint32_t clr;

    REQUIRE( frame::solid::encode(buf, sizeof(buf), 12345) > 0);
    buf[5] = 123;
    REQUIRE( frame::solid::decode(buf, sizeof(buf), &clr) > 0);
}
