#include "nebbie/system_field_config.hpp"

#include "nebbie/mob_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace nebbie {

namespace {

constexpr int kApplyResistance = 26;
constexpr int kApplySusceptibility = 27;
constexpr int kApplyImmunity = 28;

SystemFieldDefinition make_field(const char* damage_id,
                                 const char* damage_label,
                                 SystemFieldKind kind,
                                 long mob_bit,
                                 unsigned default_targets) {
    SystemFieldDefinition field;
    field.kind = kind;
    field.object_modifier_bit = mob_bit;
    field.mob_flag_bit = mob_bit;
    switch (kind) {
    case SystemFieldKind::Resistance:
        field.id = std::string("resistance.") + damage_id;
        field.label = std::string("Resistance by ") + damage_label;
        field.object_apply_type = kApplyResistance;
        break;
    case SystemFieldKind::Susceptibility:
        field.id = std::string("susceptibility.") + damage_id;
        field.label = std::string("Susceptibility to ") + damage_label;
        field.object_apply_type = kApplySusceptibility;
        break;
    case SystemFieldKind::Immunity:
        field.id = std::string("immunity.") + damage_id;
        field.label = std::string("Immunity to ") + damage_label;
        field.object_apply_type = kApplyImmunity;
        break;
    }
    field.default_targets = default_targets;
    return field;
}

std::string trim_copy(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string to_lower_copy(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

const SystemFieldDefinition* find_definition(const SystemFieldConfig& config, const std::string& id) {
    for (const auto& def : config.definitions) {
        if (def.id == id) {
            return &def;
        }
    }
    return nullptr;
}

long mob_field_value(const SystemFieldDefinition& field, long immune, long meta_immune, long susceptible) {
    switch (field.kind) {
    case SystemFieldKind::Resistance:
        return immune;
    case SystemFieldKind::Susceptibility:
        return susceptible;
    case SystemFieldKind::Immunity:
        return meta_immune;
    }
    return 0;
}

long* mob_field_ref(Mobile& mob, const SystemFieldDefinition& field) {
    switch (field.kind) {
    case SystemFieldKind::Resistance:
        return &mob.immune;
    case SystemFieldKind::Susceptibility:
        return &mob.susceptible;
    case SystemFieldKind::Immunity:
        return &mob.meta_immune;
    }
    return nullptr;
}

} // namespace

std::vector<SystemFieldDefinition> system_field_definitions() {
    std::vector<SystemFieldDefinition> defs;
    const auto immunity_flags = mob_immunity_flags();
    defs.reserve(immunity_flags.size() * 3);
    for (const auto& flag : immunity_flags) {
        std::string damage_id = flag.name;
        if (damage_id.rfind("IMM_", 0) == 0) {
            damage_id = damage_id.substr(4);
        }
        damage_id = to_lower_copy(damage_id);
        const std::string damage_label = flag.label;
        defs.push_back(make_field(damage_id.c_str(), damage_label.c_str(), SystemFieldKind::Resistance,
                                  flag.value, static_cast<unsigned>(EditTarget::Object)));
        defs.push_back(make_field(damage_id.c_str(), damage_label.c_str(), SystemFieldKind::Susceptibility,
                                  flag.value, static_cast<unsigned>(EditTarget::Object)));
        defs.push_back(make_field(damage_id.c_str(), damage_label.c_str(), SystemFieldKind::Immunity,
                                  flag.value, static_cast<unsigned>(EditTarget::Object)));
    }
    return defs;
}

SystemFieldConfig default_system_field_config() {
    SystemFieldConfig config;
    config.definitions = system_field_definitions();
    config.placements.reserve(config.definitions.size());
    for (const auto& def : config.definitions) {
        config.placements.emplace_back(def.id, def.default_targets);
    }
    return config;
}

std::filesystem::path default_system_config_path(const std::filesystem::path& lib_path) {
    return lib_path / ".nebbie" / "system.conf";
}

unsigned parse_edit_targets(const std::string& text) {
    unsigned targets = 0;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ',')) {
        token = to_lower_copy(trim_copy(token));
        if (token == "object" || token == "obj" || token == "oggetto") {
            targets |= static_cast<unsigned>(EditTarget::Object);
        } else if (token == "mob" || token == "mobile") {
            targets |= static_cast<unsigned>(EditTarget::Mob);
        } else if (token == "pg" || token == "toon" || token == "player" || token == "character") {
            targets |= static_cast<unsigned>(EditTarget::Pg);
        }
    }
    return targets;
}

std::string edit_targets_label(unsigned targets) {
    std::vector<std::string> parts;
    if (targets_allow(targets, EditTarget::Object)) {
        parts.emplace_back("object");
    }
    if (targets_allow(targets, EditTarget::Mob)) {
        parts.emplace_back("mob");
    }
    if (targets_allow(targets, EditTarget::Pg)) {
        parts.emplace_back("pg");
    }
    if (parts.empty()) {
        return "none";
    }
    std::ostringstream oss;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << parts[i];
    }
    return oss.str();
}

SystemFieldConfig read_system_field_config(const std::filesystem::path& path) {
    SystemFieldConfig config = default_system_field_config();
    std::ifstream input(path);
    if (!input.is_open()) {
        return config;
    }

    std::unordered_map<std::string, unsigned> overrides;
    std::string line;
    while (std::getline(input, line)) {
        line = trim_copy(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim_copy(line.substr(0, eq));
        const std::string value = trim_copy(line.substr(eq + 1));
        overrides[key] = parse_edit_targets(value);
    }

    for (auto& [field_id, targets] : config.placements) {
        const auto it = overrides.find(field_id);
        if (it != overrides.end()) {
            targets = it->second;
        }
    }
    return config;
}

bool write_system_field_config(const std::filesystem::path& path, const SystemFieldConfig& config) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "# Nebbie system field placement\n";
    output << "# targets: object, mob, pg (comma-separated)\n";
    output << "# Delete this file or a key to revert to defaults for that field.\n";
    for (const auto& [field_id, targets] : config.placements) {
        output << field_id << '=' << edit_targets_label(targets) << '\n';
    }
    return true;
}

SystemFieldConfig merge_system_field_config(const SystemFieldConfig& defaults,
                                            const SystemFieldConfig& stored) {
    SystemFieldConfig merged = defaults;
    for (const auto& [field_id, targets] : stored.placements) {
        set_field_targets(merged, field_id, targets);
    }
    return merged;
}

unsigned targets_for_field(const SystemFieldConfig& config, const std::string& field_id) {
    for (const auto& [id, targets] : config.placements) {
        if (id == field_id) {
            return targets;
        }
    }
    return static_cast<unsigned>(EditTarget::Object);
}

bool targets_allow(unsigned targets, EditTarget target) {
    return (targets & static_cast<unsigned>(target)) != 0;
}

void set_field_targets(SystemFieldConfig& config, const std::string& field_id, unsigned targets) {
    for (auto& [id, value] : config.placements) {
        if (id == field_id) {
            value = targets;
            return;
        }
    }
    config.placements.emplace_back(field_id, targets);
}

bool object_affect_matches_field(const ObjAffect& affect, const SystemFieldDefinition& field) {
    return affect.location == field.object_apply_type
           && (static_cast<long>(affect.modifier) & field.object_modifier_bit) != 0;
}

bool mob_flag_matches_field(long immune, long meta_immune, long susceptible,
                            const SystemFieldDefinition& field) {
    const long value = mob_field_value(field, immune, meta_immune, susceptible);
    return (value & field.mob_flag_bit) != 0;
}

bool object_affect_allowed(const SystemFieldConfig& config, const int location, const int modifier) {
    bool matched_resistance_field = false;
    for (const auto& def : config.definitions) {
        if (def.object_apply_type != location) {
            continue;
        }
        if ((static_cast<long>(modifier) & def.object_modifier_bit) == 0) {
            continue;
        }
        matched_resistance_field = true;
        if (!targets_allow(targets_for_field(config, def.id), EditTarget::Object)) {
            return false;
        }
    }
    return true;
}

bool mob_field_allowed(const SystemFieldConfig& config, const SystemFieldDefinition& field) {
    return targets_allow(targets_for_field(config, field.id), EditTarget::Mob);
}

FieldMigrationReport scan_field_migration(const World& world,
                                          const SystemFieldConfig& previous,
                                          const SystemFieldConfig& next) {
    FieldMigrationReport report;
    for (const auto& def : next.definitions) {
        const unsigned old_targets = targets_for_field(previous, def.id);
        const unsigned new_targets = targets_for_field(next, def.id);
        if (old_targets == new_targets) {
            continue;
        }

        if (targets_allow(old_targets, EditTarget::Object) && !targets_allow(new_targets, EditTarget::Object)) {
            for (const auto& [vnum, obj] : world.objects) {
                for (const auto& affect : obj.affects) {
                    if (!object_affect_matches_field(affect, def)) {
                        continue;
                    }
                    FieldMigrationEntry entry;
                    entry.field_id = def.id;
                    entry.entity_kind = "object";
                    entry.vnum = vnum;
                    entry.detail = "apply " + std::to_string(affect.location) + ' '
                                  + std::to_string(affect.modifier);
                    report.entries.push_back(entry);
                    ++report.object_affects_to_strip;
                }
            }
        }

        if (targets_allow(old_targets, EditTarget::Mob) && !targets_allow(new_targets, EditTarget::Mob)) {
            for (const auto& [vnum, mob] : world.mobiles) {
                if (!mob_flag_matches_field(mob.immune, mob.meta_immune, mob.susceptible, def)) {
                    continue;
                }
                FieldMigrationEntry entry;
                entry.field_id = def.id;
                entry.entity_kind = "mob";
                entry.vnum = vnum;
                entry.detail = "flag bit " + std::to_string(def.mob_flag_bit);
                report.entries.push_back(entry);
                ++report.mob_flags_to_strip;
            }
        }

        if (!targets_allow(old_targets, EditTarget::Object) && targets_allow(new_targets, EditTarget::Object)) {
            FieldMigrationEntry entry;
            entry.field_id = def.id;
            entry.entity_kind = "info";
            entry.vnum = 0;
            entry.detail = "object editing enabled; existing values remain on mobs/pg until imported manually";
            report.entries.push_back(entry);
        }
        if (!targets_allow(old_targets, EditTarget::Pg) && targets_allow(new_targets, EditTarget::Pg)) {
            FieldMigrationEntry entry;
            entry.field_id = def.id;
            entry.entity_kind = "info";
            entry.vnum = 0;
            entry.detail = "pg/toon editing enabled (editor UI only; runtime import not implemented here)";
            report.entries.push_back(entry);
        }
    }
    return report;
}

bool export_field_migration_csv(const FieldMigrationReport& report,
                                const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << "field_id,entity_kind,vnum,detail\n";
    for (const auto& entry : report.entries) {
        output << entry.field_id << ','
               << entry.entity_kind << ','
               << entry.vnum << ',';
        std::string detail = entry.detail;
        if (detail.find(',') != std::string::npos || detail.find('"') != std::string::npos) {
            detail.replace(detail.find('"'), 1, "\"\"");
            output << '"' << detail << '"';
        } else {
            output << detail;
        }
        output << '\n';
    }
    return true;
}

int strip_disallowed_object_affects(World& world, const SystemFieldConfig& config) {
    int removed = 0;
    for (auto& [vnum, obj] : world.objects) {
        std::vector<ObjAffect> kept;
        kept.reserve(obj.affects.size());
        for (const auto& affect : obj.affects) {
            bool allowed = true;
            for (const auto& def : config.definitions) {
                if (!object_affect_matches_field(affect, def)) {
                    continue;
                }
                if (!targets_allow(targets_for_field(config, def.id), EditTarget::Object)) {
                    allowed = false;
                    break;
                }
            }
            if (allowed) {
                kept.push_back(affect);
            } else {
                ++removed;
            }
        }
        obj.affects = std::move(kept);
    }
    return removed;
}

int strip_disallowed_mob_flags(World& world, const SystemFieldConfig& config) {
    int cleared = 0;
    for (auto& [vnum, mob] : world.mobiles) {
        for (const auto& def : config.definitions) {
            if (targets_allow(targets_for_field(config, def.id), EditTarget::Mob)) {
                continue;
            }
            long* field_ref = mob_field_ref(mob, def);
            if (field_ref == nullptr || (*field_ref & def.mob_flag_bit) == 0) {
                continue;
            }
            *field_ref &= ~def.mob_flag_bit;
            ++cleared;
        }
    }
    return cleared;
}

} // namespace nebbie
