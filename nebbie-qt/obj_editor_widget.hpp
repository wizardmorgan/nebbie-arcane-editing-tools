#pragma once

#include "nebbie/types.hpp"
#include "nebbie/system_field_config.hpp"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTextEdit;
class FlagGroupWidget;

class ObjEditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit ObjEditorWidget(QWidget* parent = nullptr);

    void loadFromObject(const nebbie::GameObject& obj);
    void saveToObject(nebbie::GameObject& obj) const;
    void setSystemFieldConfig(const nebbie::SystemFieldConfig* config);

private slots:
    void onExtraDescSelected();
    void addExtraDesc();
    void removeExtraDesc();
    void onAffectSelected();
    void addAffect();
    void removeAffect();

private:
    void refreshExtraDescForm();
    void refreshAffectForm();
    void setComboIntValue(QComboBox* combo, int value) const;
    bool currentAffectAllowed() const;

    const nebbie::SystemFieldConfig* system_field_config_ = nullptr;

    QLineEdit* name_ = nullptr;
    QTextEdit* short_descr_ = nullptr;
    QTextEdit* description_ = nullptr;
    QTextEdit* action_description_ = nullptr;

    QComboBox* type_flag_ = nullptr;
    QSpinBox* value0_ = nullptr;
    QSpinBox* value1_ = nullptr;
    QSpinBox* value2_ = nullptr;
    QSpinBox* value3_ = nullptr;

    FlagGroupWidget* wear_flags_ = nullptr;
    FlagGroupWidget* extra_flags_ = nullptr;
    QCheckBox* has_extra_flags2_ = nullptr;
    FlagGroupWidget* extra_flags2_ = nullptr;
    QWidget* extra_flags2_panel_ = nullptr;

    QSpinBox* weight_ = nullptr;
    QSpinBox* cost_ = nullptr;
    QSpinBox* cost_per_day_ = nullptr;

    QListWidget* affect_list_ = nullptr;
    QComboBox* affect_location_ = nullptr;
    QSpinBox* affect_modifier_ = nullptr;

    QListWidget* extra_desc_list_ = nullptr;
    QLineEdit* extra_desc_keyword_ = nullptr;
    QTextEdit* extra_desc_description_ = nullptr;

    QLineEdit* forbidden_char_ = nullptr;
    QLineEdit* forbidden_room_ = nullptr;
};
