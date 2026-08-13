#include "main_window.hpp"

#include "app_config.hpp"
#include "coordinator_client.hpp"
#include "mob_editor_widget.hpp"
#include "obj_editor_widget.hpp"
#include "room_editor_widget.hpp"
#include "zone_editor_widget.hpp"
#include "world_zone_map_widget.hpp"
#include "zone_map_widget.hpp"
#include "path_util.hpp"
#include "nebbie/edit.hpp"
#include "nebbie/overlay_io.hpp"
#include "nebbie/io.hpp"
#include "nebbie/validate.hpp"
#include "nebbie/world_index.hpp"
#include "nebbie/zone_graph.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QNetworkAccessManager>
#include <QTabWidget>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QDateTime>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <stdexcept>

namespace {

struct EntityPageWidgets {
    QWidget* page = nullptr;
    QLineEdit* search = nullptr;
    QPushButton* create_button = nullptr;
    QListWidget* list = nullptr;
};

EntityPageWidgets makeEntityPage(QWidget* editor, const QString& create_label) {
    EntityPageWidgets widgets;
    widgets.page = new QWidget;
    auto* layout = new QVBoxLayout(widgets.page);

    auto* top = new QHBoxLayout;
    widgets.search = new QLineEdit;
    widgets.search->setPlaceholderText("Cerca vnum o nome...");
    widgets.create_button = new QPushButton(create_label);
    top->addWidget(widgets.search, 1);
    top->addWidget(widgets.create_button);
    layout->addLayout(top);

    auto* splitter = new QSplitter;
    widgets.list = new QListWidget;
    widgets.list->setMinimumWidth(180);
    splitter->addWidget(widgets.list);
    splitter->addWidget(editor);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    splitter->setSizes({220, 680});
    layout->addWidget(splitter, 1);
    return widgets;
}

void addListItem(QListWidget* list, long vnum, const QString& label) {
    list->addItem(label);
    list->item(list->count() - 1)->setData(Qt::UserRole, static_cast<qlonglong>(vnum));
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    app_config_ = nebbie::qt::read_config();
    network_ = new QNetworkAccessManager(this);
    setupUi();
    setupMenus();
    setWindowTitle("Nebbie Editor");
    resize(1100, 720);
    setStatus("Apri una libreria (mudroot/lib) per iniziare.");
}

void MainWindow::setupUi() {
    auto* central = new QWidget;
    auto* root_layout = new QVBoxLayout(central);

    lib_label_ = new QLabel("Nessuna libreria aperta");
    lib_label_->setWordWrap(true);
    root_layout->addWidget(lib_label_);

    tabs_ = new QTabWidget;

    auto* room_scroll = new QScrollArea;
    room_scroll->setWidgetResizable(true);
    room_editor_ = new RoomEditorWidget;
    room_scroll->setWidget(room_editor_);
    auto* room_panel = new QWidget;
    auto* room_panel_layout = new QVBoxLayout(room_panel);
    room_panel_layout->setContentsMargins(0, 0, 0, 0);
    room_panel_layout->addWidget(room_scroll, 1);
    auto* room_buttons = new QHBoxLayout;
    auto* room_apply = new QPushButton("Apply changes");
    auto* room_sync_exits = new QPushButton("Allinea uscite in entrata");
    auto* room_goto_exit = new QPushButton("Go to exit target");
    room_buttons->addWidget(room_apply);
    room_buttons->addWidget(room_sync_exits);
    room_buttons->addWidget(room_goto_exit);
    room_buttons->addStretch();
    room_panel_layout->addLayout(room_buttons);
    const EntityPageWidgets room_page = makeEntityPage(room_panel, "New room");
    room_search_ = room_page.search;
    room_list_ = room_page.list;
    tabs_->addTab(room_page.page, "Rooms");

    auto* mob_scroll = new QScrollArea;
    mob_scroll->setWidgetResizable(true);
    mob_editor_ = new MobEditorWidget;
    mob_scroll->setWidget(mob_editor_);
    auto* mob_panel = new QWidget;
    auto* mob_panel_layout = new QVBoxLayout(mob_panel);
    mob_panel_layout->setContentsMargins(0, 0, 0, 0);
    mob_panel_layout->addWidget(mob_scroll, 1);
    auto* mob_apply = new QPushButton("Applica modifiche");
    mob_panel_layout->addWidget(mob_apply);
    const EntityPageWidgets mob_page = makeEntityPage(mob_panel, "Nuovo mob");
    mob_search_ = mob_page.search;
    mob_list_ = mob_page.list;
    tabs_->addTab(mob_page.page, "Mob");

    auto* obj_scroll = new QScrollArea;
    obj_scroll->setWidgetResizable(true);
    obj_editor_ = new ObjEditorWidget;
    obj_scroll->setWidget(obj_editor_);
    auto* obj_panel = new QWidget;
    auto* obj_panel_layout = new QVBoxLayout(obj_panel);
    obj_panel_layout->setContentsMargins(0, 0, 0, 0);
    obj_panel_layout->addWidget(obj_scroll, 1);
    auto* obj_apply = new QPushButton("Applica modifiche");
    obj_panel_layout->addWidget(obj_apply);
    const EntityPageWidgets obj_page = makeEntityPage(obj_panel, "Nuovo oggetto");
    obj_search_ = obj_page.search;
    obj_list_ = obj_page.list;
    tabs_->addTab(obj_page.page, "Oggetti");

    auto* zone_scroll = new QScrollArea;
    zone_scroll->setWidgetResizable(true);
    zone_editor_ = new ZoneEditorWidget;
    zone_scroll->setWidget(zone_editor_);
    auto* zone_panel = new QWidget;
    auto* zone_panel_layout = new QVBoxLayout(zone_panel);
    zone_panel_layout->setContentsMargins(0, 0, 0, 0);
    zone_panel_layout->addWidget(zone_scroll, 1);
    auto* zone_buttons = new QHBoxLayout;
    auto* zone_apply = new QPushButton("Apply changes");
    zone_buttons->addWidget(zone_apply);
    zone_buttons->addStretch();
    zone_panel_layout->addLayout(zone_buttons);

    auto* zone_page = new QWidget;
    auto* zone_layout = new QVBoxLayout(zone_page);
    auto* zone_splitter = new QSplitter;

    auto* zone_left = new QWidget;
    auto* zone_left_layout = new QVBoxLayout(zone_left);
    zone_left_layout->addWidget(new QLabel("Zone"));
    zone_list_ = new QListWidget;
    zone_list_->setMinimumWidth(160);
    zone_left_layout->addWidget(zone_list_);
    zone_splitter->addWidget(zone_left);
    zone_splitter->addWidget(zone_panel);
    zone_splitter->setStretchFactor(0, 1);
    zone_splitter->setStretchFactor(1, 3);
    zone_layout->addWidget(zone_splitter);
    zone_tab_ = zone_page;
    tabs_->addTab(zone_page, "Zone");

    map_tab_ = new QWidget;
    auto* map_layout = new QVBoxLayout(map_tab_);
    auto* map_tabs = new QTabWidget;
    map_tabs_ = map_tabs;

    auto* map_zone_page = new QWidget;
    auto* map_zone_layout = new QVBoxLayout(map_zone_page);
    map_zone_layout->addWidget(
        new QLabel("Stanze della zona. Piano Z separato; clic su su/giù cambia piano ed evidenzia la destinazione."));
    auto* map_top = new QHBoxLayout;
    map_zone_ = new QComboBox;
    map_floor_ = new QComboBox;
    map_broken_only_ = new QCheckBox("Solo link rotti");
    auto* map_refresh = new QPushButton("Aggiorna");
    auto* map_export_dot = new QPushButton("Esporta DOT");
    auto* map_export_png = new QPushButton("Esporta PNG");
    map_top->addWidget(new QLabel("Zona:"));
    map_top->addWidget(map_zone_, 1);
    map_top->addWidget(new QLabel("Piano Z:"));
    map_top->addWidget(map_floor_);
    map_top->addWidget(map_broken_only_);
    map_top->addWidget(map_refresh);
    map_top->addWidget(map_export_dot);
    map_top->addWidget(map_export_png);
    map_zone_layout->addLayout(map_top);

    map_view_ = new ZoneMapWidget;
    map_view_->setMinimumHeight(320);
    map_zone_layout->addWidget(map_view_, 1);

    map_stats_ = new QLabel;
    map_stats_->setWordWrap(true);
    map_zone_layout->addWidget(map_stats_);
    map_tabs->addTab(map_zone_page, "Zona");

    auto* map_world_page = new QWidget;
    auto* map_world_layout = new QVBoxLayout(map_world_page);
    map_world_layout->addWidget(
        new QLabel("Collegamenti tra zone (non stanze). U=vnum usati, L=vnum liberi nel range. Clic seleziona, doppio clic apre tab Zone."));
    auto* world_top = new QHBoxLayout;
    world_map_broken_only_ = new QCheckBox("Solo link rotti");
    auto* world_refresh = new QPushButton("Aggiorna");
    auto* world_export_dot = new QPushButton("Esporta DOT");
    auto* world_export_png = new QPushButton("Esporta PNG");
    auto* world_open_zone_map = new QPushButton("Mappa stanze zona");
    world_top->addWidget(world_map_broken_only_);
    world_top->addWidget(world_open_zone_map);
    world_top->addStretch(1);
    world_top->addWidget(world_refresh);
    world_top->addWidget(world_export_dot);
    world_top->addWidget(world_export_png);
    map_world_layout->addLayout(world_top);

    world_map_view_ = new WorldZoneMapWidget;
    world_map_view_->setMinimumHeight(320);
    map_world_layout->addWidget(world_map_view_, 2);

    world_map_details_ = new QPlainTextEdit;
    world_map_details_->setReadOnly(true);
    world_map_details_->setPlaceholderText("Clic su una zona per vedere vnum usati e liberi.");
    world_map_details_->setMaximumHeight(140);
    map_world_layout->addWidget(world_map_details_, 1);

    world_map_stats_ = new QLabel;
    world_map_stats_->setWordWrap(true);
    map_world_layout->addWidget(world_map_stats_);
    map_tabs->addTab(map_world_page, "Mondo (zone)");

    map_layout->addWidget(map_tabs, 1);
    tabs_->addTab(map_tab_, "Mappa");

    validation_tab_ = new QWidget;
    auto* validation_layout = new QVBoxLayout(validation_tab_);

    auto* validation_top = new QHBoxLayout;
    auto* validate_button = new QPushButton("Valida ora");
    auto* validation_summary = new QLabel("Esegui Valida per vedere errori e avvisi. Doppio clic per andare all'entità.");
    validation_summary->setWordWrap(true);
    validation_top->addWidget(validate_button);
    validation_layout->addWidget(validation_summary);
    validation_layout->addLayout(validation_top);

    validation_list_ = new QListWidget;
    validation_layout->addWidget(validation_list_, 1);
    tabs_->addTab(validation_tab_, "Validazione");

    root_layout->addWidget(tabs_);
    setCentralWidget(central);
    statusBar()->showMessage("Pronto");

    connect(room_list_, &QListWidget::currentRowChanged, this, [this](int) { onRoomSelected(); });
    connect(mob_list_, &QListWidget::currentRowChanged, this, [this](int) { onMobSelected(); });
    connect(obj_list_, &QListWidget::currentRowChanged, this, [this](int) { onObjSelected(); });
    connect(room_apply, &QPushButton::clicked, this, &MainWindow::applyRoomChanges);
    connect(room_sync_exits, &QPushButton::clicked, this, &MainWindow::syncInboundExitLabels);
    connect(room_goto_exit, &QPushButton::clicked, this, &MainWindow::goToExitTarget);
    connect(mob_apply, &QPushButton::clicked, this, &MainWindow::applyMobChanges);
    connect(obj_apply, &QPushButton::clicked, this, &MainWindow::applyObjChanges);
    connect(room_page.create_button, &QPushButton::clicked, this, &MainWindow::createRoom);
    connect(mob_page.create_button, &QPushButton::clicked, this, &MainWindow::createMob);
    connect(obj_page.create_button, &QPushButton::clicked, this, &MainWindow::createObject);
    connect(room_search_, &QLineEdit::textChanged, this, &MainWindow::onRoomSearchChanged);
    connect(mob_search_, &QLineEdit::textChanged, this, &MainWindow::onMobSearchChanged);
    connect(obj_search_, &QLineEdit::textChanged, this, &MainWindow::onObjSearchChanged);
    connect(zone_list_, &QListWidget::currentRowChanged, this, [this](int) { onZoneSelected(); });
    connect(zone_apply, &QPushButton::clicked, this, &MainWindow::applyZoneChanges);
    connect(zone_editor_, &ZoneEditorWidget::zoneModified, this, [this]() { markDirty(); });
    connect(zone_editor_, &ZoneEditorWidget::gotoRoomRequested, this, [this](long vnum) {
        if (!world_.find_room(vnum)) {
            QMessageBox::information(this, "Zone", QString("Room #%1 does not exist in this library.").arg(vnum));
            return;
        }
        selectRoomByVnum(vnum);
    });
    connect(zone_editor_, &ZoneEditorWidget::gotoMobRequested, this, [this](long vnum) {
        selectMobByVnum(vnum);
    });
    connect(zone_editor_, &ZoneEditorWidget::gotoObjectRequested, this, [this](long vnum) {
        selectObjectByVnum(vnum);
    });
    connect(validate_button, &QPushButton::clicked, this, &MainWindow::validateLib);
    connect(validation_list_, &QListWidget::itemDoubleClicked, this, &MainWindow::onValidationIssueActivated);
    connect(map_refresh, &QPushButton::clicked, this, &MainWindow::refreshZoneMap);
    connect(map_zone_, &QComboBox::currentIndexChanged, this, [this](int) { refreshZoneMap(); });
    connect(map_broken_only_, &QCheckBox::toggled, this, [this](bool enabled) {
        if (map_view_) {
            map_view_->setShowBrokenOnly(enabled);
            updateMapStats();
        }
    });
    connect(map_floor_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0 || !map_view_) {
            return;
        }
        map_view_->setActiveZLevel(map_floor_->currentData().toInt());
        updateMapStats();
    });
    connect(map_view_, &ZoneMapWidget::roomActivated, this, [this](long vnum) {
        tabs_->setCurrentIndex(0);
        selectRoomByVnum(vnum);
        setStatus(QString("Mappa: selezionata stanza #%1.").arg(vnum));
    });
    connect(map_view_, &ZoneMapWidget::floorLinkActivated, this, [this](long target_vnum, int target_z) {
        const int index = map_floor_->findData(target_z);
        if (index >= 0) {
            map_floor_->setCurrentIndex(index);
        } else {
            map_view_->setActiveZLevel(target_z);
        }
        map_view_->setHighlightedVnum(target_vnum);
        setStatus(QString("Mappa: piano Z=%1, evidenciata stanza #%2.").arg(target_z).arg(target_vnum));
    });
    connect(map_export_dot, &QPushButton::clicked, this, [this]() {
        if (map_zone_->count() == 0) {
            return;
        }
        const int zone_num = map_zone_->currentData().toInt();
        const nebbie::ZoneGraph graph = nebbie::build_zone_graph(world_, zone_num);
        QApplication::clipboard()->setText(QString::fromStdString(nebbie::zone_graph_to_dot(graph)));
        setStatus("DOT zona copiato negli appunti.");
    });
    connect(map_export_png, &QPushButton::clicked, this, [this]() {
        const int zone_num = map_zone_->count() > 0 ? map_zone_->currentData().toInt() : 0;
        exportMapPng(map_view_, QString("zona-%1.png").arg(zone_num));
    });
    connect(world_refresh, &QPushButton::clicked, this, &MainWindow::refreshWorldZoneMap);
    connect(world_map_broken_only_, &QCheckBox::toggled, this, [this](bool enabled) {
        if (world_map_view_) {
            world_map_view_->setShowBrokenOnly(enabled);
        }
    });
    connect(world_map_view_, &WorldZoneMapWidget::zoneSelected, this, [this](int zone_num) {
        selected_world_zone_ = zone_num;
        updateWorldZoneDetails(zone_num);
    });
    connect(world_map_view_, &WorldZoneMapWidget::zoneActivated, this, [this](int zone_num) {
        selected_world_zone_ = zone_num;
        updateWorldZoneDetails(zone_num);
        openZoneRoomMap(zone_num);
    });
    connect(world_open_zone_map, &QPushButton::clicked, this, [this]() {
        if (selected_world_zone_ < 0) {
            QMessageBox::information(this, "Mappa", "Seleziona prima una zona nella mappa mondo.");
            return;
        }
        openZoneRoomMap(selected_world_zone_);
    });
    connect(world_export_dot, &QPushButton::clicked, this, [this]() {
        const nebbie::WorldZoneGraph graph = nebbie::build_world_zone_graph(world_);
        QApplication::clipboard()->setText(QString::fromStdString(nebbie::world_zone_graph_to_dot(graph)));
        setStatus("DOT mondo (zone) copiato negli appunti.");
    });
    connect(world_export_png, &QPushButton::clicked, this, [this]() {
        exportMapPng(world_map_view_, QStringLiteral("mondo-zone.png"));
    });
    zone_editor_->setWorld(&world_);

    autosave_timer_ = new QTimer(this);
    autosave_timer_->setInterval(session_config_.autosave_interval_sec * 1000);
    connect(autosave_timer_, &QTimer::timeout, this, &MainWindow::onAutosaveTick);
}

void MainWindow::setupMenus() {
    auto* file_menu = menuBar()->addMenu("&File");

    auto* open_action = file_menu->addAction("&Apri libreria...");
    open_action->setShortcut(QKeySequence::Open);
    connect(open_action, &QAction::triggered, this, &MainWindow::openLib);

    auto* save_action = file_menu->addAction("&Salva");
    save_action->setShortcut(QKeySequence::Save);
    connect(save_action, &QAction::triggered, this, &MainWindow::saveLib);

    auto* save_force_action = file_menu->addAction("Salva (forza)");
    connect(save_force_action, &QAction::triggered, this, &MainWindow::saveLibForce);

    auto* history_menu = file_menu->addMenu("Cronologia");
    auto* restore_workspace_action = history_menu->addAction("Ripristina ultimo autosalvataggio");
    connect(restore_workspace_action, &QAction::triggered, this, &MainWindow::restoreFromWorkspace);
    auto* restore_version_action = history_menu->addAction("Ripristina versione...");
    connect(restore_version_action, &QAction::triggered, this, &MainWindow::restoreVersion);

    file_menu->addSeparator();
    file_menu->addAction("E&sci", this, &QWidget::close);

    auto* tools_menu = menuBar()->addMenu("&Strumenti");
    auto* validate_action = tools_menu->addAction("&Valida");
    validate_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(validate_action, &QAction::triggered, this, &MainWindow::validateLib);
    tools_menu->addSeparator();
    auto* export_overlays_action = tools_menu->addAction("Esporta overlay...");
    connect(export_overlays_action, &QAction::triggered, this, &MainWindow::exportOverlays);

    auto* coordinator_menu = menuBar()->addMenu("&Coordinator");
    auto* coordinator_config_action = coordinator_menu->addAction("Configuration...");
    connect(coordinator_config_action, &QAction::triggered, this, &MainWindow::configureCoordinator);
    auto* refresh_index_action = coordinator_menu->addAction("Refresh world index");
    connect(refresh_index_action, &QAction::triggered, this, &MainWindow::refreshWorldIndex);
    auto* load_index_action = coordinator_menu->addAction("Load world index from file...");
    connect(load_index_action, &QAction::triggered, this, &MainWindow::loadWorldIndexFromFile);
    auto* export_index_action = coordinator_menu->addAction("Export local world index");
    connect(export_index_action, &QAction::triggered, this, &MainWindow::exportLocalWorldIndex);
    coordinator_menu->addSeparator();
    auto* reserve_action = coordinator_menu->addAction("Reserve vnums...");
    connect(reserve_action, &QAction::triggered, this, &MainWindow::reserveVnums);
}

void MainWindow::rememberLibPath(const std::filesystem::path& path) {
    nebbie::qt::write_lib_path(nebbie::qt::qstring_from_path(path));
}

void MainWindow::openLibPath(const QString& path) {
    if (path.isEmpty()) {
        return;
    }
    try {
        const std::filesystem::path requested = nebbie::qt::path_from_qstring(path);
        const std::filesystem::path resolved = nebbie::resolve_lib_directory(requested);
        loadLib(resolved);
        rememberLibPath(resolved);
        if (resolved != requested) {
            setStatus(QString("Libreria risolta in: %1").arg(nebbie::qt::qstring_from_path(resolved)));
        }
    } catch (const std::exception& ex) {
        const std::filesystem::path resolved = nebbie::resolve_lib_directory(nebbie::qt::path_from_qstring(path));
        const QString detail = QString::fromUtf8(ex.what())
                               + QString("\n\nPercorso richiesto: %1").arg(path)
                               + QString("\nPercorso risolto: %1").arg(nebbie::qt::qstring_from_path(resolved))
                               + "\n\nSeleziona la cartella mudroot/lib e ricompila la GUI:\n"
                                 "  ./scripts/build.sh";
        QMessageBox::critical(this, "Errore caricamento libreria", detail);
    }
}

void MainWindow::openLib() {
    if (!confirmSaveIfDirty()) {
        return;
    }

    const QString dir = QFileDialog::getExistingDirectory(
        this, "Apri libreria Nebbie (mudroot o mudroot/lib)", QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) {
        return;
    }
    openLibPath(dir);
}

void MainWindow::openStartupLib() {
    const QString saved = nebbie::qt::read_lib_path();
    if (nebbie::qt::lib_path_exists(saved)) {
        openLibPath(saved);
        return;
    }

    if (!saved.isEmpty()) {
        promptForLibPath(QString("Il percorso salvato non è più valido:\n%1").arg(saved));
        return;
    }

    promptForLibPath(QString(
        "Benvenuto in Nebbie Editor.\n\n"
        "Seleziona la cartella della libreria di gioco (mudroot o mudroot/lib).\n"
        "Il percorso verrà salvato in:\n%1")
                         .arg(nebbie::qt::default_config_path()));
}

bool MainWindow::promptForLibPath(const QString& reason) {
    if (!reason.isEmpty()) {
        QMessageBox::information(this, "Libreria Nebbie", reason);
    }

    const QString initial = nebbie::qt::read_lib_path();
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

void MainWindow::loadLib(const std::filesystem::path& path) {
    world_.clear();
    context_ = {};
    nebbie::load_lib(world_, path, context_);
    lib_path_ = path;
    room_filter_.clear();
    mob_filter_.clear();
    object_filter_.clear();
    room_search_->clear();
    mob_search_->clear();
    obj_search_->clear();
    markClean();
    refreshRoomList();
    refreshMobList();
    refreshObjectList();
    refreshZoneList();
    refreshZoneMap();
    refreshWorldZoneMap();
    zone_editor_->setWorld(&world_);

    const QString label = QString("Libreria: %1 — %2 zone, %3 stanze, %4 mob, %5 oggetti")
                              .arg(nebbie::qt::qstring_from_path(path))
                              .arg(world_.zones.size())
                              .arg(world_.rooms.size())
                              .arg(world_.mobiles.size())
                              .arg(world_.objects.size());
    lib_label_->setText(label);
    last_version_time_ = std::chrono::system_clock::now();
    autosave_timer_->start();
    setStatus("Libreria caricata.");
}

void MainWindow::refreshRoomList() {
    const long selected = currentRoomVnum();
    room_list_->clear();
    const std::string query = room_filter_.toStdString();
    for (const auto& [vnum, room] : world_.rooms) {
        if (!nebbie::entity_matches(vnum, room.name, query)) {
            continue;
        }
        addListItem(room_list_, vnum,
                    QString("#%1 %2").arg(vnum).arg(QString::fromStdString(room.name)));
    }
    if (selected > 0) {
        selectRoomByVnum(selected);
    }
}

void MainWindow::refreshMobList() {
    mob_list_->clear();
    const std::string query = mob_filter_.toStdString();
    for (const auto& [vnum, mob] : world_.mobiles) {
        if (!nebbie::entity_matches(vnum, mob.short_descr, query)) {
            continue;
        }
        addListItem(mob_list_, vnum,
                    QString("#%1 %2").arg(vnum).arg(QString::fromStdString(mob.short_descr)));
    }
}

void MainWindow::refreshObjectList() {
    obj_list_->clear();
    const std::string query = object_filter_.toStdString();
    for (const auto& [vnum, obj] : world_.objects) {
        if (!nebbie::entity_matches(vnum, obj.short_descr, query)) {
            continue;
        }
        addListItem(obj_list_, vnum,
                    QString("#%1 %2").arg(vnum).arg(QString::fromStdString(obj.short_descr)));
    }
}

void MainWindow::refreshZoneList() {
    zone_list_->clear();
    for (const auto& zone : world_.zones) {
        const QString label = QString("#%1 %2 [%3-%4]")
                                  .arg(zone.num)
                                  .arg(QString::fromStdString(zone.name))
                                  .arg(zone.bottom)
                                  .arg(zone.top);
        addListItem(zone_list_, zone.num, label);
    }
    if (zone_list_->count() > 0 && zone_list_->currentRow() < 0) {
        zone_list_->setCurrentRow(0);
    }

    const int previous_zone = map_zone_->currentData().toInt();
    map_zone_->clear();
    for (const auto& zone : world_.zones) {
        map_zone_->addItem(QString("#%1 %2").arg(zone.num).arg(QString::fromStdString(zone.name)), zone.num);
    }
    if (map_zone_->count() > 0) {
        int index = map_zone_->findData(previous_zone);
        if (index < 0) {
            index = 0;
        }
        map_zone_->setCurrentIndex(index);
    }
}

void MainWindow::openZoneRoomMap(int zone_num) {
    if (map_tab_) {
        tabs_->setCurrentWidget(map_tab_);
    }
    if (map_tabs_) {
        map_tabs_->setCurrentIndex(0);
    }

    const int zone_index = map_zone_->findData(zone_num);
    if (zone_index >= 0) {
        map_zone_->setCurrentIndex(zone_index);
    }

    refreshZoneMap();
    setStatus(QString("Mappa stanze: zona #%1.").arg(zone_num));
}

void MainWindow::exportMapPng(ZoneMapWidget* view, const QString& suggested_name) {
    if (!view) {
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this, "Esporta mappa PNG", suggested_name, QStringLiteral("PNG (*.png)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".png");
    }

    if (!view->exportSceneToPng(path)) {
        QMessageBox::warning(this, "Esporta PNG", "Impossibile esportare la mappa (vuota?).");
        return;
    }
    setStatus(QString("Mappa esportata in %1").arg(path));
}

void MainWindow::exportMapPng(WorldZoneMapWidget* view, const QString& suggested_name) {
    if (!view) {
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this, "Esporta mappa PNG", suggested_name, QStringLiteral("PNG (*.png)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".png");
    }

    if (!view->exportSceneToPng(path)) {
        QMessageBox::warning(this, "Esporta PNG", "Impossibile esportare la mappa (vuota?).");
        return;
    }
    setStatus(QString("Mappa esportata in %1").arg(path));
}

void MainWindow::refreshZoneMap() {
    if (!map_view_ || map_zone_->count() == 0) {
        return;
    }

    const int zone_num = map_zone_->currentData().toInt();
    const nebbie::ZoneGraph graph = nebbie::build_zone_graph(world_, zone_num);
    if (graph.nodes.empty()) {
        map_view_->clearGraph();
        map_floor_->clear();
        if (map_stats_) {
            map_stats_->setText("Nessuna stanza in questa zona.");
        }
        return;
    }

    map_view_->setGraph(graph);

    const int previous_floor = map_floor_->currentData().toInt();
    map_floor_->clear();
    for (int level : map_view_->availableZLevels()) {
        map_floor_->addItem(QString("Z = %1").arg(level), level);
    }
    int floor_index = map_floor_->findData(previous_floor);
    if (floor_index < 0) {
        floor_index = 0;
    }
    map_floor_->setCurrentIndex(floor_index);
    map_view_->setActiveZLevel(map_floor_->currentData().toInt());
    if (map_broken_only_) {
        map_view_->setShowBrokenOnly(map_broken_only_->isChecked());
    }
    updateMapStats();
}

void MainWindow::refreshWorldZoneMap() {
    if (!world_map_view_) {
        return;
    }

    const nebbie::WorldZoneGraph graph = nebbie::build_world_zone_graph(world_);
    if (graph.zones.empty()) {
        world_map_view_->clearGraph();
        if (world_map_details_) {
            world_map_details_->clear();
        }
        if (world_map_stats_) {
            world_map_stats_->setText("Nessuna zona caricata.");
        }
        return;
    }

    world_map_view_->setGraph(graph);
    if (world_map_broken_only_) {
        world_map_view_->setShowBrokenOnly(world_map_broken_only_->isChecked());
    }

    int total_used = 0;
    int total_free = 0;
    int broken_edges = 0;
    for (const auto& zone : graph.zones) {
        total_used += zone.used_count;
        total_free += zone.free_count;
    }
    for (const auto& edge : graph.edges) {
        if (edge.broken_count > 0) {
            ++broken_edges;
        }
    }

    if (world_map_stats_) {
        world_map_stats_->setText(
            QString("Zone: %1 | collegamenti inter-zona: %2 | link rotti: %3 | vnum stanza usati: %4 | vnum liberi: %5")
                .arg(graph.zones.size())
                .arg(graph.edges.size())
                .arg(broken_edges)
                .arg(total_used)
                .arg(total_free));
    }
}

void MainWindow::updateWorldZoneDetails(int zone_num) {
    if (!world_map_details_) {
        return;
    }

    const nebbie::Zone* zone = nebbie::find_zone(world_, zone_num);
    if (!zone) {
        world_map_details_->setPlainText(QString("Zona #%1 non trovata.").arg(zone_num));
        return;
    }

    const nebbie::WorldZoneNode node = nebbie::build_world_zone_node(world_, *zone);
    QString used_text;
    if (node.used_vnums.empty()) {
        used_text = "(nessuna stanza nel range)";
    } else if (node.used_vnums.size() <= 40) {
        QStringList parts;
        for (long vnum : node.used_vnums) {
            parts << QString::number(vnum);
        }
        used_text = parts.join(", ");
    } else {
        used_text = QString("%1 … %2 (%3 vnum, elenco troncato)")
                        .arg(node.used_vnums.front())
                        .arg(node.used_vnums.back())
                        .arg(node.used_count);
    }

    const QString details = QString("Zona #%1 %2\nRange vnum: %3-%4\n\nStanze usate (%5):\n%6\n\nVnum liberi (%7):\n%8")
                                .arg(node.zone_num)
                                .arg(QString::fromStdString(node.name))
                                .arg(node.bottom)
                                .arg(node.top)
                                .arg(node.used_count)
                                .arg(used_text)
                                .arg(node.free_count)
                                .arg(QString::fromStdString(nebbie::format_vnum_ranges(node.free_ranges, 20)));
    world_map_details_->setPlainText(details);
    if (world_map_view_) {
        world_map_view_->setHighlightedZone(zone_num);
    }
}

void MainWindow::updateMapStats() {
    if (!map_stats_ || map_zone_->count() == 0) {
        return;
    }

    const int zone_num = map_zone_->currentData().toInt();
    const nebbie::ZoneGraph graph = nebbie::build_zone_graph(world_, zone_num);
    if (graph.nodes.empty()) {
        return;
    }

    const nebbie::ZoneZLayout z_layout = nebbie::compute_zone_z_levels(graph);
    const int active_z = map_view_ ? map_view_->activeZLevel() : 0;

    std::size_t on_floor = 0;
    std::size_t broken = 0;
    std::size_t vertical = 0;
    for (const auto& node : graph.nodes) {
        const auto it = z_layout.levels.find(node.vnum);
        if (it != z_layout.levels.end() && it->second == active_z) {
            ++on_floor;
        }
    }
    for (const auto& edge : graph.edges) {
        if (edge.broken) {
            ++broken;
        }
        if (edge.direction >= 4) {
            ++vertical;
        }
    }

    map_stats_->setText(QString("Zona %1 %2 [%3-%4] — piano Z=%5: %6 stanze | totali: %7 | archi: %8 | su/giù: %9 | rotti: %10")
                            .arg(graph.zone_num)
                            .arg(QString::fromStdString(graph.zone_name))
                            .arg(graph.bottom)
                            .arg(graph.top)
                            .arg(active_z)
                            .arg(on_floor)
                            .arg(graph.nodes.size())
                            .arg(graph.edges.size())
                            .arg(vertical)
                            .arg(broken));
}

void MainWindow::onZoneSelected() {
    const int zone_num = currentZoneNum();
    if (zone_num <= 0) {
        return;
    }
    const nebbie::Zone* zone = nebbie::find_zone(world_, zone_num);
    if (!zone) {
        return;
    }
    zone_editor_->loadFromZone(*zone);
}

void MainWindow::applyZoneChanges() {
    const int zone_num = currentZoneNum();
    if (zone_num <= 0) {
        QMessageBox::information(this, "Zone", "Select a zone.");
        return;
    }
    nebbie::Zone* zone = nebbie::find_zone(world_, zone_num);
    if (!zone) {
        QMessageBox::warning(this, "Zone", "Zone not found.");
        return;
    }

    nebbie::Zone updated = *zone;
    zone_editor_->saveZoneInfoTo(updated);
    nebbie::assign_zone_fields(*zone, updated);
    nebbie::recompute_zone_bottoms(world_);

    for (int i = 0; i < zone_list_->count(); ++i) {
        if (zone_list_->item(i)->data(Qt::UserRole).toLongLong() == zone_num) {
            zone_list_->item(i)->setText(
                QString("#%1 %2 [%3-%4]")
                    .arg(zone->num)
                    .arg(QString::fromStdString(zone->name))
                    .arg(zone->bottom)
                    .arg(zone->top));
            break;
        }
    }
    zone_editor_->loadFromZone(*zone);
    markDirty();
    setStatus(QString("Zone %1 updated in memory.").arg(zone_num));
}

int MainWindow::currentZoneNum() const {
    const auto* item = zone_list_->currentItem();
    if (!item) {
        return 0;
    }
    return static_cast<int>(item->data(Qt::UserRole).toLongLong());
}

void MainWindow::selectZoneByNum(const int zone_num) {
    for (int i = 0; i < zone_list_->count(); ++i) {
        if (zone_list_->item(i)->data(Qt::UserRole).toLongLong() == zone_num) {
            zone_list_->setCurrentRow(i);
            tabs_->setCurrentWidget(zone_tab_);
            return;
        }
    }
}

int MainWindow::preferredZoneNumForNewRoom() const {
    if (const int zone_num = currentZoneNum(); zone_num > 0) {
        return zone_num;
    }
    if (selected_world_zone_ > 0) {
        return selected_world_zone_;
    }
    const long room_vnum = currentRoomVnum();
    if (room_vnum > 0) {
        const nebbie::Room* room = world_.find_room(room_vnum);
        if (room && room->zone_index >= 0
            && room->zone_index < static_cast<int>(world_.zones.size())) {
            return world_.zones[static_cast<std::size_t>(room->zone_index)].num;
        }
    }
    return 0;
}

nebbie::WorldIndex MainWindow::worldIndexForValidation() const {
    if (world_index_) {
        return *world_index_;
    }
    nebbie::World copy = world_;
    nebbie::recompute_zone_bottoms(copy);
    return nebbie::build_world_index(copy, "local-validation");
}

long MainWindow::suggestRoomVnum() const {
    if (world_index_) {
        const int zone_num = preferredZoneNumForNewRoom();
        if (zone_num > 0) {
            const auto suggested = nebbie::suggest_room_vnum_in_zone(*world_index_, zone_num);
            if (suggested) {
                return *suggested;
            }
        }
        for (const auto& zone : world_index_->zones) {
            const auto suggested = nebbie::suggest_room_vnum_in_zone(*world_index_, zone.zone_num);
            if (suggested) {
                return *suggested;
            }
        }
    }
    return nebbie::suggest_next_room_vnum(world_);
}

long MainWindow::suggestMobVnum() const {
    if (world_index_) {
        const auto suggested = nebbie::suggest_mob_vnum(*world_index_);
        if (suggested) {
            return *suggested;
        }
    }
    return nebbie::suggest_next_mob_vnum(world_);
}

long MainWindow::suggestObjectVnum() const {
    if (world_index_) {
        const auto suggested = nebbie::suggest_object_vnum(*world_index_);
        if (suggested) {
            return *suggested;
        }
    }
    return nebbie::suggest_next_object_vnum(world_);
}

bool MainWindow::warnIfRemoteVnumConflict(const QString& kind, const long vnum) const {
    if (!world_index_) {
        return true;
    }

    bool taken = false;
    if (kind == QStringLiteral("room")) {
        taken = nebbie::room_vnum_taken(*world_index_, vnum);
    } else if (kind == QStringLiteral("mob")) {
        taken = nebbie::mob_vnum_taken(*world_index_, vnum);
    } else if (kind == QStringLiteral("object")) {
        taken = nebbie::object_vnum_taken(*world_index_, vnum);
    }
    if (!taken) {
        return true;
    }

    const auto answer = QMessageBox::warning(
        const_cast<MainWindow*>(this),
        "World index conflict",
        QString("Vnum %1 is used or reserved in the remote world index.\n"
                "Create it in the local library anyway?")
            .arg(vnum),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    return answer == QMessageBox::Yes;
}

void MainWindow::configureCoordinator() {
    QDialog dialog(this);
    dialog.setWindowTitle("Coordinator configuration");
    auto* form = new QFormLayout(&dialog);

    auto* index_url = new QLineEdit(app_config_.index_url);
    auto* coordinator_url = new QLineEdit(app_config_.coordinator_url);
    auto* coordinator_token = new QLineEdit(app_config_.coordinator_token);
    coordinator_token->setEchoMode(QLineEdit::Password);
    auto* builder_name = new QLineEdit(app_config_.builder_name);

    form->addRow("index_url:", index_url);
    form->addRow("coordinator_url:", coordinator_url);
    form->addRow("coordinator_token:", coordinator_token);
    form->addRow("builder_name:", builder_name);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    app_config_.index_url = index_url->text().trimmed();
    app_config_.coordinator_url = coordinator_url->text().trimmed();
    app_config_.coordinator_token = coordinator_token->text().trimmed();
    app_config_.builder_name = builder_name->text().trimmed();
    nebbie::qt::write_config(app_config_);
    setStatus("Coordinator configuration saved.");
}

void MainWindow::refreshWorldIndex() {
    if (app_config_.index_url.isEmpty()) {
        QMessageBox::information(this, "World index", "Configure index_url first.");
        return;
    }

    const auto result = nebbie::qt::fetch_coordinator_sync(*network_,
                                                           app_config_.index_url,
                                                           app_config_.coordinator_url,
                                                           app_config_.coordinator_token);
    if (!result.ok || !result.index) {
        QMessageBox::warning(this, "World index",
                             result.error.isEmpty() ? "Unable to download world index." : result.error);
        return;
    }

    world_index_ = *result.index;
    setStatus(QString("World index updated: %1 zones, %2 active reservations.")
                  .arg(world_index_->zones.size())
                  .arg(world_index_->reservations.size()));
}

void MainWindow::loadWorldIndexFromFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Load world index", {},
                                                      "JSON files (*.json);;All files (*)");
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "World index", "Unable to open file.");
        return;
    }

    const auto parsed = nebbie::world_index_from_json(file.readAll().toStdString());
    if (!parsed) {
        QMessageBox::warning(this, "World index", "Invalid world-index JSON.");
        return;
    }

    world_index_ = *parsed;
    if (!app_config_.coordinator_url.isEmpty() && !app_config_.coordinator_token.isEmpty()) {
        const auto result = nebbie::qt::fetch_coordinator_sync(*network_,
                                                               app_config_.index_url,
                                                               app_config_.coordinator_url,
                                                               app_config_.coordinator_token,
                                                               false);
        if (result.ok) {
            nebbie::merge_reservations(*world_index_, result.reservations);
        }
    }

    setStatus(QString("World index loaded from file: %1 zones.").arg(world_index_->zones.size()));
}

void MainWindow::exportOverlays() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Export overlay", "Apri una libreria prima di esportare gli overlay.");
        return;
    }

    try {
        const auto report = nebbie::export_myst_to_overlays(world_, lib_path_, nebbie::OverlayExportKind::all);
        QString message = QString("Overlay esportati in %1\n\n"
                                  "Stanze: %2\nOggetti: %3\nMob: %4\nReset zone: %5")
                              .arg(nebbie::qt::qstring_from_path(lib_path_))
                              .arg(report.rooms)
                              .arg(report.objects)
                              .arg(report.mobiles)
                              .arg(report.zone_resets);
        if (!report.warnings.empty()) {
            message += "\n\nAvvisi:";
            for (const auto& warning : report.warnings) {
                message += "\n• " + QString::fromStdString(warning);
            }
        }
        QMessageBox::information(this, "Export overlay", message);
        setStatus(QString("Overlay esportati: %1 stanze, %2 oggetti, %3 mob, %4 reset zone.")
                      .arg(report.rooms)
                      .arg(report.objects)
                      .arg(report.mobiles)
                      .arg(report.zone_resets));
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Export overlay", QString::fromUtf8(ex.what()));
    }
}

void MainWindow::exportLocalWorldIndex() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "World index", "Open a library first.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this, "Export world index", "world-index.json",
                                                      "JSON files (*.json)");
    if (path.isEmpty()) {
        return;
    }

    nebbie::recompute_zone_bottoms(world_);
    const nebbie::WorldIndex index = nebbie::build_world_index(world_, "local-export");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, "World index", "Unable to write file.");
        return;
    }

    QTextStream out(&file);
    out << QString::fromStdString(nebbie::world_index_to_json(index));
    setStatus(QString("Exported world index with %1 zones (bottom/top from myst.zon).")
                  .arg(index.zones.size()));
}

void MainWindow::reserveVnums() {
    if (app_config_.coordinator_url.isEmpty() || app_config_.coordinator_token.isEmpty()) {
        QMessageBox::information(this, "Reservations",
                                 "Configure coordinator_url and coordinator_token.");
        return;
    }
    if (app_config_.builder_name.isEmpty()) {
        QMessageBox::information(this, "Reservations", "Configure builder_name.");
        return;
    }

    const nebbie::WorldIndex validation_index = worldIndexForValidation();
    if (validation_index.zones.empty()) {
        QMessageBox::information(this, "Reservations",
                                 "No zones available. Open a library or refresh the world index.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Reserve vnums");
    auto* form = new QFormLayout(&dialog);

    auto* zone_combo = new QComboBox;
    for (const auto& zone : validation_index.zones) {
        zone_combo->addItem(QString("#%1 %2 [bottom %3 — top %4]")
                                .arg(zone.zone_num)
                                .arg(QString::fromStdString(zone.name))
                                .arg(zone.bottom)
                                .arg(zone.top),
                            zone.zone_num);
    }
    auto* zone_range = new QLabel;
    zone_range->setWordWrap(true);
    auto* kind = new QComboBox;
    kind->addItem("Rooms", QStringLiteral("room"));
    kind->addItem("Mobs", QStringLiteral("mob"));
    kind->addItem("Objects", QStringLiteral("object"));
    auto* start_vnum = new QSpinBox;
    start_vnum->setRange(1, 999999);
    auto* end_vnum = new QSpinBox;
    end_vnum->setRange(1, 999999);
    auto* note = new QLineEdit;

    form->addRow(new QLabel("Zone selection is required. start_vnum and end_vnum are mandatory."));
    form->addRow("Zone:", zone_combo);
    form->addRow("Zone range:", zone_range);
    form->addRow("kind:", kind);
    form->addRow("start_vnum:", start_vnum);
    form->addRow("end_vnum:", end_vnum);
    form->addRow("note:", note);

    const int preferred_zone = preferredZoneNumForNewRoom();
    if (preferred_zone > 0) {
        const int zone_index = zone_combo->findData(preferred_zone);
        if (zone_index >= 0) {
            zone_combo->setCurrentIndex(zone_index);
        }
    }

    auto update_zone_range = [&]() {
        const int zone_num = zone_combo->currentData().toInt();
        const nebbie::WorldIndexZone* zone = nebbie::find_world_index_zone(validation_index, zone_num);
        if (!zone) {
            zone_range->setText("-");
            return;
        }
        zone_range->setText(QString("bottom %1 — top %2 | free room vnums: %3")
                                .arg(zone->bottom)
                                .arg(zone->top)
                                .arg(zone->rooms_free_count));
        start_vnum->setRange(zone->bottom, zone->top);
        end_vnum->setRange(zone->bottom, zone->top);
    };

    auto update_suggestion = [&]() {
        const int zone_num = zone_combo->currentData().toInt();
        const QString kind_value = kind->currentData().toString();
        if (kind_value == QStringLiteral("room")) {
            const auto range = nebbie::suggest_room_vnum_range_in_zone(validation_index, zone_num, 10);
            if (range) {
                start_vnum->setValue(static_cast<int>(range->start));
                end_vnum->setValue(static_cast<int>(range->end));
            } else {
                const auto single = nebbie::suggest_room_vnum_in_zone(validation_index, zone_num);
                const int value = single ? static_cast<int>(*single) : start_vnum->minimum();
                start_vnum->setValue(value);
                end_vnum->setValue(value);
            }
        } else if (kind_value == QStringLiteral("mob")) {
            const int value = static_cast<int>(suggestMobVnum());
            start_vnum->setValue(value);
            end_vnum->setValue(value);
        } else {
            const int value = static_cast<int>(suggestObjectVnum());
            start_vnum->setValue(value);
            end_vnum->setValue(value);
        }
        if (end_vnum->value() < start_vnum->value()) {
            end_vnum->setValue(start_vnum->value());
        }
        update_zone_range();
    };

    connect(zone_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog,
            [&](int) { update_suggestion(); });
    connect(kind, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog,
            [&](int) { update_suggestion(); });
    connect(start_vnum, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, [&](int value) {
        if (end_vnum->value() < value) {
            end_vnum->setValue(value);
        }
    });
    update_suggestion();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (zone_combo->currentIndex() < 0) {
            QMessageBox::warning(&dialog, "Reservations", "Select a zone.");
            return;
        }
        if (end_vnum->value() < start_vnum->value()) {
            QMessageBox::warning(&dialog, "Reservations", "end_vnum must be >= start_vnum.");
            return;
        }
        if (kind->currentData().toString() != QStringLiteral("room")
            && (start_vnum->value() <= 0 || end_vnum->value() <= 0)) {
            QMessageBox::warning(&dialog, "Reservations", "start_vnum and end_vnum are required.");
            return;
        }

        nebbie::WorldIndexReservation reservation;
        reservation.builder = app_config_.builder_name.toStdString();
        reservation.zone_num = zone_combo->currentData().toInt();
        reservation.kind = kind->currentData().toString().toStdString();
        reservation.start_vnum = start_vnum->value();
        reservation.end_vnum = end_vnum->value();
        reservation.note = note->text().toStdString();
        reservation.status = "active";

        std::string error;
        if (!nebbie::validate_reservation(validation_index, reservation, &error)) {
            QMessageBox::warning(&dialog, "Reservations", QString::fromStdString(error));
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    nebbie::WorldIndexReservation reservation;
    reservation.builder = app_config_.builder_name.toStdString();
    reservation.zone_num = zone_combo->currentData().toInt();
    reservation.kind = kind->currentData().toString().toStdString();
    reservation.start_vnum = start_vnum->value();
    reservation.end_vnum = end_vnum->value();
    reservation.note = note->text().toStdString();
    reservation.status = "active";

    QString error;
    if (!nebbie::qt::post_reservation_sync(*network_,
                                           app_config_.coordinator_url,
                                           app_config_.coordinator_token,
                                           reservation,
                                           &error)) {
        QMessageBox::warning(this, "Reservations", error.isEmpty() ? "Request failed." : error);
        return;
    }

    refreshWorldIndex();
    setStatus(QString("Reserved vnums %1-%2 (%3) in zone #%4.")
                  .arg(reservation.start_vnum)
                  .arg(reservation.end_vnum)
                  .arg(QString::fromStdString(reservation.kind))
                  .arg(reservation.zone_num));
}

long MainWindow::currentRoomVnum() const {
    const auto* item = room_list_->currentItem();
    if (!item) {
        return 0;
    }
    return static_cast<long>(item->data(Qt::UserRole).toLongLong());
}

void MainWindow::selectRoomByVnum(const long vnum) {
    for (int i = 0; i < room_list_->count(); ++i) {
        if (room_list_->item(i)->data(Qt::UserRole).toLongLong() == vnum) {
            room_list_->setCurrentRow(i);
            tabs_->setCurrentIndex(0);
            return;
        }
    }
}

void MainWindow::selectMobByVnum(const long vnum) {
    for (int i = 0; i < mob_list_->count(); ++i) {
        if (mob_list_->item(i)->data(Qt::UserRole).toLongLong() == vnum) {
            mob_list_->setCurrentRow(i);
            tabs_->setCurrentIndex(1);
            return;
        }
    }
}

void MainWindow::selectObjectByVnum(const long vnum) {
    for (int i = 0; i < obj_list_->count(); ++i) {
        if (obj_list_->item(i)->data(Qt::UserRole).toLongLong() == vnum) {
            obj_list_->setCurrentRow(i);
            tabs_->setCurrentIndex(2);
            return;
        }
    }
}

void MainWindow::onRoomSearchChanged(const QString& text) {
    room_filter_ = text.trimmed();
    refreshRoomList();
}

void MainWindow::onMobSearchChanged(const QString& text) {
    mob_filter_ = text.trimmed();
    refreshMobList();
}

void MainWindow::onObjSearchChanged(const QString& text) {
    object_filter_ = text.trimmed();
    refreshObjectList();
}

void MainWindow::onRoomSelected() {
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

void MainWindow::onMobSelected() {
    const auto* item = mob_list_->currentItem();
    if (!item) {
        return;
    }
    const long vnum = static_cast<long>(item->data(Qt::UserRole).toLongLong());
    const nebbie::Mobile* mob = world_.find_mobile(vnum);
    if (!mob) {
        return;
    }
    mob_editor_->loadFromMobile(*mob);
}

void MainWindow::onObjSelected() {
    const auto* item = obj_list_->currentItem();
    if (!item) {
        return;
    }
    const long vnum = static_cast<long>(item->data(Qt::UserRole).toLongLong());
    const nebbie::GameObject* obj = world_.find_object(vnum);
    if (!obj) {
        return;
    }
    obj_editor_->loadFromObject(*obj);
}

void MainWindow::goToExitTarget() {
    const long to_room = room_editor_->selectedExitToRoom();
    if (to_room <= 0) {
        QMessageBox::information(this, "Exits", "Select an exit in the Exits tab.");
        return;
    }
    if (!world_.find_room(to_room)) {
        QMessageBox::warning(this, "Exits", QString("Room #%1 does not exist in this library.").arg(to_room));
        return;
    }
    selectRoomByVnum(to_room);
}

void MainWindow::applyRoomChanges() {
    auto* item = room_list_->currentItem();
    if (!item) {
        QMessageBox::information(this, "Rooms", "Select a room.");
        return;
    }
    const long vnum = static_cast<long>(item->data(Qt::UserRole).toLongLong());

    nebbie::Room* room = world_.find_room(vnum);
    if (!room) {
        QMessageBox::warning(this, "Rooms", "Room not found.");
        return;
    }

    const std::string old_name = room->name;
    nebbie::Room updated = *room;
    room_editor_->saveToRoom(updated);
    const bool name_changed = old_name != updated.name;
    nebbie::assign_room_fields(*room, updated);

    std::size_t aligned = 0;
    if (name_changed) {
        aligned = nebbie::refresh_inbound_exit_descriptions(world_, vnum);
        if (aligned > 0) {
            refreshRoomEditorIfInboundExitsChanged(vnum);
        }
    }

    item->setText(QString("#%1 %2").arg(vnum).arg(QString::fromStdString(room->name)));
    markDirty();
    if (aligned > 0) {
        setStatus(QString("Stanza %1 aggiornata: %2 etichetta/e di uscita in entrata allineata/e.")
                      .arg(vnum)
                      .arg(static_cast<qlonglong>(aligned)));
    } else {
        setStatus(QString("Stanza %1 aggiornata in memoria.").arg(vnum));
    }
}

void MainWindow::syncInboundExitLabels() {
    const long vnum = currentRoomVnum();
    if (vnum <= 0) {
        QMessageBox::information(this, "Rooms", "Select a room.");
        return;
    }

    const nebbie::Room* room = world_.find_room(vnum);
    if (!room) {
        QMessageBox::warning(this, "Rooms", "Room not found.");
        return;
    }

    const std::size_t aligned = nebbie::refresh_inbound_exit_descriptions(world_, vnum);
    if (aligned > 0) {
        refreshRoomEditorIfInboundExitsChanged(vnum);
        markDirty();
        setStatus(QString("Stanza %1: %2 etichetta/e di uscita in entrata allineata/e a \"%3\".")
                      .arg(vnum)
                      .arg(static_cast<qlonglong>(aligned))
                      .arg(QString::fromStdString(room->name)));
        return;
    }

    setStatus(QString("Stanza %1: le etichette delle uscite in entrata sono già allineate a \"%2\".")
                  .arg(vnum)
                  .arg(QString::fromStdString(room->name)));
}

void MainWindow::reloadRoomEditor(long vnum) {
    const nebbie::Room* room = world_.find_room(vnum);
    if (room) {
        room_editor_->loadFromRoom(*room);
    }
}

void MainWindow::refreshRoomEditorIfInboundExitsChanged(long target_vnum) {
    const long current = currentRoomVnum();
    if (current <= 0) {
        return;
    }
    if (current == target_vnum) {
        reloadRoomEditor(current);
        return;
    }

    const nebbie::Room* room = world_.find_room(current);
    if (!room) {
        return;
    }
    for (const auto& exit : room->exits) {
        if (exit.to_room == target_vnum) {
            reloadRoomEditor(current);
            return;
        }
    }
}

void MainWindow::applyMobChanges() {
    auto* item = mob_list_->currentItem();
    if (!item) {
        QMessageBox::information(this, "Mob", "Seleziona un mobile.");
        return;
    }
    const long vnum = static_cast<long>(item->data(Qt::UserRole).toLongLong());

    nebbie::Mobile* mob = world_.find_mobile(vnum);
    if (!mob) {
        QMessageBox::warning(this, "Mob", "Mobile non trovato.");
        return;
    }

    nebbie::Mobile updated = *mob;
    mob_editor_->saveToMobile(updated);
    nebbie::assign_mobile_fields(*mob, updated);

    item->setText(QString("#%1 %2").arg(vnum).arg(QString::fromStdString(mob->short_descr)));
    markDirty();
    setStatus(QString("Mobile %1 aggiornato in memoria.").arg(vnum));
}

void MainWindow::applyObjChanges() {
    auto* item = obj_list_->currentItem();
    if (!item) {
        QMessageBox::information(this, "Oggetti", "Seleziona un oggetto.");
        return;
    }
    const long vnum = static_cast<long>(item->data(Qt::UserRole).toLongLong());

    nebbie::GameObject* obj = world_.find_object(vnum);
    if (!obj) {
        QMessageBox::warning(this, "Oggetti", "Oggetto non trovato.");
        return;
    }

    nebbie::GameObject updated = *obj;
    obj_editor_->saveToObject(updated);
    nebbie::assign_object_fields(*obj, updated);

    item->setText(QString("#%1 %2").arg(vnum).arg(QString::fromStdString(obj->short_descr)));
    markDirty();
    setStatus(QString("Oggetto %1 aggiornato in memoria.").arg(vnum));
}

void MainWindow::createRoom() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Stanze", "Apri prima una libreria.");
        return;
    }

    bool ok = false;
    const int suggested = static_cast<int>(suggestRoomVnum());
    const int vnum = QInputDialog::getInt(this, "Nuova stanza", "Vnum stanza:", suggested, 1, 999999, 1, &ok);
    if (!ok) {
        return;
    }
    if (!warnIfRemoteVnumConflict(QStringLiteral("room"), vnum)) {
        return;
    }

    if (!nebbie::create_room(world_, vnum)) {
        QMessageBox::warning(this, "Stanze", QString("Impossibile creare la stanza #%1 (vnum duplicato?).").arg(vnum));
        return;
    }

    refreshRoomList();
    selectRoomByVnum(vnum);
    markDirty();
    setStatus(QString("Creata stanza #%1.").arg(vnum));
}

void MainWindow::createMob() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Mob", "Apri prima una libreria.");
        return;
    }

    bool ok = false;
    const int suggested = static_cast<int>(suggestMobVnum());
    const int vnum = QInputDialog::getInt(this, "Nuovo mob", "Vnum mobile:", suggested, 1, 99999, 1, &ok);
    if (!ok) {
        return;
    }
    if (!warnIfRemoteVnumConflict(QStringLiteral("mob"), vnum)) {
        return;
    }

    if (!nebbie::create_mob(world_, vnum)) {
        QMessageBox::warning(this, "Mob", QString("Impossibile creare il mob #%1 (vnum duplicato?).").arg(vnum));
        return;
    }

    refreshMobList();
    selectMobByVnum(vnum);
    markDirty();
    setStatus(QString("Creato mob #%1.").arg(vnum));
}

void MainWindow::createObject() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Oggetti", "Apri prima una libreria.");
        return;
    }

    bool ok = false;
    const int suggested = static_cast<int>(suggestObjectVnum());
    const int vnum = QInputDialog::getInt(this, "Nuovo oggetto", "Vnum oggetto:", suggested, 1, 99999, 1, &ok);
    if (!ok) {
        return;
    }
    if (!warnIfRemoteVnumConflict(QStringLiteral("object"), vnum)) {
        return;
    }

    if (!nebbie::create_object(world_, vnum)) {
        QMessageBox::warning(this, "Oggetti", QString("Impossibile creare l'oggetto #%1 (vnum duplicato?).").arg(vnum));
        return;
    }

    refreshObjectList();
    selectObjectByVnum(vnum);
    markDirty();
    setStatus(QString("Creato oggetto #%1.").arg(vnum));
}

void MainWindow::showValidation(const nebbie::ValidationReport& report) {
    validation_issues_ = report.issues;
    validation_list_->clear();

    for (std::size_t i = 0; i < validation_issues_.size(); ++i) {
        const auto& issue = validation_issues_[i];
        const char* level = issue.severity == nebbie::ValidationSeverity::error ? "ERRORE" : "AVVISO";
        const QString text = QString("[%1] %2: %3")
                                 .arg(level)
                                 .arg(QString::fromStdString(issue.category))
                                 .arg(QString::fromStdString(issue.message));
        auto* item = new QListWidgetItem(text, validation_list_);
        item->setData(Qt::UserRole, static_cast<qlonglong>(i));
        if (issue.severity == nebbie::ValidationSeverity::error) {
            item->setForeground(Qt::red);
        } else {
            item->setForeground(QColor(180, 120, 0));
        }
    }

    if (validation_list_->count() == 0) {
        validation_list_->addItem("Nessun problema rilevato.");
    } else {
        auto* summary = new QListWidgetItem(
            QString("\n%1 errori, %2 avvisi").arg(report.error_count()).arg(report.warning_count()));
        summary->setFlags(Qt::NoItemFlags);
        validation_list_->addItem(summary);
    }

    tabs_->setCurrentWidget(validation_tab_);
}

void MainWindow::navigateToIssue(const nebbie::ValidationIssue& issue) {
    switch (issue.target) {
    case nebbie::ValidationTarget::room:
        tabs_->setCurrentIndex(0);
        selectRoomByVnum(issue.target_vnum);
        break;
    case nebbie::ValidationTarget::mob:
        tabs_->setCurrentIndex(1);
        selectMobByVnum(issue.target_vnum);
        break;
    case nebbie::ValidationTarget::object:
        tabs_->setCurrentIndex(2);
        selectObjectByVnum(issue.target_vnum);
        break;
    case nebbie::ValidationTarget::zone:
        if (zone_tab_) {
            tabs_->setCurrentWidget(zone_tab_);
        }
        selectZoneByNum(issue.zone_num);
        if (issue.reset_index >= 0 && zone_editor_) {
            zone_editor_->selectResetIndex(issue.reset_index);
        }
        break;
    case nebbie::ValidationTarget::shop:
        setStatus(QString("Shop #%1 — usa la CLI per i dettagli shop.").arg(issue.target_vnum));
        break;
    default:
        break;
    }
}

void MainWindow::onValidationIssueActivated(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    const int index = static_cast<int>(item->data(Qt::UserRole).toLongLong());
    if (index < 0 || index >= static_cast<int>(validation_issues_.size())) {
        return;
    }
    navigateToIssue(validation_issues_[static_cast<std::size_t>(index)]);
}

void MainWindow::validateLib() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Valida", "Apri prima una libreria.");
        return;
    }
    const nebbie::ValidationReport report = nebbie::validate_world(world_);
    showValidation(report);
    if (report.ok()) {
        setStatus("Validazione OK.");
    } else {
        setStatus(QString("Validazione: %1 errori.").arg(report.error_count()));
    }
}

void MainWindow::saveLib() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Salva", "Apri prima una libreria.");
        return;
    }

    const nebbie::ValidationReport report = nebbie::validate_world(world_);
    if (!report.ok()) {
        showValidation(report);
        const auto answer = QMessageBox::question(
            this, "Errori di validazione",
            QString("Ci sono %1 errori. Salvare comunque?").arg(report.error_count()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    try {
        nebbie::save_lib_with_backup(world_, context_, lib_path_);
        markClean();
        last_version_time_ = std::chrono::system_clock::now();
        setStatus("Libreria salvata (backup creato in .nebbie/versions).");
        QMessageBox::information(this, "Salva", "Salvataggio completato. Backup pre-salvataggio creato.");
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Errore", QString::fromUtf8(ex.what()));
    }
}

void MainWindow::saveLibForce() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Salva", "Apri prima una libreria.");
        return;
    }
    try {
        nebbie::save_lib_with_backup(world_, context_, lib_path_);
        markClean();
        last_version_time_ = std::chrono::system_clock::now();
        setStatus("Libreria salvata (forzato, backup creato).");
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Errore", QString::fromUtf8(ex.what()));
    }
}

void MainWindow::onAutosaveTick() {
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

void MainWindow::restoreFromWorkspace() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Cronologia", "Apri prima una libreria.");
        return;
    }

    const auto workspace = nebbie::workspace_dir(lib_path_);
    std::error_code ec;
    if (!std::filesystem::exists(workspace, ec)) {
        QMessageBox::information(this, "Cronologia", "Nessun autosalvataggio workspace trovato.");
        return;
    }

    const auto answer = QMessageBox::question(
        this, "Ripristina autosalvataggio",
        "Ripristinare l'ultimo autosalvataggio workspace? Le modifiche correnti non salvate andranno perse.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    try {
        nebbie::restore_snapshot(world_, context_, workspace);
        markDirty();
        refreshRoomList();
        refreshMobList();
        refreshObjectList();
        refreshZoneList();
        setStatus("Ripristinato autosalvataggio workspace.");
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Errore", QString::fromUtf8(ex.what()));
    }
}

void MainWindow::restoreVersion() {
    if (lib_path_.empty()) {
        QMessageBox::information(this, "Cronologia", "Apri prima una libreria.");
        return;
    }

    const auto versions = nebbie::list_versions(lib_path_);
    if (versions.empty()) {
        QMessageBox::information(this, "Cronologia", "Nessuna versione salvata in .nebbie/versions.");
        return;
    }

    QStringList labels;
    for (const auto& version : versions) {
        labels << QString::fromStdString(version.id + " [" + version.label + "]");
    }

    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        this, "Ripristina versione", "Seleziona versione:", labels, 0, false, &ok);
    if (!ok || chosen.isEmpty()) {
        return;
    }

    const int index = labels.indexOf(chosen);
    if (index < 0) {
        return;
    }

    const auto answer = QMessageBox::question(
        this, "Ripristina versione",
        "Ripristinare la versione selezionata? Le modifiche correnti non salvate andranno perse.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    try {
        const auto snapshot = nebbie::versions_dir(lib_path_) / versions[static_cast<std::size_t>(index)].id;
        nebbie::restore_snapshot(world_, context_, snapshot);
        markDirty();
        refreshRoomList();
        refreshMobList();
        refreshObjectList();
        refreshZoneList();
        setStatus(QString("Ripristinata versione %1.").arg(chosen));
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Errore", QString::fromUtf8(ex.what()));
    }
}

void MainWindow::setStatus(const QString& message) {
    statusBar()->showMessage(message);
}

void MainWindow::markDirty() {
    dirty_ = true;
    setWindowTitle("Nebbie Editor *");
}

void MainWindow::markClean() {
    dirty_ = false;
    setWindowTitle("Nebbie Editor");
}

bool MainWindow::confirmSaveIfDirty() {
    if (!dirty_) {
        return true;
    }
    const auto answer = QMessageBox::question(
        this, "Modifiche non salvate",
        "Ci sono modifiche non salvate. Salvare prima di continuare?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save) {
        saveLib();
        return !dirty_;
    }
    if (answer == QMessageBox::Discard) {
        return true;
    }
    return false;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!confirmSaveIfDirty()) {
        event->ignore();
        return;
    }
    event->accept();
}
