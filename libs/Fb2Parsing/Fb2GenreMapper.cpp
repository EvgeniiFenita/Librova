#include "Fb2Parsing/Fb2GenreMapper.hpp"

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
    {"biogr_travel",        "Travel Biography"},
    {"entert_humor",        "Entertainment & Humor"},
    {"magician_book",       "Magic"},
    {"travel_polar",        "Polar Travel"},
    {"history_russia",      "Russian History"},
    {"science_psy",         "Psychology"},
};
// clang-format on

} // namespace

std::string_view CFb2GenreMapper::ResolveGenreName(const std::string_view fb2Code) noexcept
{
    const auto it = GGenreNames.find(fb2Code);
    if (it == GGenreNames.end())
        return fb2Code;
    return it->second;
}

} // namespace Librova::Fb2Parsing
