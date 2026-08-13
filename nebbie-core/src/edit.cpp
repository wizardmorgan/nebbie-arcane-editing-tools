#include "nebbie/edit.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

namespace nebbie {

namespace {

constexpr const char* EXIT_NAMES[EXIT_DIR_COUNT] = {
    "nord", "est", "sud", "ovest", "su", "giu",
};

long parse_long(const std::string& value, const char* field) {
    try {
        return std::stol(value);
    } catch (const std::exception&) {
        throw std::runtime_error(std::string("invalid value for ") + field + ": " + value);
    }
}

int parse_int(const std::string& value, const char* field) {
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        throw std::runtime_error(std::string("invalid value for ") + field + ": " + value);
    }
}

std::string trim_copy(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size()
           && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(start, end - start);
}

bool strings_equal_ci(const std::string& left, const std::string& right) {
    const std::string a = trim_copy(left);
    const std::string b = trim_copy(right);
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i]))
            != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

template <typename Map>
long max_vnum_in_map(const Map& entities) {
    long max_vnum = 0;
    for (const auto& [vnum, _] : entities) {
        max_vnum = std::max(max_vnum, vnum);
    }
    return max_vnum;
}

long first_zone_bottom(const World& world) {
    if (world.zones.empty()) {
        return 1;
    }
    return world.zones.front().bottom;
}

int zone_index_for_vnum(const World& world, long vnum) {
    if (const Zone* zone = world.zone_for_vnum(vnum)) {
        for (size_t i = 0; i < world.zones.size(); ++i) {
            if (&world.zones[i] == zone) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

Mobile make_default_mobile(long vnum, const MobEdit& edit) {
    Mobile mob;
    mob.vnum = vnum;
    mob.name = "nuovo mob";
    mob.short_descr = edit.short_descr.empty() ? "un nuovo mob" : edit.short_descr;
    mob.long_descr = edit.long_descr.empty() ? "Un nuovo mob e' qui." : edit.long_descr;
    mob.description = edit.description.empty() ? "Non vedi nulla di speciale." : edit.description;
    mob.act = 1;
    mob.affected_by = 0;
    mob.alignment = edit.alignment > -999999 ? edit.alignment : 0;
    mob.mobtype = 'S';
    mob.mult_att = 1;
    mob.level = edit.level >= 0 ? edit.level : 1;
    mob.hitroll = 0;
    mob.ac = 10;
    mob.hit_dice = "1d1+0";
    mob.dam_dice = "1d4+0";
    mob.gold = 0;
    mob.exp = 0;
    mob.position = 8;
    mob.default_pos = 8;
    mob.sex = 0;
    return mob;
}

GameObject make_default_object(long vnum, const ObjEdit& edit) {
    GameObject obj;
    obj.vnum = vnum;
    obj.name = "oggetto nuovo";
    obj.short_descr = edit.short_descr.empty() ? "un oggetto nuovo" : edit.short_descr;
    obj.description = edit.description.empty() ? "Un oggetto nuovo e' qui." : edit.description;
    obj.action_description.clear();
    obj.type_flag = 1;
    obj.extra_flags = 0;
    obj.wear_flags = 1;
    obj.weight = edit.weight >= 0 ? edit.weight : 1;
    obj.cost = edit.cost >= 0 ? edit.cost : 0;
    obj.cost_per_day = 0;
    return obj;
}

bool room_in_zone(long vnum, const Zone& zone) {
    return vnum >= zone.bottom && vnum <= zone.top;
}

long first_room_in_zone(const Zone& zone, const World& world) {
    for (const auto& [vnum, _] : world.rooms) {
        if (room_in_zone(vnum, zone)) {
            return vnum;
        }
    }
    return zone.bottom > 0 ? zone.bottom : 1;
}

long first_mobile_vnum(const World& world) {
    return world.mobiles.empty() ? 1 : world.mobiles.begin()->first;
}

long first_object_vnum(const World& world) {
    return world.objects.empty() ? 1 : world.objects.begin()->first;
}

} // namespace

const char* exit_direction_name(int direction) {
    if (direction < 0 || direction >= EXIT_DIR_COUNT) {
        return "?";
    }
    return EXIT_NAMES[direction];
}

int exit_direction_from_name(const std::string& name) {
    std::string lowered;
    lowered.reserve(name.size());
    for (char c : name) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    for (int i = 0; i < EXIT_DIR_COUNT; ++i) {
        if (lowered == EXIT_NAMES[i]) {
            return i;
        }
    }
    return -1;
}

bool entity_matches(long vnum, const std::string& name, const std::string& query) {
    if (query.empty()) {
        return true;
    }

    auto contains_insensitive = [](const std::string& haystack, const std::string& needle) {
        if (needle.empty()) {
            return true;
        }
        auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
        std::string hay;
        hay.reserve(haystack.size());
        for (char c : haystack) {
            hay.push_back(lower(static_cast<unsigned char>(c)));
        }
        std::string ned;
        ned.reserve(needle.size());
        for (char c : needle) {
            ned.push_back(lower(static_cast<unsigned char>(c)));
        }
        return hay.find(ned) != std::string::npos;
    };

    return contains_insensitive(std::to_string(vnum), query)
           || contains_insensitive(name, query);
}

long suggest_next_room_vnum(const World& world) {
    const long max_vnum = max_vnum_in_map(world.rooms);
    if (max_vnum > 0) {
        return max_vnum + 1;
    }
    return first_zone_bottom(world);
}

long suggest_next_mob_vnum(const World& world) {
    const long max_vnum = max_vnum_in_map(world.mobiles);
    return max_vnum > 0 ? max_vnum + 1 : 1;
}

long suggest_next_object_vnum(const World& world) {
    const long max_vnum = max_vnum_in_map(world.objects);
    return max_vnum > 0 ? max_vnum + 1 : 1;
}

void apply_room_edit(Room& room, const RoomEdit& edit) {
    if (!edit.name.empty()) {
        room.name = edit.name;
    }
    if (!edit.description.empty()) {
        room.description = edit.description;
    }
    if (edit.sector_type >= 0) {
        room.sector_type = edit.sector_type;
    }
    if (edit.room_flags >= 0) {
        room.room_flags = edit.room_flags;
    }
    if (edit.tele_time >= 0) {
        room.tele_time = edit.tele_time;
    }
    if (edit.tele_targ >= 0) {
        room.tele_targ = edit.tele_targ;
    }
    if (edit.tele_mask >= 0) {
        room.tele_mask = edit.tele_mask;
    }
    if (edit.tele_cnt >= 0) {
        room.tele_cnt = edit.tele_cnt;
    }
    if (edit.river_speed >= 0) {
        room.river_speed = edit.river_speed;
    }
    if (edit.river_dir >= 0) {
        room.river_dir = edit.river_dir;
    }
    if (edit.moblim >= 0) {
        room.moblim = edit.moblim;
    }
    if (edit.bright_set) {
        room.bright_at_night = edit.bright_at_night;
        room.bright_at_day = edit.bright_at_day;
    }
}

void assign_room_fields(Room& room, const Room& values) {
    room.name = values.name;
    room.description = values.description;
    room.room_flags = values.room_flags;
    room.sector_type = values.sector_type;
    room.tele_time = values.tele_time;
    room.tele_targ = values.tele_targ;
    room.tele_mask = values.tele_mask;
    room.tele_cnt = values.tele_cnt;
    room.river_speed = values.river_speed;
    room.river_dir = values.river_dir;
    room.moblim = values.moblim;
    room.bright_at_night = values.bright_at_night;
    room.bright_at_day = values.bright_at_day;
    room.extra_descs = values.extra_descs;
    room.exits = values.exits;
}

void assign_zone_fields(Zone& zone, const Zone& values) {
    zone.name = values.name;
    zone.top = values.top;
    zone.lifespan = values.lifespan;
    zone.reset_mode = values.reset_mode;
}

void recompute_zone_bottoms(World& world) {
    for (std::size_t i = 0; i < world.zones.size(); ++i) {
        world.zones[i].bottom = (i == 0) ? 0 : world.zones[i - 1].top + 1;
    }
}

void apply_mob_edit(Mobile& mob, const MobEdit& edit) {
    if (!edit.name.empty()) {
        mob.name = edit.name;
    }
    if (!edit.short_descr.empty()) {
        mob.short_descr = edit.short_descr;
    }
    if (!edit.long_descr.empty()) {
        mob.long_descr = edit.long_descr;
    }
    if (!edit.description.empty()) {
        mob.description = edit.description;
    }
    if (edit.level >= 0) {
        mob.level = edit.level;
    }
    if (edit.alignment > -999999) {
        mob.alignment = edit.alignment;
    }
    if (edit.act >= 0) {
        mob.act = edit.act;
    }
    if (edit.affected_by >= 0) {
        mob.affected_by = edit.affected_by;
    }
    if (edit.mobtype != '\0') {
        mob.mobtype = edit.mobtype;
    }
    if (edit.mult_att >= 0) {
        mob.mult_att = edit.mult_att;
    }
    if (edit.hitroll > -999999) {
        mob.hitroll = edit.hitroll;
    }
    if (edit.ac > -999999) {
        mob.ac = edit.ac;
    }
    if (edit.hit_bonus > -999999) {
        mob.hit_bonus = edit.hit_bonus;
    }
    if (!edit.hit_dice.empty()) {
        mob.hit_dice = edit.hit_dice;
    }
    if (!edit.dam_dice.empty()) {
        mob.dam_dice = edit.dam_dice;
    }
    if (edit.extended_gold_set) {
        mob.extended_gold = edit.extended_gold;
    }
    if (edit.gold >= 0) {
        mob.gold = edit.gold;
    }
    if (edit.exp >= 0) {
        mob.exp = edit.exp;
    }
    if (edit.race >= 0) {
        mob.race = edit.race;
    }
    if (edit.position >= 0) {
        mob.position = edit.position;
    }
    if (edit.default_pos >= 0) {
        mob.default_pos = edit.default_pos;
    }
    if (edit.sex >= 0) {
        mob.sex = edit.sex;
    }
    if (edit.extended_sex_set) {
        mob.extended_sex = edit.extended_sex;
    }
    if (edit.immune >= 0) {
        mob.immune = edit.immune;
    }
    if (edit.meta_immune >= 0) {
        mob.meta_immune = edit.meta_immune;
    }
    if (edit.susceptible >= 0) {
        mob.susceptible = edit.susceptible;
    }
    if (edit.sounds_set) {
        mob.sounds = edit.sounds;
    }
    if (edit.distant_sounds_set) {
        mob.distant_sounds = edit.distant_sounds;
    }
}

void assign_mobile_fields(Mobile& mob, const Mobile& values) {
    mob.name = values.name;
    mob.short_descr = values.short_descr;
    mob.long_descr = values.long_descr;
    mob.description = values.description;
    mob.act = values.act;
    mob.affected_by = values.affected_by;
    mob.alignment = values.alignment;
    mob.mobtype = values.mobtype;
    mob.mult_att = values.mult_att;
    mob.level = values.level;
    mob.hitroll = values.hitroll;
    mob.ac = values.ac;
    mob.hit_bonus = values.hit_bonus;
    mob.hit_dice = values.hit_dice;
    mob.dam_dice = values.dam_dice;
    mob.extended_gold = values.extended_gold;
    mob.gold = values.gold;
    mob.exp = values.exp;
    mob.race = values.race;
    mob.position = values.position;
    mob.default_pos = values.default_pos;
    mob.sex = values.sex;
    mob.extended_sex = values.extended_sex;
    mob.immune = values.immune;
    mob.meta_immune = values.meta_immune;
    mob.susceptible = values.susceptible;
    mob.sounds = values.sounds;
    mob.distant_sounds = values.distant_sounds;
    mob.extra_sound_strings = values.extra_sound_strings;
}

void apply_obj_edit(GameObject& obj, const ObjEdit& edit) {
    if (!edit.name.empty()) {
        obj.name = edit.name;
    }
    if (!edit.short_descr.empty()) {
        obj.short_descr = edit.short_descr;
    }
    if (!edit.description.empty()) {
        obj.description = edit.description;
    }
    if (!edit.action_description.empty()) {
        obj.action_description = edit.action_description;
    }
    if (edit.type_flag >= 0) {
        obj.type_flag = edit.type_flag;
    }
    if (edit.extra_flags >= 0) {
        obj.extra_flags = edit.extra_flags;
    }
    if (edit.wear_flags >= 0) {
        obj.wear_flags = edit.wear_flags;
    }
    if (edit.value0 >= 0) {
        obj.value[0] = edit.value0;
    }
    if (edit.value1 >= 0) {
        obj.value[1] = edit.value1;
    }
    if (edit.value2 >= 0) {
        obj.value[2] = edit.value2;
    }
    if (edit.value3 >= 0) {
        obj.value[3] = edit.value3;
    }
    if (edit.weight >= 0) {
        obj.weight = edit.weight;
    }
    if (edit.cost >= 0) {
        obj.cost = edit.cost;
    }
    if (edit.cost_per_day >= 0) {
        obj.cost_per_day = edit.cost_per_day;
    }
    if (edit.has_extra_flags2_set) {
        obj.has_extra_flags2 = edit.has_extra_flags2;
    }
    if (edit.extra_flags2 >= 0) {
        obj.extra_flags2 = edit.extra_flags2;
    }
    if (edit.forbidden_set) {
        obj.forbidden_char = edit.forbidden_char;
        obj.forbidden_room = edit.forbidden_room;
    }
}

void assign_object_fields(GameObject& obj, const GameObject& values) {
    obj.name = values.name;
    obj.short_descr = values.short_descr;
    obj.description = values.description;
    obj.action_description = values.action_description;
    obj.type_flag = values.type_flag;
    obj.extra_flags = values.extra_flags;
    obj.wear_flags = values.wear_flags;
    obj.value[0] = values.value[0];
    obj.value[1] = values.value[1];
    obj.value[2] = values.value[2];
    obj.value[3] = values.value[3];
    obj.weight = values.weight;
    obj.cost = values.cost;
    obj.cost_per_day = values.cost_per_day;
    obj.extra_descs = values.extra_descs;
    obj.affects = values.affects;
    obj.has_extra_flags2 = values.has_extra_flags2;
    obj.extra_flags2 = values.extra_flags2;
    obj.forbidden_char = values.forbidden_char;
    obj.forbidden_room = values.forbidden_room;
}

bool edit_room(World& world, long vnum, const RoomEdit& edit) {
    Room* room = world.find_room(vnum);
    if (!room) {
        return false;
    }
    const std::string old_name = room->name;
    apply_room_edit(*room, edit);
    if (!edit.name.empty() && !strings_equal_ci(old_name, room->name)) {
        refresh_inbound_exit_descriptions(world, vnum);
    }
    return true;
}

bool edit_mob(World& world, long vnum, const MobEdit& edit) {
    Mobile* mob = world.find_mobile(vnum);
    if (!mob) {
        return false;
    }
    apply_mob_edit(*mob, edit);
    return true;
}

bool edit_object(World& world, long vnum, const ObjEdit& edit) {
    GameObject* obj = world.find_object(vnum);
    if (!obj) {
        return false;
    }
    apply_obj_edit(*obj, edit);
    return true;
}

RoomEdit room_edit_from_flags(const std::map<std::string, std::string>& flags) {
    RoomEdit edit;
    if (auto it = flags.find("name"); it != flags.end()) {
        edit.name = it->second;
    }
    if (auto it = flags.find("desc"); it != flags.end()) {
        edit.description = it->second;
    }
    if (auto it = flags.find("sector"); it != flags.end()) {
        edit.sector_type = parse_long(it->second, "sector");
    }
    if (auto it = flags.find("flags"); it != flags.end()) {
        edit.room_flags = parse_long(it->second, "flags");
    }
    return edit;
}

MobEdit mob_edit_from_flags(const std::map<std::string, std::string>& flags) {
    MobEdit edit;
    if (auto it = flags.find("short"); it != flags.end()) {
        edit.short_descr = it->second;
    }
    if (auto it = flags.find("long"); it != flags.end()) {
        edit.long_descr = it->second;
    }
    if (auto it = flags.find("desc"); it != flags.end()) {
        edit.description = it->second;
    }
    if (auto it = flags.find("level"); it != flags.end()) {
        edit.level = parse_int(it->second, "level");
    }
    if (auto it = flags.find("alignment"); it != flags.end()) {
        edit.alignment = parse_long(it->second, "alignment");
    }
    return edit;
}

ObjEdit obj_edit_from_flags(const std::map<std::string, std::string>& flags) {
    ObjEdit edit;
    if (auto it = flags.find("short"); it != flags.end()) {
        edit.short_descr = it->second;
    }
    if (auto it = flags.find("desc"); it != flags.end()) {
        edit.description = it->second;
    }
    if (auto it = flags.find("cost"); it != flags.end()) {
        edit.cost = parse_int(it->second, "cost");
    }
    if (auto it = flags.find("weight"); it != flags.end()) {
        edit.weight = parse_int(it->second, "weight");
    }
    return edit;
}

bool create_room(World& world, long vnum, const RoomEdit& edit) {
    if (vnum <= 0 || world.rooms.count(vnum) > 0) {
        return false;
    }

    Room room;
    room.vnum = vnum;
    room.name = edit.name.empty() ? "Nuova stanza" : edit.name;
    room.description = edit.description.empty() ? "Una stanza vuota." : edit.description;
    room.sector_type = edit.sector_type >= 0 ? edit.sector_type : 1;
    room.room_flags = edit.room_flags >= 0 ? edit.room_flags : 0;
    room.zone_index = zone_index_for_vnum(world, vnum);
    world.rooms.emplace(vnum, std::move(room));
    return true;
}

bool create_mob(World& world, long vnum, const MobEdit& edit) {
    if (vnum <= 0 || world.mobiles.count(vnum) > 0) {
        return false;
    }
    world.mobiles.emplace(vnum, make_default_mobile(vnum, edit));
    return true;
}

bool create_object(World& world, long vnum, const ObjEdit& edit) {
    if (vnum <= 0 || world.objects.count(vnum) > 0) {
        return false;
    }
    world.objects.emplace(vnum, make_default_object(vnum, edit));
    return true;
}

const Exit* find_room_exit(const Room& room, int direction) {
    for (const auto& exit : room.exits) {
        if (exit.direction == direction) {
            return &exit;
        }
    }
    return nullptr;
}

std::size_t refresh_inbound_exit_descriptions(World& world, long target_vnum) {
    const Room* destination = world.find_room(target_vnum);
    if (!destination) {
        return 0;
    }

    std::size_t updated = 0;
    for (auto& [from_vnum, room] : world.rooms) {
        for (auto& exit : room.exits) {
            if (exit.to_room != target_vnum) {
                continue;
            }
            if (strings_equal_ci(exit.description, destination->name)) {
                continue;
            }
            exit.description = destination->name;
            ++updated;
        }
        (void)from_vnum;
    }
    return updated;
}

bool set_room_exit(World& world, long room_vnum, const ExitEdit& edit) {
    Room* room = world.find_room(room_vnum);
    if (!room || edit.direction < 0 || edit.direction >= EXIT_DIR_COUNT) {
        return false;
    }

    Exit exit;
    exit.direction = edit.direction;
    exit.description = edit.description;
    exit.keyword = edit.keyword;
    exit.exit_info = edit.exit_info;
    exit.key = edit.key;
    exit.to_room = edit.to_room;
    exit.open_cmd = edit.open_cmd;

    for (auto& existing : room->exits) {
        if (existing.direction == edit.direction) {
            existing = exit;
            return true;
        }
    }
    room->exits.push_back(exit);
    return true;
}

bool remove_room_exit(World& world, long room_vnum, int direction) {
    Room* room = world.find_room(room_vnum);
    if (!room) {
        return false;
    }
    const auto it = std::remove_if(room->exits.begin(),
                                   room->exits.end(),
                                   [direction](const Exit& exit) {
                                       return exit.direction == direction;
                                   });
    if (it == room->exits.end()) {
        return false;
    }
    room->exits.erase(it, room->exits.end());
    return true;
}

Zone* find_zone(World& world, int zone_num) {
    for (auto& zone : world.zones) {
        if (zone.num == zone_num) {
            return &zone;
        }
    }
    return nullptr;
}

const Zone* find_zone(const World& world, int zone_num) {
    for (const auto& zone : world.zones) {
        if (zone.num == zone_num) {
            return &zone;
        }
    }
    return nullptr;
}

bool is_editable_reset_command(const char command) {
    return std::strchr("HFMCOGEPD", command) != nullptr;
}

std::string reset_command_summary(const ResetCommand& cmd) {
    if (cmd.command == '*') {
        std::string line = cmd.raw_line;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        return std::string("Comment: ") + line;
    }
    if (cmd.command == ';') {
        return "Separator";
    }

    std::string summary(1, cmd.command);
    summary += " if=" + std::to_string(cmd.if_flag);
    summary += " a1=" + std::to_string(cmd.arg1);
    summary += " a2=" + std::to_string(cmd.arg2);
    summary += " a3=" + std::to_string(cmd.arg3);
    summary += " a4=" + std::to_string(cmd.arg4);

    switch (cmd.command) {
    case 'M':
        summary += " (mob #" + std::to_string(cmd.arg1) + " -> room #" + std::to_string(cmd.arg3)
                   + ")";
        break;
    case 'O':
        summary += " (obj #" + std::to_string(cmd.arg1) + " -> room #" + std::to_string(cmd.arg3)
                   + ")";
        break;
    case 'G':
        summary += " (obj #" + std::to_string(cmd.arg1) + " to last mob)";
        break;
    case 'E':
        summary += " (obj #" + std::to_string(cmd.arg1) + " equip pos " + std::to_string(cmd.arg3)
                   + ")";
        break;
    case 'P':
        summary += " (obj #" + std::to_string(cmd.arg1) + " in container #"
                   + std::to_string(cmd.arg3) + ")";
        break;
    case 'D':
        summary += " (room #" + std::to_string(cmd.arg1) + " exit " + std::to_string(cmd.arg2)
                   + " state " + std::to_string(cmd.arg3) + ")";
        break;
    case 'C':
        summary += " (mob #" + std::to_string(cmd.arg1) + " cmd " + std::to_string(cmd.arg2)
                   + ")";
        break;
    case 'H':
        summary += " (room #" + std::to_string(cmd.arg1) + " hour " + std::to_string(cmd.arg2)
                   + ")";
        break;
    default:
        break;
    }
    return summary;
}

ResetCommand default_zone_reset(const char command, const Zone& zone, const World& world) {
    ResetCommand cmd;
    cmd.command = command;
    cmd.if_flag = 0;
    cmd.arg1 = -1;
    cmd.arg2 = 0;
    cmd.arg3 = -1;
    cmd.arg4 = 0;

    const long room = first_room_in_zone(zone, world);
    const long mob = first_mobile_vnum(world);
    const long obj = first_object_vnum(world);

    switch (command) {
    case 'M':
        cmd.arg1 = static_cast<int>(mob);
        cmd.arg2 = 1;
        cmd.arg3 = static_cast<int>(room);
        cmd.arg4 = 1;
        break;
    case 'O':
        cmd.arg1 = static_cast<int>(obj);
        cmd.arg2 = 1;
        cmd.arg3 = static_cast<int>(room);
        cmd.arg4 = 1;
        break;
    case 'G':
    case 'E':
        cmd.arg1 = static_cast<int>(obj);
        cmd.arg2 = 1;
        cmd.arg3 = command == 'E' ? 0 : -1;
        break;
    case 'P':
        cmd.arg1 = static_cast<int>(obj);
        cmd.arg2 = 1;
        cmd.arg3 = static_cast<int>(obj);
        break;
    case 'D':
        cmd.arg1 = static_cast<int>(room);
        cmd.arg2 = 0;
        break;
    case 'C':
        cmd.arg1 = static_cast<int>(mob);
        cmd.arg2 = 0;
        break;
    case 'H':
        cmd.arg1 = static_cast<int>(room);
        cmd.arg2 = 0;
        break;
    default:
        break;
    }
    return cmd;
}

bool add_zone_reset(World& world, const int zone_num, const ResetCommand& cmd) {
    Zone* zone = find_zone(world, zone_num);
    if (!zone || !is_editable_reset_command(cmd.command)) {
        return false;
    }
    zone->commands.push_back(cmd);
    return true;
}

bool update_zone_reset(World& world, const int zone_num, const std::size_t index, const ResetCommand& cmd) {
    Zone* zone = find_zone(world, zone_num);
    if (!zone || index >= zone->commands.size()) {
        return false;
    }
    if (!is_editable_reset_command(cmd.command)) {
        return false;
    }
    zone->commands[index] = cmd;
    zone->commands[index].raw_line.clear();
    return true;
}

bool remove_zone_reset(World& world, const int zone_num, const std::size_t index) {
    Zone* zone = find_zone(world, zone_num);
    if (!zone || index >= zone->commands.size()) {
        return false;
    }
    const ResetCommand& cmd = zone->commands[index];
    if (!is_editable_reset_command(cmd.command)) {
        return false;
    }
    zone->commands.erase(zone->commands.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool move_zone_reset(World& world, const int zone_num, const std::size_t from_index, const std::size_t to_index) {
    Zone* zone = find_zone(world, zone_num);
    if (!zone || from_index >= zone->commands.size() || to_index >= zone->commands.size()
        || from_index == to_index) {
        return false;
    }
    ResetCommand cmd = zone->commands[from_index];
    zone->commands.erase(zone->commands.begin() + static_cast<std::ptrdiff_t>(from_index));
    zone->commands.insert(zone->commands.begin() + static_cast<std::ptrdiff_t>(to_index), cmd);
    return true;
}

} // namespace nebbie
