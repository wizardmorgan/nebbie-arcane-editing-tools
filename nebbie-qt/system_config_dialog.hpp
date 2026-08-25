#pragma once

#include "nebbie/system_field_config.hpp"
#include "nebbie/world.hpp"

#include <QDialog>

class QTableWidget;
class QCheckBox;

class SystemConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit SystemConfigDialog(QWidget* parent = nullptr);

    void setConfig(const nebbie::SystemFieldConfig& config);
    void setBaselineConfig(const nebbie::SystemFieldConfig& baseline);
    void setWorld(const nebbie::World* world);
    nebbie::SystemFieldConfig config() const;

private slots:
    void restoreDefaults();
    void previewMigration();
    void exportMigrationCsv();

private:
    struct RowWidgets {
        QCheckBox* object_box = nullptr;
        QCheckBox* mob_box = nullptr;
        QCheckBox* pg_box = nullptr;
    };

    void rebuildTable();
    unsigned targetsFromRow(int row) const;
    void setRowTargets(int row, unsigned targets);

    nebbie::SystemFieldConfig baseline_;
    nebbie::SystemFieldConfig working_;
    const nebbie::World* world_ = nullptr;
    QTableWidget* table_ = nullptr;
};
