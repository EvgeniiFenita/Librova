#include "Parsing/Fb2GenreMapper.hpp"

#include <unordered_map>

namespace Librova::Fb2Parsing {
namespace {

// clang-format off
const std::unordered_map<std::string_view, std::string_view> GGenreNames{
    // Science Fiction & Fantasy
    {"sf_history",    "Alternative History"},
    {"sf_action",     "Action SF"},
    {"sf_epic",       "Epic"},
    {"sf_heroic",     "Heroic"},
    {"sf_detective",  "Detective"},
    {"sf_cyberpunk",  "Cyberpunk"},
    {"sf_space",      "Space"},
    {"science_fiction", "Science Fiction"},
    {"sf_social",     "Social-Philosophical"},
    {"sf_horror",     "Horror & Mystic"},
    {"sf_humor",      "Humor"},
    {"sf_fantasy",    "Fantasy"},
    {"sf",            "Science Fiction"},

    // Detectives & Thrillers
    {"det_classic",   "Classical Detectives"},
    {"det_police",    "Police Stories"},
    {"det_action",    "Detective Action"},
    {"det_irony",     "Ironical Detectives"},
    {"det_history",   "Historical Detectives"},
    {"det_espionage", "Espionage"},
    {"det_crime",     "Crime"},
    {"det_political", "Political Detectives"},
    {"det_maniac",    "Maniacs"},
    {"det_hard",      "Hard-Boiled"},
    {"thriller",      "Thrillers"},
    {"detective",     "Detectives"},

    // Prose
    {"prose_classic",      "Classic Prose"},
    {"prose_history",      "Historical Prose"},
    {"prose_contemporary", "Contemporary Prose"},
    {"prose_counter",      "Counterculture"},
    {"prose_rus_classic",  "Russian Classic Prose"},
    {"prose_su_classics",  "Soviet Classic Prose"},

    // Romance
    {"love_contemporary", "Contemporary Romance"},
    {"love_history",      "Historical Romance"},
    {"love_detective",    "Detective Romance"},
    {"love_short",        "Short Romance"},
    {"love_erotica",      "Erotica"},

    // Adventure
    {"adv_western",  "Western"},
    // adv_history and sci_history intentionally share the display name "History".
    // Both codes denote historical content in their respective parent categories;
    // distinguishing them at filter level is not required for MVP and both codes
    // are already listed in GSafeLegacyMigrationCodes for safe v5 migration.
    {"adv_history",  "History"},
    {"adv_indian",   "Indians"},
    {"adv_maritime", "Maritime Fiction"},
    {"adv_geo",      "Travel & Geography"},
    {"adv_animal",   "Nature & Animals"},
    {"adventure",    "Adventure"},

    // Children's
    {"child_tale",      "Fairy Tales"},
    {"child_verse",     "Children's Verses"},
    {"child_prose",     "Children's Prose"},
    {"child_sf",        "Children's Science Fiction"},
    {"child_det",       "Children's Detectives"},
    {"child_adv",       "Children's Adventures"},
    {"child_education", "Educational"},
    {"child_4",         "Children's"},
    {"child_characters","Children's Characters"},
    {"children",        "Children's"},

    // Poetry & Dramaturgy
    {"poetry",     "Poetry"},
    {"dramaturgy", "Dramaturgy"},

    // Antique Literature
    {"antique_ant",      "Antique"},
    {"antique_european", "European Antique"},
    {"antique_russian",  "Old Russian"},
    {"antique_east",     "Old Eastern"},
    {"antique_myths",    "Myths, Legends & Epics"},
    {"antique",          "Antique Literature"},

    // Scientific-Educational
    // sci_history shares the display name "History" with adv_history above.
    // See comment there for the rationale.
    {"sci_history",    "History"},
    {"sci_psychology", "Psychology"},
    {"sci_culture",    "Cultural Studies"},
    {"sci_religion",   "Religious Studies"},
    {"sci_philosophy", "Philosophy"},
    {"sci_politics",   "Politics"},
    {"sci_business",   "Business"},
    {"sci_juris",      "Jurisprudence"},
    {"sci_linguistic", "Linguistics"},
    {"sci_medicine",   "Medicine"},
    {"sci_phys",       "Physics"},
    {"sci_math",       "Mathematics"},
    {"sci_chem",       "Chemistry"},
    {"sci_biology",    "Biology"},
    {"sci_tech",       "Technical"},
    {"science",        "Science"},

    // Computers & Internet
    {"comp_www",         "Internet"},
    {"comp_programming", "Programming"},
    {"comp_hard",        "Hardware"},
    {"comp_soft",        "Software"},
    {"comp_db",          "Databases"},
    {"comp_osnet",       "OS & Networking"},
    {"computers",        "Computers & Internet"},

    // Reference
    {"ref_encyc", "Encyclopedias"},
    {"ref_dict",  "Dictionaries"},
    {"ref_ref",   "Reference"},
    {"ref_guide", "Guidebooks"},
    {"reference", "Reference"},

    // Nonfiction
    {"nonf_biography", "Biography & Memoirs"},
    {"nonf_publicism", "Publicism"},
    {"nonf_criticism", "Criticism"},
    {"design",         "Art & Design"},
    {"nonfiction",     "Non-Fiction"},

    // Religion & Inspiration
    {"religion_rel",       "Religion"},
    {"religion_esoterics", "Esoterics"},
    {"religion_self",      "Self-Improvement"},
    {"religion",           "Religion"},

    // Humor
    {"humor_anecdote", "Anecdotes"},
    {"humor_prose",    "Humorous Prose"},
    {"humor_verse",    "Humorous Verses"},
    {"humor",          "Humor"},

    // Home & Family
    {"home_cooking",   "Cooking"},
    {"home_pets",      "Pets"},
    {"home_crafts",    "Hobbies & Crafts"},
    {"home_entertain", "Entertaining"},
    {"home_health",    "Health"},
    {"home_garden",    "Garden"},
    {"home_diy",       "Do It Yourself"},
    {"home_sport",     "Sports"},
    {"home_sex",       "Erotica & Sex"},
    {"home",           "Home & Family"},

    // Intentional alias groups — multiple codes represent the same concept.
    // Both members map to the same display name.
    //
    //   romance / love          → "Romance"
    //   sf / sf_etc             → "Science Fiction"
    //   sci_psychology /
    //     science_psy           → "Psychology"
    //   religion_rel / religion → "Religion"
    //   ref_ref / reference     → "Reference"
    //
    // adv_history and sci_history are both intentionally mapped to "History" —
    // they represent the same genre concept in different FB2 sub-category groups.
    //
    {"biography",           "Biography"},
    {"horror",              "Horror"},
    {"mystery",             "Mystery"},
    {"romance",             "Romance"},
    {"love",                "Romance"},
    {"prose",               "Prose"},
    {"other",               "Other"},
    {"literature",          "Literature"},
    {"literature_classics", "Classic Literature"},
    {"literature_su_classics", "Soviet Literature"},
    {"foreign_sf",          "Foreign Science Fiction"},
    {"foreign_fantasy",     "Foreign Fantasy"},
    {"historical_fantasy",  "Historical Fantasy"},
    {"fantasy_alt_hist",    "Alternative History Fantasy"},
    {"fantasy_fight",       "Action Fantasy"},
    {"romance_fantasy",     "Fantasy Romance"},
    {"romance_sf",          "Science Fiction Romance"},
    {"thriller_mystery",    "Mystery Thriller"},
    {"sf_etc",              "Science Fiction"},
    {"sf_fantasy_city",     "Urban Fantasy"},
    {"prose_military",      "Military Prose"},
    {"prose_root",          "Prose"},
    {"proce",               "Prose"},
    // All biogr_* codes collapse to a single genre filter entry (#122)
    {"biogr_travel",           "Biography & Memoirs"},
    {"biogr_professionals",    "Biography & Memoirs"},
    {"biogr_historical",       "Biography & Memoirs"},
    {"biogr_arts",             "Biography & Memoirs"},
    {"biogr_leaders",          "Biography & Memoirs"},
    {"entert_humor",           "Entertainment & Humor"},
    {"magician_book",          "Magic"},
    // Regional travel variants → single "Travel" filter entry; guidebooks stay distinct (#122)
    {"travel_polar",           "Travel"},
    {"travel_europe",          "Travel"},
    {"travel_asia",            "Travel"},
    {"travel_ex_ussr",         "Travel"},
    {"travel_guidebook_series","Travel Guides"},
    // Regional history variants → "History"; military history is thematically distinct (#122)
    {"history_russia",         "History"},
    {"history_europe",         "History"},
    {"history_world",          "History"},
    {"history_asia",           "History"},
    {"history_middle_east",    "History"},
    {"history_military_science","Military History"},
    {"history_usa",            "History"},
    {"science_psy",            "Psychology"},
    {"science_biolog",         "Biology"},
    {"literature_adv",         "Adventure Literature"},
    {"literature_history",     "Historical Fiction"},
    {"literature_short",       "Short Stories"},
    {"literature_essay",       "Essays"},
    {"literature_war",         "War Literature"},
    {"literature_british",     "British Literature"},
    {"literature_poetry",      "Poetry"},
    {"literature_fairy",       "Fairy Tales"},
    {"literature_critic",      "Literary Criticism"},
    {"literature_drama",       "Drama"},
    {"literature_rus_classsic","Russian Classic Literature"},
    {"biz_management",         "Business Management"},
    {"biz_economics",          "Economics"},
    {"biz_accounting",         "Accounting"},
    {"nonfiction_politics",    "Politics & Society"},
    {"nonfiction_folklor",     "Folklore"},
    {"nonfiction_edu",         "Education"},
    {"outdoors_fauna",         "Nature & Wildlife"},
    {"health_self_help",       "Self-Help & Health"},
    {"health_psy",             "Health Psychology"},
    {"family",                 "Family"},
    {"family_pregnancy",       "Pregnancy & Parenting"},
    {"family_parenting",       "Parenting"},
    {"family_health",          "Family Health"},
    {"people",                 "People & Society"},
    {"sport",                  "Sports"},
    {"teens_sf",               "Teen Science Fiction"},
    {"sf_cyber_punk",          "Cyberpunk"},
    {"sf_history_avant",       "Alternative History"},
    {"thriller_police",        "Police Thrillers"},
    {"romance_historical",     "Historical Romance"},
    {"horror_occult",          "Occult Horror"},
    {"religion_spirituality",  "Spirituality"},
    {"religion_occult",        "Occult"},
    {"religion_east",          "Eastern Religions"},
    {"religion_buddhism",      "Buddhism"},
    {"chris_pravoslavie",      "Orthodox Christianity"},
    {"chris_edu",              "Christian Education"},
    {"child_9",                "Children's"},

    // Romance
    {"love_sf",                "Romance Science Fiction"},

    // Business & Economics
    {"popular_business",       "Popular Business"},
    {"marketing",              "Marketing"},
    {"sci_economy",            "Economics"},
    {"economics",              "Economics"},
    {"management",             "Management"},

    // Health & Alternative Medicine
    {"sci_medicine_alternative", "Alternative Medicine"},
    {"health_sex",             "Erotica & Sex"},

    // Science Fiction variants
    {"sf_postapocalyptic",     "Post-Apocalyptic"},

    // Non-standard / Russian-language codes from lib.rus.ec
    {"SF",                     "Science Fiction"},
    {"публицистика",           "Publicism"},
    {"Детская литература",     "Children's"},

    // Arts & Architecture
    {"architecture",           "Architecture"},

    // Linguistics
    {"sci_philology",          "Philology"},

    // Creative Writing
    {"sf_writing",             "Writing"},

    // Additional lib.rus.ec community codes — high-frequency unmapped codes observed in production imports
    {"network_literature",        "Network Literature"},
    {"fanfiction",                "Fanfiction"},
    {"literature_world",          "World Literature"},
    {"religion_budda",            "Buddhism"},              // common misspelling of religion_buddhism
    {"rel_boddizm",               "Buddhism"},              // alternate misspelling
    {"histor_military",           "Military History"},
    {"child_animals",             "Nature & Animals"},
    {"professional_law",          "Jurisprudence"},
    {"popadanec",                 "Fantasy"},
    {"popadancy",                 "Fantasy"},               // alternate spelling
    {"nonfiction_law",            "Jurisprudence"},
    {"outdoors_nature_writing",   "Nature & Wildlife"},
    {"foreign_publicism",         "Publicism"},
    {"teens_history",             "Children's History"},
    {"travel_spec",               "Travel"},
    {"nonfiction_philosophy",     "Philosophy"},
    {"travel_africa",             "Travel"},
    {"romance_multicultural",     "Romance"},
    {"psy_personal",              "Psychology"},
    {"foreign_religion",          "Religion"},
    {"sci_popular",               "Popular Science"},
    {"foreign_children",          "Children's"},
    {"literature_political",      "Political Fiction"},
    {"humor_fantasy",             "Humor"},
    {"literature_western",        "Western"},
    {"sci_transport",             "Transportation & Technology"},
    {"travel_lat_am",             "Travel"},
    {"religion_orthodoxy",        "Orthodox Christianity"},
    {"upbringing_book",           "Parenting & Upbringing"},
    {"pedagogy_book",             "Pedagogy"},
    {"foreign_edu",               "Education"},
    {"horror_fantasy",            "Horror"},
    {"teens_literature",          "Teen Literature"},
    {"love_fantasy",              "Fantasy Romance"},
    {"russian_contemporary",      "Contemporary Russian Fiction"},
    {"periodic",                  "Periodicals"},
    {"personal_finance",          "Personal Finance"},
    {"psy_sex_and_family",        "Psychology"},
    {"psy_social",                "Social Psychology"},
    {"health_rel",                "Health"},
    {"prose_magic",               "Fantasy"},
    {"psy_theraphy",              "Psychology"},            // misspelling of "therapy"
    {"psy_childs",                "Child Psychology"},
    {"psy_generic",               "Psychology"},
    {"industries",                "Industries"},
    {"military_history",          "Military History"},
    {"military_special",          "Military"},
    {"health_nutrition",          "Nutrition & Health"},
    {"health_women",              "Health"},
    {"health_men",                "Health"},
    {"health_alt_medicine",       "Alternative Medicine"},
    {"romance_contemporary",      "Contemporary Romance"},
    {"romance_romantic_suspense", "Romantic Suspense"},
    {"travel",                    "Travel"},
    {"sf_litrpg",                 "LitRPG"},
    {"sf_realrpg",                "LitRPG"},
    {"sf_technofantasy",          "Science Fantasy"},
    {"sf_irony",                  "Humorous SF"},
    {"sf_mystic",                 "Mystic SF"},
    {"sf_stimpank",               "Steampunk"},
    {"biz_beogr",                 "Biography & Memoirs"},  // business biographies → same filter entry (#122)
    {"biz_life",                  "Business & Lifestyle"},
    {"biogr_sports",              "Biography & Memoirs"},  // sports biographies → same filter entry (#122)
    {"science_earth",             "Earth Sciences"},
    {"science_medicine",          "Medicine"},
    {"science_archaeology",       "Archaeology"},
    {"science_history_philosophy","History & Philosophy of Science"},
    {"religion_christianity",     "Christianity"},
    {"chris_fiction",             "Christian Fiction"},
    {"chris_orthodoxy",           "Orthodox Christianity"},
    {"sci_social_studies",        "Social Studies"},
    {"sci_state",                 "Political Science"},
    {"sci_geo",                   "Geography"},
    {"sci_build",                 "Construction & Architecture"},
    {"city_fantasy",              "Urban Fantasy"},
    {"women_single",              "Fiction"},
    {"epistolary_fiction",        "Epistolary Fiction"},
    {"outdoors_hunt_fish",        "Hunting & Fishing"},
    {"outdoors_travel",           "Travel"},
    {"outdoors_conservation",     "Conservation"},
    {"folk_tale",                 "Folk Tales"},
    {"short_story",               "Short Stories"},
    {"thriller_psychology",       "Psychological Thriller"},
    {"teens_social",              "Teen Fiction"},
    {"teens_health",              "Health"},
    {"russian_fantasy",           "Russian Fantasy"},
    {"prose_rus_classics",        "Russian Classic Prose"},  // variant of prose_rus_classic
    {"prose_su_classic",          "Soviet Classic Prose"},   // variant of prose_su_classics
    {"literature_religion",       "Religious Fiction"},
    {"literature_erotica",        "Erotica"},
    {"literature_books",          "Literature"},
    {"literature_antology",       "Anthology"},
    {"literature_19",             "Literature"},  // century sub-genres → single filter entry (#122)
    {"literature_18",             "Literature"},
    {"literature_sea",            "Maritime Fiction"},
    {"literature_usa",            "American Literature"},
    {"literature_women",          "Women's Fiction"},
    {"literature_men_advent",     "Adventure Literature"},
    {"cine",                      "Cinema"},
    {"cinema_theatre",            "Cinema & Theatre"},
    {"child_people",              "Children's"},
    {"child_history",             "Children's History"},
    {"boyar_anime",               "Anime-style Fiction"},
    {"banking",                   "Finance & Banking"},
    {"stock",                     "Finance & Investing"},
    {"art_criticism",             "Art Criticism"},
    {"art",                       "Art"},
    {"architecture_book",         "Architecture"},
    {"aphorisms",                 "Aphorisms"},
    {"aphorism_quote",            "Aphorisms"},
    {"adventure_fantasy",         "Adventure Fantasy"},
    {"fantasy_dark",              "Dark Fantasy"},
    {"adv_history_avant",         "Alternative History"},
    {"cooking",                   "Cooking"},
    {"fairy_fantasy",             "Fantasy"},
    {"family_relations",          "Family"},
    {"nonfiction_true_accounts",  "Non-Fiction"},
    {"nonfiction_crime",          "Crime Non-Fiction"},
    {"nonf_military",             "Military Non-Fiction"},
    {"narrative",                 "Prose"},
    {"music",                     "Music"},
    {"vampire_book",              "Horror"},
    {"horror_vampires",           "Horror"},
    {"org_behavior",              "Organizational Behavior"},
    {"hronoopera",                "Time-Travel Fiction"},
    {"history_australia",         "History"},
    {"history_ancient",           "Ancient History"},
    {"foreign_prose",             "Foreign Prose"},
    {"foreign_love",              "Foreign Romance"},
    {"foreign_contemporary",      "Contemporary Foreign Fiction"},
    {"foreign_business",          "Foreign Business"},
    {"foreign_antique",           "Antique Literature"},
    {"foreign_adventure",         "Adventure"},
    {"performance",               "Performing Arts"},

    // Cyrillic free-form genre tags from lib.rus.ec community tagging
    {"История",                   "History"},
    {"Биографии и мемуары",       "Biography & Memoirs"},
    {"Боевая фантастика",         "Action Fantasy"},
    {"Иронический детектив",      "Ironical Detectives"},
    {"Фэнтези Юмор",              "Fantasy & Humor"},
    {"Фэнтези",                   "Fantasy"},
    {"Фентези",                   "Fantasy"},
    {"Сказка",                    "Fairy Tales"},
    {"религиозная литература",    "Religious Literature"},
    {"Публицистика",              "Publicism"},
    {"Психология",                "Psychology"},
    {"Приключения",               "Adventure"},
    {"Космическая фантастика",    "Space Fiction"},
    {"Историческая проза",        "Historical Prose"},
    {"Интернет",                  "Internet"},
    {"Эзотерика",                 "Esoterics"},
    {"Шпионский Детектив",        "Espionage"},
    {"языкознание",               "Linguistics"},
    {"Проза",                     "Prose"},

    // Codes seen ≥ 5 times in import logs that had no prior mapping
    // Prose & Narrative
    {"story",               "Short Stories"},
    {"great_story",         "Novella"},
    {"roman",               "Novel"},
    {"essay",               "Essays"},
    {"unfinished",          "Unfinished Works"},
    {"prose_abs",           "Absurdist Prose"},
    {"prose_sentimental",   "Sentimental Prose"},
    {"prose_game",          "Game-Inspired Prose"},
    {"modern_tale",         "Modern Fairy Tales"},
    {"notes",               "Notes & Essays"},

    // Poetry, Drama & Theatre
    {"drama",               "Drama"},
    {"comedy",              "Comedy"},
    {"tragedy",             "Tragedy"},
    {"theatre",             "Theatre"},
    {"screenplays",         "Screenplays"},
    {"lyrics",              "Song Lyrics & Poetry"},
    {"song_poetry",         "Song Poetry"},
    {"poetry_modern",       "Modern Poetry"},
    {"folk_songs",          "Folk Songs"},

    // Science Fiction variants
    {"sf_all",              "Science Fiction"},
    {"sf_space_opera",      "Space Opera"},
    {"sf_fantasy_irony",    "Humorous Fantasy"},
    {"sci-fi",              "Science Fiction"},
    {"nsf",                 "Science Fiction"},
    {"fantasy",             "Fantasy"},
    {"dragon_fantasy",      "Dragon Fantasy"},
    {"gothic_novel",        "Gothic Fiction"},
    {"litrpg",              "LitRPG"},

    // Detective & Thriller variants
    {"det_cozy",            "Cozy Mysteries"},
    {"det_su",              "Soviet-Era Detectives"},
    {"det_all",             "Detectives"},
    {"foreign_detective",   "Foreign Detectives"},
    {"thriller_legal",      "Legal Thrillers"},
    {"thriller_medical",    "Medical Thrillers"},
    {"thriller_techno",     "Techno-Thriller"},

    // Romance variants
    {"love_hard",           "Erotic Romance"},
    {"love_all",            "Romance"},

    // Adventure variants
    {"adv_all",             "Adventure"},
    {"adv_modern",          "Modern Adventure"},
    {"adv_story",           "Adventure Stories"},

    // Arts & Visual
    {"visual_arts",         "Visual Arts"},
    {"painting",            "Painting & Visual Arts"},
    {"art_world_culture",   "World Arts & Culture"},
    {"comics",              "Comics & Graphic Novels"},

    // Science & Education
    {"sci_pedagogy",        "Pedagogy"},
    {"sci_cosmos",          "Astronomy & Cosmology"},
    {"sci_textbook",        "Textbooks"},
    {"sci_theories",        "Scientific Theories"},
    {"sci_ecology",         "Ecology"},
    {"sci_zoo",             "Zoology"},
    {"sci_botany",          "Botany"},
    {"sci_radio",           "Radio & Electronics"},
    {"sci_oriental",        "Oriental Studies"},
    {"foreign_psychology",  "Psychology"},
    {"foreign_language",    "Foreign Languages"},
    {"tbg_higher",          "Higher Education"},
    {"tbg_secondary",       "Secondary Education"},
    {"equ_history",         "History"},

    // Travel & Geography
    {"geo_guides",          "Travel Guides"},
    {"travel_notes",        "Travel Notes"},

    // Humor
    {"humor_satire",        "Satire & Humor"},
    {"humor_all",           "Humor"},

    // Young Adult
    {"ya",                  "Young Adult"},

    // Literature by period — century suffixes add clutter; all collapse to "Literature" (#122)
    {"literature_20",       "Literature"},

    // Military
    {"military",            "Military"},
    {"military_arts",       "Martial Arts"},
    {"military_weapon",     "Military Technology"},

    // Religion variants
    {"religion_islam",      "Islam"},
    {"religion_catholicism","Catholicism"},
    {"religion_hinduism",   "Hinduism"},
    {"religion_protestantism","Protestantism"},
    {"religion_all",        "Religion"},

    // Non-Fiction umbrella
    {"nonf_all",            "Non-Fiction"},

    // Business & Finance
    {"accounting",          "Accounting"},
    {"small_business",      "Small Business"},
    {"job_hunting",         "Career & Job Search"},

    // Esotericism
    {"astrology",           "Astrology"},

    // Home & Foreign
    {"foreign_home",        "Home & Family"},

    // Historical fiction / sagas
    {"sagas",               "Sagas"},
    {"dissident",           "Dissident Literature"},

    // Fanfiction genre tags (community-specific)
    {"POV",                 "Fanfiction"},
    {"AU",                  "Fanfiction"},
    {"PWP",                 "Fanfiction"},
    {"Первый раз",          "Fanfiction"},

    // Cyrillic community free-form tags (≥ 5 occurrences)
    {"Попаданцы",           "Isekai / Portal Fantasy"},
    {"Юмор",                "Humor"},
    {"Мифические существа", "Mythical Creatures"},
    {"Мистика",             "Mystic & Supernatural"},
    {"Романтика",           "Romance"},
    {"Фантастика",          "Science Fiction"},
    {"Стёб",                "Parody & Satire"},
    {"folklore",            "Folklore"},

    // High-frequency codes from 2026-04 production import run (task #150)
    // Cyrillic religion aliases
    {"Православие",         "Orthodox Christianity"},   // 48 occurrences
    {"Христианство",        "Christianity"},            // 4 occurrences

    // Business & economics
    {"paper_work",          "Office & Administration"}, // 21 occurrences
    {"global_economy",      "Economics"},               // 6 occurrences
    {"economics_ref",       "Economics"},               // 4 occurrences

    // Real estate / property
    {"real_estate",         "Real Estate"},             // 6 occurrences

    // Religion variants
    {"religion_judaism",    "Judaism"},                 // 5 occurrences
};
// clang-format on

} // namespace

std::string_view CFb2GenreMapper::ResolveGenreName(const std::string_view fb2Code) noexcept
{
    // Fast path: exact match
    if (const auto it = GGenreNames.find(fb2Code); it != GGenreNames.end())
        return it->second;

    // Normalization: trim leading/trailing ASCII whitespace
    const auto ltrim = fb2Code.find_first_not_of(" \t\r\n");
    if (ltrim == std::string_view::npos)
        return {}; // all-whitespace → treat as empty

    const auto rtrim = fb2Code.find_last_not_of(" \t\r\n");
    const std::string_view trimmed = fb2Code.substr(ltrim, rtrim - ltrim + 1);
    if (trimmed.size() != fb2Code.size())
    {
        if (const auto it = GGenreNames.find(trimmed); it != GGenreNames.end())
            return it->second;
    }

    // Normalization: mixed-language label "ascii_code CyrillicWord" — try prefix before first space.
    // Handles real-world values such as "sci_history История".
    if (const auto spacePos = trimmed.find(' '); spacePos != std::string_view::npos)
    {
        const std::string_view prefix = trimmed.substr(0, spacePos);
        if (!prefix.empty())
        {
            if (const auto it = GGenreNames.find(prefix); it != GGenreNames.end())
                return it->second;
        }
    }

    // Normalization: ASCII code with appended non-ASCII bytes (mojibake suffix).
    // Handles values such as "accountingВухучет" where the real code is "accounting".
    // Find first byte ≥ 0x80 (start of any multi-byte UTF-8 sequence).
    std::size_t nonAsciiPos = 0;
    while (nonAsciiPos < trimmed.size() && static_cast<unsigned char>(trimmed[nonAsciiPos]) < 0x80)
        ++nonAsciiPos;
    if (nonAsciiPos > 0 && nonAsciiPos < trimmed.size())
    {
        const std::string_view asciiPrefix = trimmed.substr(0, nonAsciiPos);
        if (const auto it = GGenreNames.find(asciiPrefix); it != GGenreNames.end())
            return it->second;
    }

    return fb2Code;
}

} // namespace Librova::Fb2Parsing
