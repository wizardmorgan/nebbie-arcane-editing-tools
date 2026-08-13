#include "room_editor_widget.hpp"

#include "flag_group_widget.hpp"
#include "nebbie/mob_catalog.hpp"
#include "nebbie/room_catalog.hpp"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTabWidget>
#include <QFont>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

void configureLineField(QLineEdit* field) {
    field->setMinimumWidth(420);
    field->setMinimumHeight(30);
    field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void configureTextField(QTextEdit* field, const int min_height) {
    field->setMinimumHeight(min_height);
    field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void configureDescriptionField(QTextEdit* field) {
    field->setMinimumWidth(560);
    field->setMinimumHeight(320);
    field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    field->setLineWrapMode(QTextEdit::NoWrap);
    field->setAcceptRichText(false);
    QFont font = field->font();
    font.setFamily(QStringLiteral("Monospace"));
    font.setStyleHint(QFont::Monospace);
    field->setFont(font);
}

QLabel* makeLegend(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

void fillCombo(QComboBox* combo, const std::vector<std::pair<int, std::string>>& choices) {
    combo->clear();
    for (const auto& [value, label] : choices) {
        combo->addItem(QString::fromStdString(label), value);
    }
}

int comboIntValue(QComboBox* combo) {
    return combo->currentData().toInt();
}

nebbie::Exit readExitItem(const QListWidgetItem* item) {
    nebbie::Exit exit;
    exit.direction = item->data(Qt::UserRole).toInt();
    exit.to_room = item->data(Qt::UserRole + 1).toLongLong();
    exit.description = item->data(Qt::UserRole + 2).toString().toStdString();
    exit.keyword = item->data(Qt::UserRole + 3).toString().toStdString();
    exit.exit_info = item->data(Qt::UserRole + 4).toLongLong();
    exit.key = item->data(Qt::UserRole + 5).toLongLong();
    exit.open_cmd = item->data(Qt::UserRole + 6).toLongLong();
    return exit;
}

QString formatActiveFlags(const long value, const std::vector<nebbie::MobFlagDef>& defs) {
    QStringList names;
    for (const nebbie::MobFlagDef& def : defs) {
        if ((value & def.value) != 0) {
            names.push_back(QString::fromUtf8(def.label));
        }
    }
    return names.join(", ");
}

void writeExitItem(QListWidgetItem* item, const nebbie::Exit& exit) {
    item->setData(Qt::UserRole, exit.direction);
    item->setData(Qt::UserRole + 1, static_cast<qlonglong>(exit.to_room));
    item->setData(Qt::UserRole + 2, QString::fromStdString(exit.description));
    item->setData(Qt::UserRole + 3, QString::fromStdString(exit.keyword));
    item->setData(Qt::UserRole + 4, static_cast<qlonglong>(exit.exit_info));
    item->setData(Qt::UserRole + 5, static_cast<qlonglong>(exit.key));
    item->setData(Qt::UserRole + 6, static_cast<qlonglong>(exit.open_cmd));

    QString details;
    if (!exit.keyword.empty()) {
        details += QString(" \"%1\"").arg(QString::fromStdString(exit.keyword));
    }
    if (exit.key > 0) {
        details += QString(" key #%1").arg(exit.key);
    }
    const QString exit_flags = formatActiveFlags(exit.exit_info, nebbie::exit_flag_defs());
    if (!exit_flags.isEmpty()) {
        details += QString(" [%1]").arg(exit_flags);
    }
    if (exit.open_cmd >= 0) {
        details += QString(" open_cmd=%1").arg(exit.open_cmd);
    }

    item->setText(QString("%1 -> #%2%3")
                      .arg(QString::fromStdString(nebbie::exit_direction_label(exit.direction)))
                      .arg(exit.to_room)
                      .arg(details));
}

QString buildOverviewText(const nebbie::Room& room) {
    QStringList lines;

    const QString name = QString::fromStdString(room.name).trimmed();
    if (!name.isEmpty()) {
        lines << QString("Nome: %1").arg(name);
    }

    const QString description = QString::fromStdString(room.description).trimmed();
    if (!description.isEmpty()) {
        const QString first_line = description.split('\n').constFirst().left(160);
        lines << QString("Descrizione: %1").arg(first_line);
        if (description.count('\n') > 0 || description.size() > 160) {
            lines << QString("  (%1 caratteri totali)").arg(description.size());
        }
    }

    lines << QString("Settore: %1").arg(QString::fromStdString(nebbie::room_sector_name(room.sector_type)));

    const QString room_flags = formatActiveFlags(room.room_flags, nebbie::room_flag_defs());
    if (!room_flags.isEmpty()) {
        lines << QString("Flag stanza: %1").arg(room_flags);
    }

    if (room.tele_time != 0 || room.tele_targ != 0 || room.tele_mask != 0 || room.tele_cnt != 0) {
        lines << "Teletrasporto:";
        if (room.tele_time != 0) {
            lines << QString("  tele_time: %1").arg(room.tele_time);
        }
        if (room.tele_targ != 0) {
            lines << QString("  tele_targ: %1").arg(room.tele_targ);
        }
        if (room.tele_mask != 0) {
            lines << QString("  tele_mask: %1").arg(room.tele_mask);
        }
        if (room.tele_cnt != 0) {
            lines << QString("  tele_cnt: %1").arg(room.tele_cnt);
        }
    }

    if (nebbie::room_sector_uses_river(room.sector_type)
        && (room.river_speed != 0 || room.river_dir != 0)) {
        lines << "Fiume:";
        if (room.river_speed != 0) {
            lines << QString("  river_speed: %1").arg(room.river_speed);
        }
        if (room.river_dir != 0) {
            lines << QString("  river_dir: %1").arg(room.river_dir);
        }
    }

    if (nebbie::room_flags_use_moblim(room.room_flags) && room.moblim > 0) {
        lines << QString("moblim (TUNNEL): %1").arg(room.moblim);
    }

    if (!room.bright_at_night.empty()) {
        lines << QString("Bright at night: %1").arg(QString::fromStdString(room.bright_at_night));
    }
    if (!room.bright_at_day.empty()) {
        lines << QString("Bright at day: %1").arg(QString::fromStdString(room.bright_at_day));
    }

    if (!room.extra_descs.empty()) {
        lines << QString("Extra descriptions (%1):").arg(room.extra_descs.size());
        for (const auto& extra : room.extra_descs) {
            const QString keyword = QString::fromStdString(extra.keyword).trimmed();
            lines << QString("  - %1").arg(keyword.isEmpty() ? "(senza keyword)" : keyword);
        }
    }

    if (!room.exits.empty()) {
        lines << QString("Uscite (%1):").arg(room.exits.size());
        for (const auto& exit : room.exits) {
            QStringList parts;
            parts << QString("%1 -> #%2")
                         .arg(QString::fromStdString(nebbie::exit_direction_label(exit.direction)))
                         .arg(exit.to_room);
            if (!exit.description.empty()) {
                parts << QString("desc \"%1\"").arg(QString::fromStdString(exit.description));
            }
            if (!exit.keyword.empty()) {
                parts << QString("keyword \"%1\"").arg(QString::fromStdString(exit.keyword));
            }
            if (exit.key > 0) {
                parts << QString("chiave oggetto #%1").arg(exit.key);
            }
            const QString exit_flags = formatActiveFlags(exit.exit_info, nebbie::exit_flag_defs());
            if (!exit_flags.isEmpty()) {
                parts << exit_flags;
            }
            if (exit.open_cmd >= 0) {
                parts << QString("open_cmd=%1").arg(exit.open_cmd);
            }
            lines << QString("  - %1").arg(parts.join(" | "));
        }
    }

    if (lines.isEmpty()) {
        return QStringLiteral("(nessuna configurazione attiva)");
    }
    return lines.join('\n');
}

} // namespace

void RoomEditorWidget::setComboIntValue(QComboBox* combo, const int value) const {
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

RoomEditorWidget::RoomEditorWidget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget;

    auto* overview_tab = new QWidget;
    auto* overview_layout = new QVBoxLayout(overview_tab);
    overview_layout->addWidget(makeLegend(
        "Riepilogo in tempo reale della stanza: mostra solo settori, flag, uscite e altri "
        "campi attivi o valorizzati. La chiave di una porta si configura nel tab Exits "
        "(campo Key vnum).",
        overview_tab));
    overview_ = new QTextEdit;
    overview_->setReadOnly(true);
    overview_->setLineWrapMode(QTextEdit::WidgetWidth);
    overview_->setAcceptRichText(false);
    configureTextField(overview_, 420);
    overview_layout->addWidget(overview_, 1);
    tabs->addTab(overview_tab, "Riepilogo");

    auto* text_tab = new QWidget;
    auto* text_layout = new QVBoxLayout(text_tab);
    name_ = new QLineEdit;
    configureLineField(name_);
    description_ = new QTextEdit;
    configureDescriptionField(description_);
    text_layout->addWidget(new QLabel("Name:"));
    text_layout->addWidget(name_);
    text_layout->addWidget(new QLabel("Description:"));
    text_layout->addWidget(description_, 1);
    tabs->addTab(text_tab, "Text");

    auto* sector_tab = new QWidget;
    auto* sector_layout = new QVBoxLayout(sector_tab);
    sector_layout->addWidget(makeLegend(
        "Sector type sets terrain and weather behavior. River fields apply to Water NoSwim (7) and "
        "Underwater (8) in this file format. TUNNEL flag enables moblim (max mobiles in room).",
        sector_tab));
    auto* sector_form = new QFormLayout;
    sector_type_ = new QComboBox;
    fillCombo(sector_type_, nebbie::room_sector_choices());
    room_flags_ = new FlagGroupWidget(nebbie::room_flag_defs(), sector_tab);
    sector_form->addRow("Sector type:", sector_type_);
    sector_layout->addLayout(sector_form);
    sector_layout->addWidget(new QLabel("Room flags"));
    sector_layout->addWidget(room_flags_);
    tabs->addTab(new QScrollArea, "Sector / Flags");
    {
        auto* scroll = qobject_cast<QScrollArea*>(tabs->widget(2));
        scroll->setWidgetResizable(true);
        scroll->setWidget(sector_tab);
    }

    auto* tele_tab = new QWidget;
    auto* tele_layout = new QVBoxLayout(tele_tab);
    tele_layout->addWidget(makeLegend(
        "Teleport rooms write sector -1 in myst.wld. tele_time / tele_targ / tele_mask control the "
        "teleport. If tele_mask bit 0 (TELE_COUNT) is set, tele_cnt is written as an extra field.",
        tele_tab));
    auto* tele_form = new QFormLayout;
    tele_time_ = new QSpinBox;
    tele_targ_ = new QSpinBox;
    tele_mask_ = new QSpinBox;
    tele_cnt_ = new QSpinBox;
    for (QSpinBox* spin : {tele_time_, tele_targ_, tele_mask_, tele_cnt_}) {
        spin->setRange(0, 2000000000);
    }
    tele_form->addRow("tele_time:", tele_time_);
    tele_form->addRow("tele_targ:", tele_targ_);
    tele_form->addRow("tele_mask:", tele_mask_);
    tele_form->addRow("tele_cnt:", tele_cnt_);
    tele_layout->addLayout(tele_form);
    tabs->addTab(tele_tab, "Teleport");

    auto* env_tab = new QWidget;
    auto* env_layout = new QVBoxLayout(env_tab);
    river_panel_ = new QWidget(env_tab);
    auto* river_form = new QFormLayout(river_panel_);
    river_speed_ = new QSpinBox;
    river_dir_ = new QSpinBox;
    river_speed_->setRange(0, 2000000000);
    river_dir_->setRange(0, 5);
    river_dir_->setToolTip("River direction 0=north .. 5=down (Arcane redit).");
    river_form->addRow("River speed:", river_speed_);
    river_form->addRow("River direction (0-5):", river_dir_);
    moblim_panel_ = new QWidget(env_tab);
    auto* moblim_form = new QFormLayout(moblim_panel_);
    moblim_ = new QSpinBox;
    moblim_->setRange(1, 2000000000);
    moblim_form->addRow("moblim (TUNNEL):", moblim_);
    bright_at_night_ = new QLineEdit;
    bright_at_day_ = new QLineEdit;
    configureLineField(bright_at_night_);
    configureLineField(bright_at_day_);
    auto* bright_form = new QFormLayout;
    bright_form->addRow("Bright at night (L):", bright_at_night_);
    bright_form->addRow("Bright at day (L):", bright_at_day_);
    env_layout->addWidget(makeLegend(
        "River lines are saved for water sectors. moblim is saved when TUNNEL room flag is set. "
        "L section stores optional brightness strings.",
        env_tab));
    env_layout->addWidget(river_panel_);
    env_layout->addWidget(moblim_panel_);
    env_layout->addLayout(bright_form);
    tabs->addTab(env_tab, "Environment");

    auto* extra_tab = new QWidget;
    auto* extra_layout = new QVBoxLayout(extra_tab);
    extra_desc_list_ = new QListWidget;
    extra_desc_keyword_ = new QLineEdit;
    configureLineField(extra_desc_keyword_);
    extra_desc_description_ = new QTextEdit;
    configureTextField(extra_desc_description_, 90);
    auto* extra_desc_form = new QFormLayout;
    extra_desc_form->addRow("Keyword:", extra_desc_keyword_);
    extra_desc_form->addRow("Description:", extra_desc_description_);
    auto* extra_desc_buttons = new QHBoxLayout;
    auto* extra_desc_add = new QPushButton("Add");
    auto* extra_desc_remove = new QPushButton("Remove");
    extra_desc_buttons->addWidget(extra_desc_add);
    extra_desc_buttons->addWidget(extra_desc_remove);
    extra_desc_buttons->addStretch();
    extra_layout->addWidget(new QLabel("Extra descriptions (section E)"));
    extra_layout->addWidget(extra_desc_list_, 1);
    extra_layout->addLayout(extra_desc_form);
    extra_layout->addLayout(extra_desc_buttons);
    tabs->addTab(extra_tab, "Extra descriptions");

    auto* exit_tab = new QWidget;
    auto* exit_layout = new QVBoxLayout(exit_tab);
    exit_layout->addWidget(makeLegend(
        "Exit directions: 0=north, 1=east, 2=south, 3=west, 4=up, 5=down. Description is "
        "usually the destination room name (shown when looking that way). Keyword is the door "
        "name used by open/close/unlock commands. Key vnum is the object number that unlocks a "
        "locked door (-1 = no key). Renaming a room updates inbound exit labels automatically; "
        "use Allinea uscite in entrata for manual myst.wld edits.",
        exit_tab));
    exit_list_ = new QListWidget;
    exit_list_->setMaximumHeight(140);
    exit_direction_ = new QComboBox;
    fillCombo(exit_direction_, nebbie::exit_direction_choices());
    exit_to_room_ = new QSpinBox;
    exit_to_room_->setRange(0, 999999);
    exit_description_ = new QLineEdit;
    exit_keyword_ = new QLineEdit;
    configureLineField(exit_description_);
    configureLineField(exit_keyword_);
    exit_flags_ = new FlagGroupWidget(nebbie::exit_flag_defs(), exit_tab);
    exit_key_ = new QSpinBox;
    exit_key_->setRange(-1, 999999);
    exit_key_->setValue(-1);
    exit_key_->setToolTip("VNUM dell'oggetto chiave per porte bloccate. Usare -1 se la porta non richiede chiave.");
    exit_open_cmd_ = new QSpinBox;
    exit_open_cmd_->setRange(-1, 999999);
    exit_open_cmd_->setValue(-1);
    exit_open_cmd_->setToolTip("Comando open_cmd opzionale scritto in myst.wld; -1 = omesso.");
    auto* exit_form = new QFormLayout;
    exit_form->addRow("Direction:", exit_direction_);
    exit_form->addRow("To room #:", exit_to_room_);
    exit_form->addRow("Description:", exit_description_);
    exit_form->addRow("Keyword (porta):", exit_keyword_);
    exit_form->addRow("Exit flags (exit_info):", exit_flags_);
    exit_form->addRow("Key vnum (oggetto):", exit_key_);
    exit_form->addRow("open_cmd:", exit_open_cmd_);
    auto* exit_buttons = new QHBoxLayout;
    auto* exit_apply = new QPushButton("Add / update exit");
    auto* exit_remove = new QPushButton("Remove exit");
    exit_buttons->addWidget(exit_apply);
    exit_buttons->addWidget(exit_remove);
    exit_buttons->addStretch();
    exit_layout->addWidget(exit_list_);
    exit_layout->addLayout(exit_form);
    exit_layout->addLayout(exit_buttons);
    tabs->addTab(exit_tab, "Exits");

    root->addWidget(tabs);

    connect(sector_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { updateConditionalFields(); });
    connect(room_flags_, &FlagGroupWidget::valueChanged, this, [this]() { updateConditionalFields(); });
    connect(extra_desc_list_, &QListWidget::currentRowChanged, this, [this](int) { onExtraDescSelected(); });
    connect(extra_desc_add, &QPushButton::clicked, this, &RoomEditorWidget::addExtraDesc);
    connect(extra_desc_remove, &QPushButton::clicked, this, &RoomEditorWidget::removeExtraDesc);
    connect(exit_list_, &QListWidget::currentRowChanged, this, [this](int) { onExitSelected(); });
    connect(exit_apply, &QPushButton::clicked, this, &RoomEditorWidget::addOrUpdateExit);
    connect(exit_remove, &QPushButton::clicked, this, &RoomEditorWidget::removeExit);

    connectOverviewUpdates();

    river_panel_->setVisible(false);
    moblim_panel_->setVisible(false);
}

void RoomEditorWidget::connectOverviewUpdates() {
    const auto refresh = [this]() { refreshOverview(); };

    connect(name_, &QLineEdit::textChanged, this, refresh);
    connect(description_, &QTextEdit::textChanged, this, refresh);
    connect(sector_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, refresh);
    connect(room_flags_, &FlagGroupWidget::valueChanged, this, refresh);
    for (QSpinBox* spin :
         {tele_time_, tele_targ_, tele_mask_, tele_cnt_, river_speed_, river_dir_, moblim_,
          exit_to_room_, exit_key_, exit_open_cmd_}) {
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, refresh);
    }
    connect(bright_at_night_, &QLineEdit::textChanged, this, refresh);
    connect(bright_at_day_, &QLineEdit::textChanged, this, refresh);
    connect(extra_desc_keyword_, &QLineEdit::textChanged, this, refresh);
    connect(extra_desc_description_, &QTextEdit::textChanged, this, refresh);
    connect(exit_direction_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, refresh);
    connect(exit_description_, &QLineEdit::textChanged, this, refresh);
    connect(exit_keyword_, &QLineEdit::textChanged, this, refresh);
    connect(exit_flags_, &FlagGroupWidget::valueChanged, this, refresh);

    if (extra_desc_list_->model() != nullptr) {
        connect(extra_desc_list_->model(), &QAbstractItemModel::rowsInserted, this, refresh);
        connect(extra_desc_list_->model(), &QAbstractItemModel::rowsRemoved, this, refresh);
        connect(extra_desc_list_->model(), &QAbstractItemModel::dataChanged, this, refresh);
        connect(extra_desc_list_->model(), &QAbstractItemModel::modelReset, this, refresh);
    }
    if (exit_list_->model() != nullptr) {
        connect(exit_list_->model(), &QAbstractItemModel::rowsInserted, this, refresh);
        connect(exit_list_->model(), &QAbstractItemModel::rowsRemoved, this, refresh);
        connect(exit_list_->model(), &QAbstractItemModel::dataChanged, this, refresh);
        connect(exit_list_->model(), &QAbstractItemModel::modelReset, this, refresh);
    }
}

void RoomEditorWidget::refreshOverview() {
    if (loading_ || overview_ == nullptr) {
        return;
    }

    nebbie::Room snapshot;
    saveToRoom(snapshot);

    const int selected_row = exit_list_->currentRow();
    if (selected_row >= 0) {
        nebbie::Exit pending;
        pending.direction = comboIntValue(exit_direction_);
        pending.to_room = exit_to_room_->value();
        pending.description = exit_description_->text().toStdString();
        pending.keyword = exit_keyword_->text().toStdString();
        pending.exit_info = exit_flags_->value();
        pending.key = exit_key_->value();
        pending.open_cmd = exit_open_cmd_->value();

        bool replaced = false;
        for (auto& exit : snapshot.exits) {
            if (exit.direction == pending.direction) {
                exit = pending;
                replaced = true;
                break;
            }
        }
        if (!replaced && pending.to_room > 0) {
            snapshot.exits.push_back(pending);
        }
    }

    overview_->setPlainText(buildOverviewText(snapshot));
}

void RoomEditorWidget::updateConditionalFields() {
    const int sector = comboIntValue(sector_type_);
    river_panel_->setVisible(nebbie::room_sector_uses_river(sector));
    moblim_panel_->setVisible(nebbie::room_flags_use_moblim(room_flags_->value()));
}

void RoomEditorWidget::loadFromRoom(const nebbie::Room& room) {
    loading_ = true;
    name_->setText(QString::fromStdString(room.name));
    description_->setPlainText(QString::fromStdString(room.description));
    setComboIntValue(sector_type_, static_cast<int>(room.sector_type));
    room_flags_->setValue(room.room_flags);

    tele_time_->setValue(static_cast<int>(room.tele_time));
    tele_targ_->setValue(static_cast<int>(room.tele_targ));
    tele_mask_->setValue(static_cast<int>(room.tele_mask));
    tele_cnt_->setValue(static_cast<int>(room.tele_cnt));

    river_speed_->setValue(static_cast<int>(room.river_speed));
    river_dir_->setValue(static_cast<int>(room.river_dir));
    moblim_->setValue(room.moblim > 0 ? static_cast<int>(room.moblim) : 1);

    bright_at_night_->setText(QString::fromStdString(room.bright_at_night));
    bright_at_day_->setText(QString::fromStdString(room.bright_at_day));

    extra_desc_list_->clear();
    for (const auto& extra : room.extra_descs) {
        const QString label = QString::fromStdString(extra.keyword);
        auto* item = new QListWidgetItem(label.isEmpty() ? "(no keyword)" : label);
        item->setData(Qt::UserRole, QString::fromStdString(extra.keyword));
        item->setData(Qt::UserRole + 1, QString::fromStdString(extra.description));
        extra_desc_list_->addItem(item);
    }

    exit_list_->clear();
    for (const auto& exit : room.exits) {
        auto* item = new QListWidgetItem;
        writeExitItem(item, exit);
        exit_list_->addItem(item);
    }

    updateConditionalFields();
    loading_ = false;
    refreshOverview();
}

void RoomEditorWidget::saveToRoom(nebbie::Room& room) const {
    room.name = name_->text().toStdString();
    room.description = description_->toPlainText().toStdString();
    room.sector_type = comboIntValue(sector_type_);
    room.room_flags = room_flags_->value();

    room.tele_time = tele_time_->value();
    room.tele_targ = tele_targ_->value();
    room.tele_mask = tele_mask_->value();
    room.tele_cnt = tele_cnt_->value();

    if (nebbie::room_sector_uses_river(room.sector_type)) {
        room.river_speed = river_speed_->value();
        room.river_dir = river_dir_->value();
    } else {
        room.river_speed = 0;
        room.river_dir = 0;
    }

    if (nebbie::room_flags_use_moblim(room.room_flags)) {
        room.moblim = moblim_->value();
    } else {
        room.moblim = 0;
    }

    room.bright_at_night = bright_at_night_->text().toStdString();
    room.bright_at_day = bright_at_day_->text().toStdString();

    room.extra_descs.clear();
    for (int i = 0; i < extra_desc_list_->count(); ++i) {
        const auto* item = extra_desc_list_->item(i);
        nebbie::ExtraDesc extra;
        extra.keyword = item->data(Qt::UserRole).toString().toStdString();
        extra.description = item->data(Qt::UserRole + 1).toString().toStdString();
        room.extra_descs.push_back(std::move(extra));
    }

    room.exits.clear();
    for (int i = 0; i < exit_list_->count(); ++i) {
        room.exits.push_back(readExitItem(exit_list_->item(i)));
    }
}

long RoomEditorWidget::selectedExitToRoom() const {
    const auto* item = exit_list_->currentItem();
    if (!item) {
        return 0;
    }
    return readExitItem(item).to_room;
}

void RoomEditorWidget::onExtraDescSelected() {
    refreshExtraDescForm();
}

void RoomEditorWidget::addExtraDesc() {
    const QString keyword = extra_desc_keyword_->text().trimmed();
    const QString description = extra_desc_description_->toPlainText();
    if (keyword.isEmpty() && description.trimmed().isEmpty()) {
        return;
    }
    auto* item = new QListWidgetItem(keyword.isEmpty() ? "(no keyword)" : keyword);
    item->setData(Qt::UserRole, keyword);
    item->setData(Qt::UserRole + 1, description);
    extra_desc_list_->addItem(item);
    extra_desc_list_->setCurrentItem(item);
    extra_desc_keyword_->clear();
    extra_desc_description_->clear();
    refreshOverview();
}

void RoomEditorWidget::removeExtraDesc() {
    const int row = extra_desc_list_->currentRow();
    if (row < 0) {
        return;
    }
    delete extra_desc_list_->takeItem(row);
    refreshExtraDescForm();
    refreshOverview();
}

void RoomEditorWidget::refreshExtraDescForm() {
    const auto* item = extra_desc_list_->currentItem();
    if (!item) {
        extra_desc_keyword_->clear();
        extra_desc_description_->clear();
        return;
    }
    extra_desc_keyword_->setText(item->data(Qt::UserRole).toString());
    extra_desc_description_->setPlainText(item->data(Qt::UserRole + 1).toString());
}

void RoomEditorWidget::onExitSelected() {
    refreshExitForm();
}

void RoomEditorWidget::addOrUpdateExit() {
    nebbie::Exit exit;
    exit.direction = comboIntValue(exit_direction_);
    exit.to_room = exit_to_room_->value();
    exit.description = exit_description_->text().toStdString();
    exit.keyword = exit_keyword_->text().toStdString();
    exit.exit_info = exit_flags_->value();
    exit.key = exit_key_->value();
    exit.open_cmd = exit_open_cmd_->value();

    for (int i = 0; i < exit_list_->count(); ++i) {
        auto* item = exit_list_->item(i);
        if (item->data(Qt::UserRole).toInt() == exit.direction) {
            writeExitItem(item, exit);
            exit_list_->setCurrentItem(item);
            refreshOverview();
            return;
        }
    }

    auto* item = new QListWidgetItem;
    writeExitItem(item, exit);
    exit_list_->addItem(item);
    exit_list_->setCurrentItem(item);
    refreshOverview();
}

void RoomEditorWidget::removeExit() {
    const int row = exit_list_->currentRow();
    if (row < 0) {
        return;
    }
    delete exit_list_->takeItem(row);
    refreshExitForm();
    refreshOverview();
}

void RoomEditorWidget::refreshExitForm() {
    const auto* item = exit_list_->currentItem();
    if (!item) {
        exit_key_->setValue(-1);
        exit_open_cmd_->setValue(-1);
        refreshOverview();
        return;
    }
    const nebbie::Exit exit = readExitItem(item);
    setComboIntValue(exit_direction_, exit.direction);
    exit_to_room_->setValue(static_cast<int>(exit.to_room));
    exit_description_->setText(QString::fromStdString(exit.description));
    exit_keyword_->setText(QString::fromStdString(exit.keyword));
    exit_flags_->setValue(exit.exit_info);
    exit_key_->setValue(static_cast<int>(exit.key));
    exit_open_cmd_->setValue(static_cast<int>(exit.open_cmd));
    refreshOverview();
}
