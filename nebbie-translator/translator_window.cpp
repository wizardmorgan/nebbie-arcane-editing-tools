#include "translator_window.hpp"

#include "app_config.hpp"
#include "app_i18n.hpp"
#include "mud_color_common.hpp"
#include "mud_color_dialogs.hpp"
#include "mud_color_list_delegate.hpp"
#include "mud_color_widgets.hpp"
#include "translator_room_widget.hpp"
#include "validation_report_ui.hpp"

#include "path_util.hpp"

#include "nebbie/edit.hpp"
#include "nebbie/io.hpp"
#include "nebbie/validate.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QIcon>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

using nebbie::qt::appTr;

namespace {

void addListItem(QListWidget* list, long vnum, const QString& label) {
    list->addItem(label);
    list->item(list->count() - 1)->setData(Qt::UserRole, static_cast<qlonglong>(vnum));
}

void addRoomListItem(QListWidget* list, long vnum, const std::string& storage_name) {
    auto* item = new QListWidgetItem;
    nebbie::qt::setRoomListItemData(item, vnum, storage_name);
    list->addItem(item);
}

nebbie::qt::MudColorListDelegate* roomListDelegate(QListWidget* list) {
    return qobject_cast<nebbie::qt::MudColorListDelegate*>(list->itemDelegate());
}

} // namespace

TranslatorWindow::TranslatorWindow(QWidget* parent) : QMainWindow(parent) {
    app_config_ = nebbie::translate::read_config();
    nebbie::qt::setAppLanguage(nebbie::qt::parseLanguageCode(app_config_.ui_language));
    network_ = new QNetworkAccessManager(this);
    update_checker_ = new nebbie::qt::ReleaseUpdateChecker(nebbie::qt::ReleaseProduct::Cypher, this);
    connect(update_checker_, &nebbie::qt::ReleaseUpdateChecker::checkFinished, this,
            &TranslatorWindow::onUpdateCheckFinished);
    setupUi();
    setupMenus();
    retranslateUi();
    room_editor_->setMaxLineLength(app_config_.max_line_length);
    room_editor_->setShowColorCodes(app_config_.show_color_codes);
    if (auto* delegate = roomListDelegate(room_list_)) {
        delegate->setShowColorCodes(app_config_.show_color_codes);
    }
    setWindowIcon(QIcon(QStringLiteral(":/app-icon.png")));
    resize(960, 680);
    scheduleStartupUpdateCheck();
}

void TranslatorWindow::setupUi() {
    auto* central = new QWidget;
    auto* root_layout = new QVBoxLayout(central);

    lib_label_ = new QLabel;
    lib_label_->setWordWrap(true);
    root_layout->addWidget(lib_label_);

    auto* top = new QHBoxLayout;
    room_search_ = new QLineEdit;
    top->addWidget(room_search_, 1);
    root_layout->addLayout(top);

    auto* splitter = new QSplitter;
    room_list_ = new QListWidget;
    room_list_->setMinimumWidth(200);
    room_list_->setItemDelegate(new nebbie::qt::MudColorListDelegate(room_list_));
    splitter->addWidget(room_list_);

    auto* editor_panel = new QWidget;
    auto* editor_layout = new QVBoxLayout(editor_panel);
    editor_layout->setContentsMargins(0, 0, 0, 0);
    auto* room_scroll = new QScrollArea;
    room_scroll->setWidgetResizable(true);
    room_editor_ = new TranslatorRoomWidget;
    room_scroll->setWidget(room_editor_);
    editor_layout->addWidget(room_scroll, 1);

    auto* buttons = new QHBoxLayout;
    auto* apply_button = new QPushButton;
    apply_button_ = apply_button;
    buttons->addWidget(apply_button);
    buttons->addStretch();
    editor_layout->addLayout(buttons);
    splitter->addWidget(editor_panel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    splitter->setSizes({240, 720});
    root_layout->addWidget(splitter, 1);

    setCentralWidget(central);

    autosave_timer_ = new QTimer(this);
    autosave_timer_->setInterval(session_config_.autosave_interval_sec * 1000);
    connect(autosave_timer_, &QTimer::timeout, this, &TranslatorWindow::onAutosaveTick);
    connect(room_list_, &QListWidget::currentRowChanged, this, &TranslatorWindow::onRoomSelected);
    connect(room_search_, &QLineEdit::textChanged, this, &TranslatorWindow::onRoomSearchChanged);
    connect(apply_button, &QPushButton::clicked, this, &TranslatorWindow::applyRoomChanges);
}

void TranslatorWindow::setupMenus() {
    auto* file_menu = menuBar()->addMenu(appTr("menu.file"));

    auto* open_action = file_menu->addAction(appTr("menu.open_lib"));
    open_action->setShortcut(QKeySequence::Open);
    connect(open_action, &QAction::triggered, this, &TranslatorWindow::openLib);

    auto* reload_action = file_menu->addAction(appTr("menu.reload_lib"));
    reload_action->setShortcut(QKeySequence::Refresh);
    reload_action->setToolTip(appTr("menu.reload_lib_tip"));
    connect(reload_action, &QAction::triggered, this, &TranslatorWindow::reloadLib);

    auto* save_action = file_menu->addAction(appTr("menu.save"));
    save_action->setShortcut(QKeySequence::Save);
    connect(save_action, &QAction::triggered, this, &TranslatorWindow::saveLib);

    auto* save_force_action = file_menu->addAction(appTr("menu.save_force"));
    connect(save_force_action, &QAction::triggered, this, &TranslatorWindow::saveLibForce);

    file_menu->addSeparator();
    file_menu->addAction(appTr("menu.exit"), this, &QWidget::close);

    auto* tools_menu = menuBar()->addMenu(appTr("menu.tools"));
    auto* validate_action = tools_menu->addAction(appTr("menu.validate"));
    validate_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(validate_action, &QAction::triggered, this, &TranslatorWindow::validateLib);

    auto* prefs_menu = menuBar()->addMenu(appTr("menu.prefs"));
    auto* line_limit_action = prefs_menu->addAction(appTr("menu.line_limit"));
    connect(line_limit_action, &QAction::triggered, this, &TranslatorWindow::editLineLengthLimit);

    auto* extended_color_action = prefs_menu->addAction(appTr("menu.extended_colors"));
    extended_color_action->setCheckable(true);
    extended_color_action->setChecked(app_config_.show_color_codes);
    connect(extended_color_action, &QAction::toggled, this, &TranslatorWindow::toggleExtendedColorView);

    auto* language_menu = prefs_menu->addMenu(appTr("menu.language"));
    auto* language_group = new QActionGroup(this);
    language_group->setExclusive(true);
    auto* italian_action = language_menu->addAction(appTr("menu.language_it"));
    italian_action->setCheckable(true);
    italian_action->setData(static_cast<int>(nebbie::qt::AppLanguage::Italian));
    language_group->addAction(italian_action);
    auto* english_action = language_menu->addAction(appTr("menu.language_en"));
    english_action->setCheckable(true);
    english_action->setData(static_cast<int>(nebbie::qt::AppLanguage::English));
    language_group->addAction(english_action);
    if (nebbie::qt::currentAppLanguage() == nebbie::qt::AppLanguage::English) {
        english_action->setChecked(true);
    } else {
        italian_action->setChecked(true);
    }
    connect(language_group, &QActionGroup::triggered, this, [this](QAction* action) {
        setUiLanguage(static_cast<nebbie::qt::AppLanguage>(action->data().toInt()));
    });

    auto* colors_menu = menuBar()->addMenu(appTr("menu.colors"));
    auto* legend_action = colors_menu->addAction(appTr("menu.color_legend"));
    connect(legend_action, &QAction::triggered, this, &TranslatorWindow::showColorLegend);
    auto* insert_color_action = colors_menu->addAction(appTr("menu.insert_color"));
    connect(insert_color_action, &QAction::triggered, this, &TranslatorWindow::insertColorCode);

    auto* help_menu = menuBar()->addMenu(appTr("menu.help"));
    auto* check_updates_action = help_menu->addAction(appTr("menu.check_updates"));
    connect(check_updates_action, &QAction::triggered, this, &TranslatorWindow::checkForUpdates);
    auto* auto_updates_action = help_menu->addAction(appTr("menu.check_updates_startup"));
    auto_updates_action->setCheckable(true);
    auto_updates_action->setChecked(app_config_.check_updates);
    connect(auto_updates_action, &QAction::toggled, this, &TranslatorWindow::toggleCheckUpdatesOnStartup);
    help_menu->addSeparator();
    help_menu->addAction(appTr("menu.about_cypher"), this, [this]() {
        QMessageBox::about(this,
                           nebbie::qt::cypherDisplayName(),
                           nebbie::qt::cypherAboutText(QApplication::applicationVersion()));
    });
}

void TranslatorWindow::retranslateUi() {
    updateBranding();
    if (lib_path_.empty()) {
        lib_label_->setText(appTr("status.no_lib"));
        setStatus(appTr("status.open_lib_translate"));
    }
    room_search_->setPlaceholderText(appTr("ui.search_room"));
    apply_button_->setText(appTr("ui.apply_changes"));
    if (lib_path_.empty() && statusBar()->currentMessage().isEmpty()) {
        statusBar()->showMessage(appTr("status.ready"));
    }
}

void TranslatorWindow::updateBranding() {
    QString title = nebbie::qt::cypherWindowTitle();
    if (dirty_) {
        title = QStringLiteral("* ") + title;
    }
    setWindowTitle(title);
    QApplication::setApplicationDisplayName(nebbie::qt::cypherWindowTitle());
}

void TranslatorWindow::setUiLanguage(const nebbie::qt::AppLanguage language) {
    if (nebbie::qt::currentAppLanguage() == language) {
        return;
    }
    nebbie::qt::setAppLanguage(language);
    app_config_.ui_language = nebbie::qt::languageCode(language);
    nebbie::translate::write_config(app_config_);
    menuBar()->clear();
    setupMenus();
    retranslateUi();
}

void TranslatorWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

void TranslatorWindow::openLibPath(const QString& path) {
    if (path.isEmpty()) {
        return;
    }
    if (!lib_path_.empty()) {
        const std::filesystem::path requested = nebbie::qt::path_from_qstring(path);
        const std::filesystem::path resolved = nebbie::resolve_lib_directory(requested);
        if (resolved != lib_path_ && !confirmSaveIfDirty()) {
            return;
        }
    }
    try {
        const std::filesystem::path requested = nebbie::qt::path_from_qstring(path);
        const std::filesystem::path resolved = nebbie::resolve_lib_directory(requested);
        if (!std::filesystem::exists(resolved)) {
            QMessageBox::warning(this, "Apri libreria",
                                 QString("Percorso non valido:\n%1").arg(path));
            return;
        }
        if (!nebbie::directory_has_lib_files(resolved)) {
            QMessageBox::warning(
                this,
                "Apri libreria",
                QString("La cartella selezionata non contiene file libreria Nebbie "
                        "(.zon, .wld, .mob, .obj, ...).\n\n"
                        "Percorso richiesto: %1\n"
                        "Percorso risolto: %2")
                    .arg(path)
                    .arg(nebbie::qt::qstring_from_path(resolved)));
            return;
        }
        if (requested != resolved) {
            setStatus(QString("Libreria risolta in: %1")
                          .arg(nebbie::qt::qstring_from_path(resolved)));
        }
        loadLib(resolved);
        rememberLibPath(resolved);
        if (!context_.load_warnings.empty()) {
            QString warning_text;
            for (const std::string& warning : context_.load_warnings) {
                if (!warning_text.isEmpty()) {
                    warning_text += "\n\n";
                }
                warning_text += QString::fromUtf8(warning.c_str());
            }
            QMessageBox::warning(
                this,
                "Avvisi caricamento libreria",
                QString("La libreria e' stata aperta, ma alcuni file monolitici opzionali "
                        "non sono stati caricati:\n\n%1")
                    .arg(warning_text));
        }
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Apri libreria", QString::fromUtf8(ex.what()));
    }
}

void TranslatorWindow::openLib() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Apri libreria Nebbie (mudroot o mudroot/lib)", nebbie::translate::read_lib_path(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) {
        return;
    }
    openLibPath(dir);
}

void TranslatorWindow::reloadLib() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Aggiorna libreria", "Apri prima una libreria.");
        return;
    }
    if (!confirmSaveIfDirty()) {
        return;
    }

    try {
        loadLib(lib_path_);
        setStatus(QString("Libreria ricaricata da %1.")
                      .arg(nebbie::qt::qstring_from_path(lib_path_)));
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Aggiorna libreria", QString::fromUtf8(ex.what()));
    }
}

void TranslatorWindow::openStartupLib() {
    const QString saved = nebbie::translate::read_lib_path();
    if (nebbie::translate::lib_path_exists(saved)) {
        openLibPath(saved);
        return;
    }

    if (!saved.isEmpty()) {
        promptForLibPath(QString("Il percorso salvato non è più valido:\n%1").arg(saved));
        return;
    }

    promptForLibPath(QString(
        "Benvenuto in Nebbie Translate.\n\n"
        "Seleziona la cartella della libreria di gioco (mudroot o mudroot/lib).\n"
        "Il percorso verrà salvato in:\n%1")
                         .arg(nebbie::translate::default_config_path()));
}

bool TranslatorWindow::promptForLibPath(const QString& reason) {
    if (!reason.isEmpty()) {
        QMessageBox::information(this, "Nebbie Translate", reason);
    }

    const QString initial = nebbie::translate::read_lib_path();
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        "Seleziona libreria Nebbie (mudroot o mudroot/lib)",
        initial.isEmpty() ? QDir::homePath() : initial,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) {
        setStatus("Nessuna libreria selezionata. Usa File → Apri libreria.");
        return false;
    }

    openLibPath(dir);
    return !lib_path_.empty();
}

void TranslatorWindow::loadLib(const std::filesystem::path& path) {
    nebbie::World loaded_world;
    nebbie::LibContext loaded_context;
    nebbie::load_lib(loaded_world, path, loaded_context);

    world_ = std::move(loaded_world);
    context_ = std::move(loaded_context);
    lib_path_ = path;
    room_filter_.clear();
    room_search_->clear();
    markClean();
    refreshRoomList();

    lib_label_->setText(QString("Libreria: %1 — %2 zone, %3 stanze")
                            .arg(nebbie::qt::qstring_from_path(path))
                            .arg(world_.zones.size())
                            .arg(world_.rooms.size()));
    last_version_time_ = std::chrono::system_clock::now();
    autosave_timer_->start();
    setStatus(QString("Libreria caricata: %1 stanze.").arg(world_.rooms.size()));
}

void TranslatorWindow::refreshRoomList() {
    const long selected = currentRoomVnum();
    room_list_->clear();
    const std::string query = room_filter_.toStdString();
    for (const auto& [vnum, room] : world_.rooms) {
        if (!nebbie::entity_matches(vnum, room.name, query)) {
            continue;
        }
        addRoomListItem(room_list_, vnum, room.name);
    }
    if (selected > 0) {
        selectRoomByVnum(selected);
    } else if (room_list_->count() > 0 && room_list_->currentRow() < 0) {
        room_list_->setCurrentRow(0);
    }
}

void TranslatorWindow::selectRoomByVnum(const long vnum) {
    for (int i = 0; i < room_list_->count(); ++i) {
        if (room_list_->item(i)->data(Qt::UserRole).toLongLong() == vnum) {
            room_list_->setCurrentRow(i);
            return;
        }
    }
}

long TranslatorWindow::currentRoomVnum() const {
    const auto* item = room_list_->currentItem();
    if (!item) {
        return -1;
    }
    return static_cast<long>(item->data(Qt::UserRole).toLongLong());
}

void TranslatorWindow::onRoomSelected() {
    const long vnum = currentRoomVnum();
    if (vnum <= 0) {
        return;
    }
    const nebbie::Room* room = world_.find_room(vnum);
    if (!room) {
        return;
    }
    room_editor_->loadFromRoom(*room);
}

void TranslatorWindow::onRoomSearchChanged(const QString& text) {
    room_filter_ = text;
    refreshRoomList();
}

void TranslatorWindow::applyRoomChanges() {
    const long vnum = currentRoomVnum();
    if (vnum <= 0) {
        QMessageBox::information(this, "Stanze", "Seleziona una stanza.");
        return;
    }

    nebbie::Room* room = world_.find_room(vnum);
    if (!room) {
        QMessageBox::warning(this, "Stanze", "Stanza non trovata.");
        return;
    }

    const std::string old_name = room->name;
    nebbie::Room updated = *room;
    room_editor_->saveTranslatableFields(*room, updated);
    nebbie::assign_room_fields(*room, updated);

    std::size_t aligned = 0;
    if (old_name != room->name) {
        aligned = nebbie::refresh_inbound_exit_descriptions(
            world_, vnum, nebbie::InboundExitAlignPolicy::SyncDestinationName, &old_name);
    }

    if (auto* item = room_list_->currentItem()) {
        nebbie::qt::refreshRoomListItemData(item, vnum, room->name);
    }

    markDirty();
    dirty_room_vnums_.insert(vnum);
    if (aligned > 0) {
        setStatus(QString("Stanza %1 aggiornata; %2 description collegate sincronizzate.")
                      .arg(vnum)
                      .arg(static_cast<qlonglong>(aligned)));
    } else {
        setStatus(QString("Stanza %1 aggiornata in memoria.").arg(vnum));
    }
}

void TranslatorWindow::validateLib() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Valida", "Apri prima una libreria.");
        return;
    }

    const nebbie::ValidationReport report = nebbie::validate_world(world_, validationOptions());
    showValidation(report);
    if (report.ok()) {
        if (report.warning_count() > 0) {
            setStatus(QString("Validazione OK con %1 avvisi.").arg(report.warning_count()));
        } else {
            setStatus("Validazione OK.");
        }
    } else {
        setStatus(QString("Validazione: %1 errori.").arg(report.error_count()));
    }
}

void TranslatorWindow::showValidation(const nebbie::ValidationReport& report) {
    const QString library = lib_path_.empty() ? QString() : nebbie::qt::qstring_from_path(lib_path_);
    nebbie::qt::show_validation_report_dialog(
        this,
        report,
        QStringLiteral("Validazione"),
        QStringLiteral("Report validazione Nebbie Translate"),
        library,
        nebbie::qt::translate_validation_log_path());
}

nebbie::ValidationOptions TranslatorWindow::validationOptions() const {
    nebbie::ValidationOptions options;
    options.max_line_length = app_config_.max_line_length;
    return options;
}

std::vector<long> TranslatorWindow::roomsPendingSaveValidation() const {
    if (!dirty_room_vnums_.empty()) {
        return {dirty_room_vnums_.begin(), dirty_room_vnums_.end()};
    }
    const long current = currentRoomVnum();
    if (current > 0) {
        return {current};
    }
    return {};
}

void TranslatorWindow::saveLib() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Salva", "Apri prima una libreria.");
        return;
    }

    const std::vector<long> rooms_to_check = roomsPendingSaveValidation();
    const nebbie::ValidationReport report =
        nebbie::validate_translatable_rooms(world_, validationOptions(), &rooms_to_check);
    if (report.warning_count() > 0) {
        showValidation(report);
        setStatus(QString("Avvisi salvati in %1").arg(nebbie::qt::translate_validation_log_path()));
        const auto answer = QMessageBox::question(
            this, "Avvisi sui testi modificati",
            QString("Ci sono %1 avvisi sui testi delle stanze modificate in questa sessione. "
                    "Salvare comunque?\n\n"
                    "(Gli errori strutturali del mondo — uscite mancanti, reset, ecc. — "
                    "non bloccano il salvataggio da Translate.)")
                .arg(report.warning_count()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    try {
        nebbie::save_lib_with_backup(world_, context_, lib_path_);
        markClean();
        last_version_time_ = std::chrono::system_clock::now();
        setStatus("Libreria salvata (backup in .nebbie/versions).");
        QMessageBox::information(this, "Salva", "Salvataggio completato.");
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Errore", QString::fromUtf8(ex.what()));
    }
}

void TranslatorWindow::saveLibForce() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Salva", "Apri prima una libreria.");
        return;
    }
    try {
        nebbie::save_lib_with_backup(world_, context_, lib_path_);
        markClean();
        last_version_time_ = std::chrono::system_clock::now();
        setStatus("Libreria salvata (forzato).");
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Errore", QString::fromUtf8(ex.what()));
    }
}

void TranslatorWindow::onAutosaveTick() {
    if (!dirty_ || lib_path_.empty()) {
        return;
    }

    try {
        const auto result = nebbie::run_autosave(world_, context_, lib_path_, session_config_, last_version_time_);
        if (result.version_created) {
            last_version_time_ = std::chrono::system_clock::now();
        }
        const QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
        if (result.version_created) {
            setStatus(QString("Autosalvataggio + versione %1 (%2)")
                          .arg(QString::fromStdString(result.version_id))
                          .arg(time));
        } else {
            setStatus(QString("Autosalvataggio workspace (%1)").arg(time));
        }
    } catch (const std::exception& ex) {
        setStatus(QString("Autosalvataggio fallito: %1").arg(QString::fromUtf8(ex.what())));
    }
}

void TranslatorWindow::rememberLibPath(const std::filesystem::path& path) {
    app_config_.lib_path = nebbie::qt::qstring_from_path(path);
    nebbie::translate::write_config(app_config_);
}

void TranslatorWindow::toggleExtendedColorView(bool enabled) {
    app_config_.show_color_codes = enabled;
    nebbie::translate::write_config(app_config_);
    room_editor_->setShowColorCodes(enabled);
    if (auto* delegate = roomListDelegate(room_list_)) {
        delegate->setShowColorCodes(enabled);
        room_list_->viewport()->update();
    }
    if (enabled) {
        setStatus(appTr("status.extended_on"));
    } else {
        setStatus(appTr("status.extended_off"));
    }
}

void TranslatorWindow::showColorLegend() {
    nebbie::qt::MudColorLegendDialog dialog(this);
    dialog.exec();
}

void TranslatorWindow::insertColorCode() {
    nebbie::qt::MudColorInsertDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (auto* field = room_editor_->focusedMudField()) {
        field->insertColorCode(dialog.selectedCode());
        setStatus(QString("Inserito codice %1.").arg(dialog.selectedCode()));
    }
}

void TranslatorWindow::scheduleStartupUpdateCheck() {
    if (!nebbie::qt::shouldCheckForUpdates(app_config_.check_updates,
                                           false,
                                           app_config_.last_update_check)) {
        return;
    }
    QTimer::singleShot(3000, this, [this]() {
        update_checker_->checkForUpdates(*network_,
                                         false,
                                         this,
                                         app_config_.dismissed_update_version);
    });
}

void TranslatorWindow::checkForUpdates() {
    update_checker_->checkForUpdates(*network_,
                                     true,
                                     this,
                                     app_config_.dismissed_update_version);
}

void TranslatorWindow::onUpdateCheckFinished(const nebbie::qt::ReleaseUpdateInfo& info) {
    if (!info.ok) {
        return;
    }
    app_config_.last_update_check = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!info.user_dismissed_version.isEmpty()) {
        app_config_.dismissed_update_version = info.user_dismissed_version;
    }
    nebbie::translate::write_config(app_config_);
}

void TranslatorWindow::toggleCheckUpdatesOnStartup(const bool enabled) {
    app_config_.check_updates = enabled;
    nebbie::translate::write_config(app_config_);
    if (enabled) {
        setStatus("Controllo aggiornamenti all'avvio attivo.");
    } else {
        setStatus("Controllo aggiornamenti all'avvio disattivato.");
    }
}

void TranslatorWindow::editLineLengthLimit() {
    bool ok = false;
    const int value = QInputDialog::getInt(
        this,
        "Limite caratteri per riga",
        "Numero massimo di caratteri per riga nei testi traducibili (Windows, Linux, macOS).\n"
        "Imposta 0 per disattivare il controllo.\n\n"
        "Salvato in:\n" + nebbie::translate::default_config_path(),
        app_config_.max_line_length,
        0,
        512,
        1,
        &ok);
    if (!ok) {
        return;
    }

    app_config_.max_line_length = value;
    nebbie::translate::write_config(app_config_);
    room_editor_->setMaxLineLength(app_config_.max_line_length);
    if (app_config_.max_line_length > 0) {
        setStatus(QString("Limite righe impostato a %1 caratteri.").arg(app_config_.max_line_length));
    } else {
        setStatus("Controllo lunghezza righe disattivato.");
    }
}

void TranslatorWindow::markDirty() {
    dirty_ = true;
    updateBranding();
}

void TranslatorWindow::markClean() {
    dirty_ = false;
    dirty_room_vnums_.clear();
    updateBranding();
}

bool TranslatorWindow::confirmSaveIfDirty() {
    if (!dirty_) {
        return true;
    }

    const auto answer = QMessageBox::question(
        this, "Modifiche non salvate",
        "Ci sono modifiche non salvate. Salvare prima di continuare?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) {
        return false;
    }
    if (answer == QMessageBox::Save) {
        saveLib();
        return !dirty_;
    }
    markClean();
    return true;
}

void TranslatorWindow::setStatus(const QString& text) {
    statusBar()->showMessage(text);
}

void TranslatorWindow::closeEvent(QCloseEvent* event) {
    if (!confirmSaveIfDirty()) {
        event->ignore();
        return;
    }
    event->accept();
}
