#pragma once

#include "nebbie/types.hpp"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTabWidget;
class QTextEdit;
class FlagGroupWidget;

class RoomEditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit RoomEditorWidget(QWidget* parent = nullptr);

    void loadFromRoom(const nebbie::Room& room);
    void saveToRoom(nebbie::Room& room) const;

    long selectedExitToRoom() const;

private slots:
    void onExtraDescSelected();
    void addExtraDesc();
    void removeExtraDesc();
    void onExitSelected();
    void addOrUpdateExit();
    void removeExit();

private:
    void updateConditionalFields();
    void refreshExtraDescForm();
    void refreshExitForm();
    void refreshOverview();
    void connectOverviewUpdates();
    void setComboIntValue(QComboBox* combo, int value) const;

    bool loading_ = false;
    QTextEdit* overview_ = nullptr;

    QLineEdit* name_ = nullptr;
    QTextEdit* description_ = nullptr;

    QComboBox* sector_type_ = nullptr;
    FlagGroupWidget* room_flags_ = nullptr;

    QSpinBox* tele_time_ = nullptr;
    QSpinBox* tele_targ_ = nullptr;
    QSpinBox* tele_mask_ = nullptr;
    QSpinBox* tele_cnt_ = nullptr;

    QWidget* river_panel_ = nullptr;
    QSpinBox* river_speed_ = nullptr;
    QSpinBox* river_dir_ = nullptr;
    QWidget* moblim_panel_ = nullptr;
    QSpinBox* moblim_ = nullptr;

    QLineEdit* bright_at_night_ = nullptr;
    QLineEdit* bright_at_day_ = nullptr;

    QListWidget* extra_desc_list_ = nullptr;
    QLineEdit* extra_desc_keyword_ = nullptr;
    QTextEdit* extra_desc_description_ = nullptr;

    QListWidget* exit_list_ = nullptr;
    QComboBox* exit_direction_ = nullptr;
    QSpinBox* exit_to_room_ = nullptr;
    QLineEdit* exit_description_ = nullptr;
    QLineEdit* exit_keyword_ = nullptr;
    FlagGroupWidget* exit_flags_ = nullptr;
    QSpinBox* exit_key_ = nullptr;
    QSpinBox* exit_open_cmd_ = nullptr;
};
