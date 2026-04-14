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

TEST_CASE("Fb2GenreMapper resolves lib.rus.ec community codes added from production imports", "[fb2-genre-mapper]")
{
    // High-frequency unmapped codes from real lib.rus.ec import runs
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("network_literature")    == "Network Literature");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("fanfiction")            == "Fanfiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion_budda")        == "Buddhism");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("rel_boddizm")           == "Buddhism");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("histor_military")       == "Military History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("professional_law")      == "Jurisprudence");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("popadanec")             == "Fantasy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("popadancy")             == "Fantasy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("nonfiction_law")        == "Jurisprudence");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("foreign_publicism")     == "Publicism");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_popular")           == "Popular Science");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_litrpg")             == "LitRPG");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_stimpank")           == "Steampunk");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("psy_theraphy")          == "Psychology");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("hronoopera")            == "Time-Travel Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("fantasy_dark")          == "Dark Fantasy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("city_fantasy")          == "Urban Fantasy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("folk_tale")             == "Folk Tales");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("adv_history_avant")     == "Alternative History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("prose_rus_classics")    == "Russian Classic Prose");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("prose_su_classic")      == "Soviet Classic Prose");

    // Cyrillic free-form community tags
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("История")               == "History");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Биографии и мемуары")   == "Biography & Memoirs");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Боевая фантастика")     == "Action Fantasy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Фэнтези")               == "Fantasy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Фентези")               == "Fantasy");
    // Multi-word Cyrillic tag — must not be shadowed by the shorter "Фэнтези" key
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Фэнтези Юмор")         == "Fantasy & Humor");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("языкознание")           == "Linguistics");
}

TEST_CASE("Fb2GenreMapper resolves high-frequency import-log codes (5+ occurrences)", "[fb2-genre-mapper]")
{
    // Prose & narrative
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("story")             == "Short Stories");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("great_story")       == "Novella");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("roman")             == "Novel");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("essay")             == "Essays");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("unfinished")        == "Unfinished Works");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("prose_abs")         == "Absurdist Prose");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("prose_sentimental") == "Sentimental Prose");

    // Poetry, drama & theatre
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("drama")             == "Drama");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("comedy")            == "Comedy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("tragedy")           == "Tragedy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("theatre")           == "Theatre");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("screenplays")       == "Screenplays");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("lyrics")            == "Song Lyrics & Poetry");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("song_poetry")       == "Song Poetry");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("poetry_modern")     == "Modern Poetry");

    // SF & fantasy variants
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_all")            == "Science Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_space_opera")    == "Space Opera");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sf_fantasy_irony")  == "Humorous Fantasy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci-fi")            == "Science Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("fantasy")           == "Fantasy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("dragon_fantasy")    == "Dragon Fantasy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("gothic_novel")      == "Gothic Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("litrpg")            == "LitRPG");

    // Detective & thriller variants
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("det_cozy")          == "Cozy Mysteries");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("det_su")            == "Soviet-Era Detectives");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("det_all")           == "Detectives");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("foreign_detective") == "Foreign Detectives");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("thriller_legal")    == "Legal Thrillers");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("thriller_medical")  == "Medical Thrillers");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("thriller_techno")   == "Techno-Thriller");

    // Romance & adventure
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("love_hard")         == "Erotic Romance");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("love_all")          == "Romance");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("adv_all")           == "Adventure");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("adv_modern")        == "Modern Adventure");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("adv_story")         == "Adventure Stories");

    // Arts
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("visual_arts")       == "Visual Arts");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("comics")            == "Comics & Graphic Novels");

    // Science & education
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_pedagogy")      == "Pedagogy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_cosmos")        == "Astronomy & Cosmology");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_textbook")      == "Textbooks");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_ecology")       == "Ecology");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_zoo")           == "Zoology");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_botany")        == "Botany");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sci_radio")         == "Radio & Electronics");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("tbg_higher")        == "Higher Education");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("tbg_secondary")     == "Secondary Education");

    // Travel
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("geo_guides")        == "Travel Guides");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("travel_notes")      == "Travel Notes");

    // Humor
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("humor_satire")      == "Satire & Humor");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("humor_all")         == "Humor");

    // Young Adult
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("ya")                == "Young Adult");

    // Literature
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("literature_20")     == "20th Century Literature");

    // Military
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("military")          == "Military");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("military_arts")     == "Martial Arts");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("military_weapon")   == "Military Technology");

    // Religion
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion_islam")       == "Islam");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion_catholicism") == "Catholicism");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion_hinduism")    == "Hinduism");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion_protestantism")== "Protestantism");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("religion_all")         == "Religion");

    // Non-Fiction umbrella
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("nonf_all")          == "Non-Fiction");

    // Business
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("accounting")        == "Accounting");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("small_business")    == "Small Business");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("job_hunting")       == "Career & Job Search");

    // Misc
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("astrology")         == "Astrology");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("sagas")             == "Sagas");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("dissident")         == "Dissident Literature");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("folklore")          == "Folklore");

    // Fanfiction community tags
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("POV")               == "Fanfiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("AU")                == "Fanfiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("PWP")               == "Fanfiction");

    // Cyrillic high-frequency tags
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Попаданцы")         == "Isekai / Portal Fantasy");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Юмор")              == "Humor");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Мифические существа")== "Mythical Creatures");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Мистика")           == "Mystic & Supernatural");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Романтика")         == "Romance");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Фантастика")        == "Science Fiction");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Стёб")              == "Parody & Satire");
    REQUIRE(Librova::Fb2Parsing::CFb2GenreMapper::ResolveGenreName("Первый раз")        == "Fanfiction");
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
