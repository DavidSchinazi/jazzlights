#include "unisparks.hpp"
#include "catch.hpp"

void dummyrender(int, uint8_t, uint8_t, uint8_t) {
}

TEST_CASE( "Player invariants", "[player]" ) {
    using namespace unisparks;
    Player pl;
    Matrix bigPane{250,50};

    pl.begin(bigPane, dummyrender);

    pl.render();

    REQUIRE( pl.effectCount() > 2 );
    REQUIRE( pl.effectIndex() == 0 );
    REQUIRE( !strcmp(pl.effectName(), "rainbow" ));

    pl.render(); // check that we don't crash
    
//    pl.begin();    // should do nothing
//    pl.begin(); // should do nothing

    REQUIRE( pl.effectIndex() == 0 );
    pl.next();
    REQUIRE( pl.effectIndex() == 1 );
    pl.next();
    REQUIRE( pl.effectIndex() == 2 );
    pl.prev();
    REQUIRE( pl.effectIndex() == 1 );
    pl.play(0);
    REQUIRE( pl.effectIndex() == 0 );
    pl.play(2);
    REQUIRE( pl.effectIndex() == 2 );


}
