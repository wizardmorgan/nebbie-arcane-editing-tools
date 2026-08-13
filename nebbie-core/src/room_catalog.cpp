#include "nebbie/room_catalog.hpp"

namespace nebbie {

namespace {

constexpr long kRoomFlagTunnel = 64;
constexpr int kSectorWaterNoSwim = 7;
constexpr int kSectorUnderwaterNebbie = 8;

const char* kSectorTypes[] = {
    "Inside", "City", "Field", "Forest", "Hills", "Mountains", "Water Swim",
    "Water NoSwim", "Air", "Underwater", "Desert", "Tree", "Dark City",
};

const char* kRoomBits[] = {
    "DARK", "DEATH", "NO_MOB", "INDOORS", "PEACEFUL", "NOSTEAL", "NO_SUM", "NO_MAGIC",
    "TUNNEL", "PRIVATE", "SILENCE", "LARGE", "NO_DEATH", "SAVE_ROOM", "NO_TRACK", "NO_MIND",
    "DESERTIC", "ARTIC", "UNDERGROUND", "HOT", "WET", "COLD", "DRY", "BRIGHT",
    "NO_ASTRAL", "NO_REGAIN", "RESERVED", "RESERVED", "RESERVED", "RESERVED", "RESERVED",
    "RESERVED",
};

const char* kExitDirs[] = {
    "north", "east", "south", "west", "up", "down",
};

std::vector<MobFlagDef> make_bit_flags(const char* const* names, std::size_t count) {
    std::vector<MobFlagDef> defs;
    defs.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        MobFlagDef def;
        def.value = 1L << i;
        def.name = names[i];
        def.label = names[i];
        defs.push_back(def);
    }
    return defs;
}

} // namespace

std::vector<std::pair<int, std::string>> room_sector_choices() {
    std::vector<std::pair<int, std::string>> choices;
    const std::size_t count = sizeof(kSectorTypes) / sizeof(kSectorTypes[0]);
    for (std::size_t i = 0; i < count; ++i) {
        choices.emplace_back(static_cast<int>(i), kSectorTypes[i]);
    }
    return choices;
}

std::vector<MobFlagDef> room_flag_defs() {
    return make_bit_flags(kRoomBits, sizeof(kRoomBits) / sizeof(kRoomBits[0]));
}

std::vector<MobFlagDef> exit_flag_defs() {
    // Names match NebbieArcane exit_bits[] / autoenums.hpp; labels are editor UI only.
    return {
        {1L << 0, "ISDOOR", "ISDOOR"},
        {1L << 1, "CLOSED", "CLOSED"},
        {1L << 2, "LOCKED", "LOCKED"},
        {1L << 3, "SECRECT", "SECRECT"},
        {1L << 4, "NOTBASH", "NOTBASH"},
        {1L << 5, "PICKPROOF", "PICKPROOF"},
        {1L << 6, "CLIMB", "CLIMB"},
        {1L << 7,
         "EX_MALE: articolo maschile italiano per la keyword (il/la, aperto/aperta)",
         "MASC"},
        {1L << 8, "NOLOOKT", "NOLOOKT"},
    };
}

std::vector<std::pair<int, std::string>> exit_direction_choices() {
    std::vector<std::pair<int, std::string>> choices;
    const std::size_t count = sizeof(kExitDirs) / sizeof(kExitDirs[0]);
    for (std::size_t i = 0; i < count; ++i) {
        choices.emplace_back(static_cast<int>(i), kExitDirs[i]);
    }
    return choices;
}

std::string room_sector_name(const int sector_type) {
    const std::size_t count = sizeof(kSectorTypes) / sizeof(kSectorTypes[0]);
    if (sector_type >= 0 && static_cast<std::size_t>(sector_type) < count) {
        return kSectorTypes[sector_type];
    }
    return "Sector #" + std::to_string(sector_type);
}

std::string exit_direction_label(const int direction) {
    const std::size_t count = sizeof(kExitDirs) / sizeof(kExitDirs[0]);
    if (direction >= 0 && static_cast<std::size_t>(direction) < count) {
        return kExitDirs[direction];
    }
    return "dir #" + std::to_string(direction);
}

bool room_sector_uses_river(const int sector_type) {
    return sector_type == kSectorWaterNoSwim || sector_type == kSectorUnderwaterNebbie;
}

bool room_flags_use_moblim(const long room_flags) {
    return (room_flags & kRoomFlagTunnel) != 0;
}

} // namespace nebbie
