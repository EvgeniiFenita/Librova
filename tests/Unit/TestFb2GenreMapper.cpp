#include <catch2/catch_test_macros.hpp>

#include "Fb2Parsing/Fb2GenreMapper.hpp"

TEST_CASE("Fb2GenreMapper resolves known FB2 2.1 codes to display names", "[fb2-genre-mapper]")
{
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf")            == "Science Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_space")      == "Space");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("science_fiction") == "Science Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_horror")     == "Horror & Mystic");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("det_police")    == "Police Stories");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("det_hard")      == "Hard-Boiled");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("thriller")      == "Thrillers");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("detective")     == "Detectives");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("adventure")     == "Adventure");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("adv_western")   == "Western");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("adv_geo")       == "Travel & Geography");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("children")      == "Children's");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("child_tale")    == "Fairy Tales");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("poetry")        == "Poetry");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("antique")       == "Antique Literature");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("antique_myths") == "Myths, Legends & Epics");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_math")      == "Mathematics");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_history")   == "History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("science")       == "Science");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("comp_programming") == "Programming");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("computers")     == "Computers & Internet");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("ref_ref")       == "Reference");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("reference")     == "Reference");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("nonf_biography") == "Biography & Memoirs");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("nonfiction")    == "Non-Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion")      == "Religion");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("humor")         == "Humor");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("home_cooking")  == "Cooking");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("home_sex")      == "Erotica & Sex");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("home")          == "Home & Family");
}

TEST_CASE("Fb2GenreMapper returns the raw code unchanged for unrecognized input", "[fb2-genre-mapper]")
{
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("custom_genre")    == "custom_genre");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("")                == "");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("SF")              == "SF");
}

TEST_CASE("Fb2GenreMapper resolution is case-sensitive", "[fb2-genre-mapper]")
{
    // FB2 codes are always lowercase; uppercase variants are not recognised codes
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("SF_SPACE")   == "SF_SPACE");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Adventure")  == "Adventure");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("THRILLER")   == "THRILLER");
}
