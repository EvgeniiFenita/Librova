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
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("children")          == "Children's");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("child_tale")        == "Fairy Tales");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("child_4")           == "Children's");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("child_characters")  == "Children's Characters");
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
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_adv")          == "Adventure Literature");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_history")      == "Historical Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("biogr_professionals")     == "Professional Biographies");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("biz_management")          == "Business Management");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("travel_europe")           == "Travel: Europe");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("travel_guidebook_series") == "Travel Guides");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_short")        == "Short Stories");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_essay")        == "Essays");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("biogr_historical")        == "Historical Biographies");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_rus_classsic") == "Russian Classic Literature");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("travel_asia")             == "Travel: Asia");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_history_avant")        == "Alternative History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("people")                  == "People & Society");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_war")          == "War Literature");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_british")      == "British Literature");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("history_europe")          == "European History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_poetry")       == "Poetry");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("biz_economics")           == "Economics");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("biogr_arts")              == "Arts & Culture Biographies");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("outdoors_fauna")          == "Nature & Wildlife");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("history_world")           == "World History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_cyber_punk")           == "Cyberpunk");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("thriller_police")         == "Police Thrillers");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("nonfiction_politics")     == "Politics & Society");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("biogr_leaders")           == "Leadership Biographies");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sport")                   == "Sports");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("biz_accounting")          == "Accounting");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("travel_ex_ussr")          == "Travel: Former USSR");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("science_biolog")          == "Biology");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("romance_historical")      == "Historical Romance");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion_spirituality")   == "Spirituality");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion_occult")         == "Occult");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion_east")           == "Eastern Religions");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion_buddhism")       == "Buddhism");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("nonfiction_folklor")      == "Folklore");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("nonfiction_edu")          == "Education");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("health_self_help")        == "Self-Help & Health");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("history_asia")            == "Asian History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("family_pregnancy")        == "Pregnancy & Parenting");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("history_middle_east")     == "Middle East History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("history_military_science")== "Military History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("history_usa")             == "US History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("horror_occult")           == "Occult Horror");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("family_parenting")        == "Parenting");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_critic")       == "Literary Criticism");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_drama")        == "Drama");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("family_health")           == "Family Health");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("chris_pravoslavie")       == "Orthodox Christianity");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("chris_edu")               == "Christian Education");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("child_9")                 == "Children's");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("family")                  == "Family");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("health_psy")              == "Health Psychology");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("teens_sf")                == "Teen Science Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_fairy")        == "Fairy Tales");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("biogr_arts")              == "Arts & Culture Biographies");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("travel_guidebook_series") == "Travel Guides");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("love_sf")                 == "Romance Science Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("popular_business")        == "Popular Business");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("marketing")               == "Marketing");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("architecture")            == "Architecture");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_philology")           == "Philology");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_writing")              == "Writing");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_economy")             == "Economics");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("economics")               == "Economics");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("management")              == "Management");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_medicine_alternative")== "Alternative Medicine");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("health_sex")              == "Erotica & Sex");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_postapocalyptic")      == "Post-Apocalyptic");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("SF")                      == "Science Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("публицистика")            == "Publicism");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Детская литература")      == "Children's");
}

TEST_CASE("Fb2GenreMapper returns the raw code unchanged for unrecognized input", "[fb2-genre-mapper]")
{
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("custom_genre")    == "custom_genre");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("")                == "");
}

TEST_CASE("Fb2GenreMapper resolution is case-sensitive", "[fb2-genre-mapper]")
{
    // FB2 codes are always lowercase; uppercase variants are not recognised codes
    // Exception: 'SF' is a known non-standard code from lib.rus.ec and IS mapped.
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("SF_SPACE")   == "SF_SPACE");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Adventure")  == "Adventure");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("THRILLER")   == "THRILLER");
}
