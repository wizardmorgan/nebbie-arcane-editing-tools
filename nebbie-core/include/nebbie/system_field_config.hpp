#pragma once

#include "nebbie/world.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace nebbie {

enum class EditTarget : unsigned {
    Object = 1u << 0,
    Mob = 1u << 1,
    Pg = 1u << 2,
};

enum class SystemFieldKind {
    Resistance,
    Susceptibility,
    Immunity,
};

struct SystemFieldDefinition {
    std::string id;
    std::string label;
    SystemFieldKind kind = SystemFieldKind::Resistance;
    int object_apply_type = 0;
    long object_modifier_bit = 0;
    long mob_flag_bit = 0;
    unsigned default_targets = static_cast<unsigned>(EditTarget::Object);
};

struct SystemFieldConfig {
    std::vector<SystemFieldDefinition> definitions;
    std::vector<std::pair<std::string, unsigned>> placements;
};

struct FieldMigrationEntry {
    std::string field_id;
    std::string entity_kind;
    long vnum = 0;
    std::string detail;
};

struct FieldMigrationReport {
    std::vector<FieldMigrationEntry> entries;
    int object_affects_to_strip = 0;
    int mob_flags_to_strip = 0;
};

std::vector<SystemFieldDefinition> system_field_definitions();

SystemFieldConfig default_system_field_config();

std::filesystem::path default_system_config_path(const std::filesystem::path& lib_path);

SystemFieldConfig read_system_field_config(const std::filesystem::path& path);
bool write_system_field_config(const std::filesystem::path& path, const SystemFieldConfig& config);

SystemFieldConfig merge_system_field_config(const SystemFieldConfig& defaults,
                                            const SystemFieldConfig& stored);

unsigned targets_for_field(const SystemFieldConfig& config, const std::string& field_id);

bool targets_allow(unsigned targets, EditTarget target);

void set_field_targets(SystemFieldConfig& config, const std::string& field_id, unsigned targets);

bool object_affect_matches_field(const ObjAffect& affect, const SystemFieldDefinition& field);

bool mob_flag_matches_field(long immune, long meta_immune, long susceptible,
                            const SystemFieldDefinition& field);

bool object_affect_allowed(const SystemFieldConfig& config, int location, int modifier);

bool mob_field_allowed(const SystemFieldConfig& config, const SystemFieldDefinition& field);

FieldMigrationReport scan_field_migration(const World& world,
                                          const SystemFieldConfig& previous,
                                          const SystemFieldConfig& next);

bool export_field_migration_csv(const FieldMigrationReport& report,
                                const std::filesystem::path& path);

int strip_disallowed_object_affects(World& world, const SystemFieldConfig& config);

int strip_disallowed_mob_flags(World& world, const SystemFieldConfig& config);

std::string edit_targets_label(unsigned targets);

unsigned parse_edit_targets(const std::string& text);

} // namespace nebbie
