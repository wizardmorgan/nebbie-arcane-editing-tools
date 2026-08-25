#pragma once

#include "nebbie/types.hpp"
#include "nebbie/system_field_config.hpp"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QTextEdit;
class FlagGroupWidget;

class MobEditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit MobEditorWidget(QWidget* parent = nullptr);

    void loadFromMobile(const nebbie::Mobile& mob);
    void saveToMobile(nebbie::Mobile& mob) const;
    void setSystemFieldConfig(const nebbie::SystemFieldConfig* config);

private:
    void updateTypeDependentFields();
    void applySystemFieldRules();

    const nebbie::SystemFieldConfig* system_field_config_ = nullptr;

    QLineEdit* name_ = nullptr;
    QTextEdit* short_descr_ = nullptr;
    QTextEdit* long_descr_ = nullptr;
    QTextEdit* description_ = nullptr;

    QComboBox* mobtype_ = nullptr;
    QSpinBox* mult_att_ = nullptr;
    QSpinBox* level_ = nullptr;
    QSpinBox* hitroll_ = nullptr;
    QSpinBox* ac_ = nullptr;
    QSpinBox* hit_bonus_ = nullptr;
    QSpinBox* hit_num_ = nullptr;
    QSpinBox* hit_size_ = nullptr;
    QSpinBox* hit_plus_ = nullptr;
    QSpinBox* dam_num_ = nullptr;
    QSpinBox* dam_size_ = nullptr;
    QSpinBox* dam_plus_ = nullptr;
    QWidget* hit_dice_row_ = nullptr;
    QWidget* hit_bonus_row_ = nullptr;

    QSpinBox* alignment_ = nullptr;
    QSpinBox* gold_ = nullptr;
    QSpinBox* exp_ = nullptr;
    QCheckBox* extended_gold_ = nullptr;
    QComboBox* race_ = nullptr;
    QWidget* race_row_ = nullptr;

    QComboBox* position_ = nullptr;
    QComboBox* default_pos_ = nullptr;
    QComboBox* sex_ = nullptr;
    QCheckBox* extended_sex_ = nullptr;
    QWidget* immunity_panel_ = nullptr;

    FlagGroupWidget* act_flags_ = nullptr;
    FlagGroupWidget* affected_flags_ = nullptr;
    FlagGroupWidget* immune_flags_ = nullptr;
    FlagGroupWidget* meta_immune_flags_ = nullptr;
    FlagGroupWidget* susceptible_flags_ = nullptr;

    QLineEdit* sounds_ = nullptr;
    QLineEdit* distant_sounds_ = nullptr;
    QTextEdit* extra_sounds_ = nullptr;
    QWidget* sounds_panel_ = nullptr;
};
