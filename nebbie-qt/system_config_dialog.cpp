#include "system_config_dialog.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

SystemConfigDialog::SystemConfigDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Parametri di sistema");
    resize(760, 520);

    auto* root = new QVBoxLayout(this);
    root->addWidget(new QLabel(
        "Configura dove ogni campo di resistenza/immunita' puo' essere editato "
        "(oggetto, mob o PG/toon). I valori PG sono solo preparati per il futuro; "
        "l'import runtime resta esterno (es. legacyimport sul server). "
        "Eliminare .nebbie/system.conf ripristina i default."));

    table_ = new QTableWidget;
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"Campo", "Oggetto", "Mob", "PG/Toon"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    root->addWidget(table_, 1);

    auto* buttons = new QHBoxLayout;
    auto* defaults_button = new QPushButton("Ripristina default");
    auto* preview_button = new QPushButton("Anteprima migrazione...");
    auto* export_button = new QPushButton("Esporta CSV...");
    connect(defaults_button, &QPushButton::clicked, this, &SystemConfigDialog::restoreDefaults);
    connect(preview_button, &QPushButton::clicked, this, &SystemConfigDialog::previewMigration);
    connect(export_button, &QPushButton::clicked, this, &SystemConfigDialog::exportMigrationCsv);
    buttons->addWidget(defaults_button);
    buttons->addWidget(preview_button);
    buttons->addWidget(export_button);
    buttons->addStretch();
    root->addLayout(buttons);

    auto* dialog_buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(dialog_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(dialog_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(dialog_buttons);
}

void SystemConfigDialog::setConfig(const nebbie::SystemFieldConfig& config) {
    working_ = config;
    rebuildTable();
}

void SystemConfigDialog::setBaselineConfig(const nebbie::SystemFieldConfig& baseline) {
    baseline_ = baseline;
}

void SystemConfigDialog::setWorld(const nebbie::World* world) {
    world_ = world;
}

nebbie::SystemFieldConfig SystemConfigDialog::config() const {
    nebbie::SystemFieldConfig saved = working_;
    for (int row = 0; row < table_->rowCount(); ++row) {
        const QString field_id = table_->item(row, 0)->data(Qt::UserRole).toString();
        nebbie::set_field_targets(saved, field_id.toStdString(), targetsFromRow(row));
    }
    return saved;
}

void SystemConfigDialog::rebuildTable() {
    table_->setRowCount(static_cast<int>(working_.definitions.size()));
    int row = 0;
    for (const auto& def : working_.definitions) {
        const unsigned targets = nebbie::targets_for_field(working_, def.id);
        auto* label_item = new QTableWidgetItem(QString::fromStdString(def.label));
        label_item->setData(Qt::UserRole, QString::fromStdString(def.id));
        label_item->setFlags(label_item->flags() & ~Qt::ItemIsEditable);
        table_->setItem(row, 0, label_item);

        auto* object_box = new QCheckBox;
        object_box->setChecked(nebbie::targets_allow(targets, nebbie::EditTarget::Object));
        auto* mob_box = new QCheckBox;
        mob_box->setChecked(nebbie::targets_allow(targets, nebbie::EditTarget::Mob));
        auto* pg_box = new QCheckBox;
        pg_box->setChecked(nebbie::targets_allow(targets, nebbie::EditTarget::Pg));
        pg_box->setToolTip("Riservato al futuro editing PG/toon e import server-side");

        table_->setCellWidget(row, 1, object_box);
        table_->setCellWidget(row, 2, mob_box);
        table_->setCellWidget(row, 3, pg_box);
        ++row;
    }
}

unsigned SystemConfigDialog::targetsFromRow(const int row) const {
    unsigned targets = 0;
    if (auto* box = qobject_cast<QCheckBox*>(table_->cellWidget(row, 1)); box && box->isChecked()) {
        targets |= static_cast<unsigned>(nebbie::EditTarget::Object);
    }
    if (auto* box = qobject_cast<QCheckBox*>(table_->cellWidget(row, 2)); box && box->isChecked()) {
        targets |= static_cast<unsigned>(nebbie::EditTarget::Mob);
    }
    if (auto* box = qobject_cast<QCheckBox*>(table_->cellWidget(row, 3)); box && box->isChecked()) {
        targets |= static_cast<unsigned>(nebbie::EditTarget::Pg);
    }
    return targets;
}

void SystemConfigDialog::setRowTargets(const int row, const unsigned targets) {
    if (auto* box = qobject_cast<QCheckBox*>(table_->cellWidget(row, 1))) {
        box->setChecked(nebbie::targets_allow(targets, nebbie::EditTarget::Object));
    }
    if (auto* box = qobject_cast<QCheckBox*>(table_->cellWidget(row, 2))) {
        box->setChecked(nebbie::targets_allow(targets, nebbie::EditTarget::Mob));
    }
    if (auto* box = qobject_cast<QCheckBox*>(table_->cellWidget(row, 3))) {
        box->setChecked(nebbie::targets_allow(targets, nebbie::EditTarget::Pg));
    }
}

void SystemConfigDialog::restoreDefaults() {
    working_ = baseline_;
    for (int row = 0; row < table_->rowCount(); ++row) {
        const QString field_id = table_->item(row, 0)->data(Qt::UserRole).toString();
        setRowTargets(row, nebbie::targets_for_field(baseline_, field_id.toStdString()));
    }
}

void SystemConfigDialog::previewMigration() {
    const nebbie::SystemFieldConfig next = config();
    const nebbie::FieldMigrationReport report = world_ == nullptr
        ? nebbie::scan_field_migration({}, baseline_, next)
        : nebbie::scan_field_migration(*world_, baseline_, next);
    QMessageBox::information(
        this,
        "Anteprima migrazione",
        QString("Voci nel report: %1\n"
                "Affect oggetto da rimuovere se applichi la pulizia: %2\n"
                "Flag mob da rimuovere se applichi la pulizia: %3\n\n"
                "Esporta CSV per alimentare legacyimport o altri tool server-side.")
            .arg(static_cast<int>(report.entries.size()))
            .arg(report.object_affects_to_strip)
            .arg(report.mob_flags_to_strip));
}

void SystemConfigDialog::exportMigrationCsv() {
    const QString path =
        QFileDialog::getSaveFileName(this, "Esporta CSV migrazione", {}, "CSV (*.csv)");
    if (path.isEmpty()) {
        return;
    }
    const nebbie::FieldMigrationReport report = world_ == nullptr
        ? nebbie::scan_field_migration({}, baseline_, config())
        : nebbie::scan_field_migration(*world_, baseline_, config());
    if (!nebbie::export_field_migration_csv(report, path.toStdString())) {
        QMessageBox::warning(this, "Esporta CSV", "Impossibile scrivere il file.");
        return;
    }
    QMessageBox::information(this, "Esporta CSV", "CSV esportato.");
}
