#include "MainWindow.h"

#include "EditDialog.h"
#include "ImportDialog.h"
#include "Translator.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <functional>

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QScrollBar>
#include <QPixmap>
#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QPainter>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QTreeView>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QVector>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {

class PeopleTree : public QTreeView
{
public:
    using QTreeView::QTreeView;

protected:
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const bool group = !index.parent().isValid() && model() && model()->hasChildren(index);
        if (!group) {
            QTreeView::drawRow(painter, option, index);
            return;
        }

        const QRect row(0, option.rect.y(), viewport()->width(), option.rect.height());
        painter->fillRect(row, QColor(226, 234, 244));

        const int indent = indentation();
        drawBranches(painter, QRect(0, row.top(), indent, row.height()), index);

        QFont font = option.font;
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(option.palette.color(QPalette::Text));
        const QRect textRect(indent + 4, row.top(), qMax(0, row.width() - indent - 8), row.height());
        const QString title = index.data(Qt::DisplayRole).toString();
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(title, Qt::ElideRight, textRect.width()));
    }
};

class PeopleDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(size.height() + 6);
        return size;
    }
};

QIcon toolbarIcon(int index)
{
    static QPixmap sheet;
    if (sheet.isNull()) {
        QImage image(QStringLiteral(":/Toolbar.bmp"));
        image = image.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixelColor(x, y) == QColor(0, 0, 0))
                    image.setPixelColor(x, y, Qt::transparent);
            }
        }
        sheet = QPixmap::fromImage(image);
    }
    return QIcon(sheet.copy(index * 16, 0, 16, 16));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    resize(1280, 800);

    m_people = new PersonModel(&m_catalog, this);
    m_stats = new StatsModel(&m_catalog, this);
    m_dictionary = new DictionaryModel(&m_catalog, this);

    // действие панели и меню: значок из полосы, текст меню, короткий текст
    // под значком (текст меню, если пусто), подсказка
    const auto makeAction = [this](int icon, const QString& text, const QString& iconText, const QString& toolTip,
                                   auto&& slot) {
        auto* action = new QAction(toolbarIcon(icon), text, this);
        if (!iconText.isEmpty())
            action->setIconText(iconText);
        action->setToolTip(toolTip);
        connect(action, &QAction::triggered, this, std::forward<decltype(slot)>(slot));
        return action;
    };
    auto* openDb = makeAction(0, QStringLiteral("Открыть базу..."), QStringLiteral("База данных"),
                              QStringLiteral("Сменить базу данных"), &MainWindow::chooseDatabase);
    auto* importDecrees = makeAction(1, QStringLiteral("Импорт указов..."), QStringLiteral("Импорт указов"),
                                     QStringLiteral("Импорт указов из файлов Word (docx)"), [this] {
        if (!m_catalog.isOpen())
            return;
        ImportDialog dialog(&m_catalog, this);
        dialog.exec();
    });
    auto* exportCsv = makeAction(2, QStringLiteral("Экспорт награждений..."), QStringLiteral("Экспорт награждений"),
                                 QStringLiteral("Экспорт награждений в csv файл"), &MainWindow::exportCsv);
    auto* addRecord = makeAction(3, QStringLiteral("Добавить запись"), QStringLiteral("Добавить"),
                                 QStringLiteral("Добавить запись"), [this] { addPerson(nullptr); });
    auto* deleteRecord = makeAction(4, QStringLiteral("Удалить запись"), QStringLiteral("Удалить"),
                                    QStringLiteral("Удалить запись"), &MainWindow::deleteSelected);
    auto* editRecord = makeAction(5, QStringLiteral("Редактировать запись"), QStringLiteral("Редактировать"),
                                  QStringLiteral("Редактировать запись"), &MainWindow::editCurrent);
    auto* copyUkase = makeAction(6, QStringLiteral("Копия указа"), QString(),
                                 QStringLiteral("Создать копию указа о награждении"), [this] {
        const auto index = m_peopleView->currentIndex();
        addPerson(index.isValid() ? m_people->personAt(index) : nullptr);
    });
    auto* printAction = makeAction(7, QStringLiteral("Печать..."), QString(), QStringLiteral("Печать"),
                                   &MainWindow::printCurrent);

    auto* fileMenu = menuBar()->addMenu(QStringLiteral("Файл"));
    fileMenu->addAction(openDb);
    fileMenu->addAction(importDecrees);
    fileMenu->addAction(exportCsv);
    fileMenu->addSeparator();
    fileMenu->addAction(printAction);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Выход"), this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu(QStringLiteral("Правка"));
    editMenu->addAction(editRecord);
    editMenu->addAction(addRecord);
    editMenu->addAction(copyUkase);
    editMenu->addAction(deleteRecord);
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Поиск и замена..."), this, &MainWindow::replaceText);
    editMenu->addAction(QStringLiteral("Исправить латинские буквы в кириллице..."), this,
                        &MainWindow::fixLatinBelarusianI);
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("API-ключ Яндекса..."), this, [this] {
        bool ok = false;
        const QString key = QInputDialog::getText(this, QStringLiteral("API-ключ Яндекс.Переводчика"),
            QStringLiteral("Ключ (хранится в настройках пользователя):"), QLineEdit::Password,
            yandexApiKey(), &ok);
        if (ok)
            setYandexApiKey(key);
    });

    auto* toolbar = addToolBar(QStringLiteral("Стандартная"));
    toolbar->setObjectName(QStringLiteral("Standard"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(openDb);
    toolbar->addAction(importDecrees);
    toolbar->addAction(exportCsv);
    toolbar->addSeparator();
    toolbar->addAction(addRecord);
    toolbar->addAction(deleteRecord);
    toolbar->addAction(editRecord);
    toolbar->addAction(copyUkase);
    toolbar->addSeparator();
    toolbar->addAction(printAction);
    if (auto* printButton = qobject_cast<QToolButton*>(toolbar->widgetForAction(printAction)))
        printButton->setToolButtonStyle(Qt::ToolButtonIconOnly);

    auto* viewMenu = menuBar()->addMenu(QStringLiteral("Вид"));
    auto* groupAction = viewMenu->addAction(QStringLiteral("Группировать по награде"));
    groupAction->setCheckable(true);
    groupAction->setChecked(true);
    // только держит пункт меню и флажок в одном состоянии
    connect(groupAction, &QAction::toggled, this, [this](bool checked) {
        if (m_group && m_group->isChecked() != checked)
            m_group->setChecked(checked);
    });

    auto* helpMenu = menuBar()->addMenu(QStringLiteral("Справка"));
    helpMenu->addAction(QStringLiteral("О программе..."), this, [this] {
        QMessageBox::about(this, QStringLiteral("Кавалер"),
            QStringLiteral("Каталог государственных наград Беларуси.\n"
                           "База SQLite, интерфейс Qt.\n\n"
                           "Импорт указов из Word (docx), перевод через Яндекс."));
    });

    m_award = new QComboBox;
    m_year = new QComboBox;
    m_name = new QLineEdit;
    m_name->setPlaceholderText(QStringLiteral("от 3 букв"));
    m_quick = new QLineEdit;
    m_quick->setPlaceholderText(QStringLiteral("по видимым полям"));
    m_quotes = new QCheckBox(QStringLiteral("Проверка кавычек"));
    m_needTranslation = new QCheckBox(QStringLiteral("Требующие перевода"));
    m_group = new QCheckBox(QStringLiteral("Группировать"));
    m_group->setChecked(true);
    connect(m_group, &QCheckBox::toggled, groupAction, &QAction::setChecked);

    auto* filters = new QHBoxLayout;
    filters->addWidget(new QLabel(QStringLiteral("Награда:")));
    filters->addWidget(m_award, 2);
    filters->addWidget(new QLabel(QStringLiteral("Год:")));
    filters->addWidget(m_year);
    filters->addWidget(new QLabel(QStringLiteral("Имя:")));
    filters->addWidget(m_name);
    filters->addWidget(new QLabel(QStringLiteral("Фильтр:")));
    filters->addWidget(m_quick);
    filters->addWidget(m_quotes);
    filters->addWidget(m_needTranslation);
    filters->addWidget(m_group);

    m_peopleView = new PeopleTree;
    m_peopleView->setModel(m_people);
    m_peopleView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_peopleView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_peopleView->setAlternatingRowColors(true);
    m_peopleView->setSortingEnabled(true);
    m_peopleView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_peopleView->setWordWrap(false);
    m_peopleView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_peopleView->setExpandsOnDoubleClick(true);
    m_peopleView->setUniformRowHeights(true);
    m_peopleView->setAnimated(false);
    m_peopleView->setItemDelegate(new PeopleDelegate(m_peopleView));
    m_peopleView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_peopleView->header()->setStretchLastSection(false);
    m_peopleView->header()->setMinimumSectionSize(20);
    m_peopleView->header()->setSortIndicator(3, Qt::AscendingOrder);
    const int columnWeights[] = {200, 200, 400, 100, 50, 50};
    for (int column = 0; column < 6; ++column)
        m_peopleView->setColumnWidth(column, columnWeights[column]);
    m_peopleView->viewport()->installEventFilter(this);
    connect(m_peopleView->header(), &QHeaderView::sectionResized, this,
            [this](int logical, int, int newSize) { adjustPeopleColumn(logical, newSize); });
    // фильтры, группировка и сортировка сбрасывают модель; изменения каталога
    // запоминаются раньше (Catalog::aboutToChange), пока people() ещё старый
    connect(m_people, &QAbstractItemModel::modelAboutToBeReset, this, [this] {
        if (!m_peopleState.valid && !m_openingDatabase)
            capturePeopleState();
    });
    connect(m_people, &QAbstractItemModel::modelReset, this, [this] {
        const bool grouped = m_people->isGrouped();
        m_peopleView->setRootIsDecorated(grouped);
        m_peopleView->setIndentation(grouped ? 18 : 0);
        m_peopleView->setTreePosition(grouped ? 1 : 0);
        m_adjustingColumns = true;
        m_peopleView->setColumnHidden(0, grouped);
        m_adjustingColumns = false;
        if (grouped && m_people->shownCount() <= 10000)
            m_peopleView->expandAll();
        layoutPeopleColumns();
        restorePeopleState();
    });
    connect(m_peopleView, &QTableView::doubleClicked, this, [this] { editCurrent(); });
    connect(m_peopleView, &QWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);

    auto* peoplePage = new QWidget;
    auto* peopleLayout = new QVBoxLayout(peoplePage);
    peopleLayout->addLayout(filters);
    peopleLayout->addWidget(m_peopleView);

    m_statsByMonth = new QCheckBox(QStringLiteral("По месяцам"));
    m_statsByMonth->setChecked(true);
    m_statsView = new QTableView;
    m_statsView->setModel(m_stats);
    m_statsView->setAlternatingRowColors(true);
    m_statsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_statsView->verticalHeader()->setVisible(false);
    m_statsView->horizontalHeader()->setStretchLastSection(true);
    m_statsView->setColumnWidth(0, 280);
    auto* statsPage = new QWidget;
    auto* statsLayout = new QVBoxLayout(statsPage);
    statsLayout->addWidget(m_statsByMonth);
    statsLayout->addWidget(m_statsView);

    m_dictFilter = new QLineEdit;
    m_dictFilter->setPlaceholderText(QStringLiteral("Фильтр словаря"));
    m_dictView = new QTableView;
    m_dictView->setModel(m_dictionary);
    m_dictView->setAlternatingRowColors(true);
    m_dictView->setSortingEnabled(false);
    m_dictView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dictView->verticalHeader()->setVisible(false);
    m_dictView->horizontalHeader()->setStretchLastSection(true);
    m_dictView->setColumnWidth(0, 110);
    m_dictView->setColumnWidth(1, 180);
    m_dictView->setColumnWidth(2, 180);
    auto* dictPage = new QWidget;
    auto* dictLayout = new QVBoxLayout(dictPage);
    dictLayout->addWidget(m_dictFilter);
    dictLayout->addWidget(m_dictView);

    m_tabs = new QTabWidget;
    m_tabs->addTab(peoplePage, QStringLiteral("Список награждений"));
    m_tabs->addTab(statsPage, QStringLiteral("Статистика"));
    m_tabs->addTab(dictPage, QStringLiteral("Словарь имен"));
    setCentralWidget(m_tabs);

    m_count = new QLabel;
    statusBar()->addWidget(m_count);

    const auto refresh = [this] { refreshPeople(); };
    connect(m_award, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_year, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_quotes, &QCheckBox::toggled, this, [this](bool checked) { m_people->setCheckQuotes(checked); });
    connect(m_needTranslation, &QCheckBox::toggled, this, refresh);
    connect(m_group, &QCheckBox::toggled, this, refresh);
    // оба текстовых фильтра ждут паузы в наборе
    auto* quickTimer = new QTimer(this);
    quickTimer->setSingleShot(true);
    quickTimer->setInterval(200);
    connect(m_name, &QLineEdit::textChanged, quickTimer, [quickTimer](const QString& text) {
        if (text.size() >= 3 || text.isEmpty())
            quickTimer->start();
    });
    connect(m_quick, &QLineEdit::textChanged, quickTimer, [quickTimer](const QString&) { quickTimer->start(); });
    connect(quickTimer, &QTimer::timeout, this, refresh);
    connect(m_statsByMonth, &QCheckBox::toggled, this, [this] { refreshStats(); });
    connect(m_dictFilter, &QLineEdit::textChanged, this, [this] { refreshDictionary(); });
    connect(&m_catalog, &Catalog::aboutToChange, this, [this] {
        if (!m_openingDatabase)
            capturePeopleState();
    });
    connect(&m_catalog, &Catalog::dataChanged, this, [this] {
        refreshPeople();
        // статистика и словарь имён пересобираются, только когда на них смотрят
        m_statsStale = true;
        m_dictionaryStale = true;
        refreshVisibleTab();
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] { refreshVisibleTab(); });

    const QString path = storedDatabasePath();
    if (!path.isEmpty() && QFileInfo::exists(path))
        openDatabase(path);
    else
        statusBar()->showMessage(QStringLiteral("Откройте файл базы SQLite"));
}

QString MainWindow::storedDatabasePath() const
{
    // пусто при первом запуске: окно просит открыть базу
    return QSettings().value(QStringLiteral("Database")).toString();
}

void MainWindow::rememberDatabasePath(const QString& path)
{
    QSettings settings;
    settings.setValue(QStringLiteral("Database"), path);
}

void MainWindow::chooseDatabase()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Открыть базу"), storedDatabasePath(),
        QStringLiteral("SQLite (*.sqlite *.db);;Все файлы (*.*)"));
    if (!path.isEmpty())
        openDatabase(path);
}

void MainWindow::openDatabase(const QString& path)
{
    QString error;
    // другая база: номера старой ничего не значат, вид начинаем заново
    m_openingDatabase = true;
    m_peopleState = {};
    const bool opened = m_catalog.open(path, &error);
    m_openingDatabase = false;
    if (!opened) {
        // негодный файл отвергается до закрытия текущей базы;
        // без открытой базы остаёмся только если сбой случился при загрузке
        if (!m_catalog.isOpen()) {
            setWindowTitle(QStringLiteral("Кавалер"));
            fillFilters();
        }
        QMessageBox::critical(this, windowTitle(), error);
        return;
    }
    rememberDatabasePath(path);
    setWindowTitle(QStringLiteral("Кавалер — %1").arg(path));
    fillFilters();
    m_openingDatabase = true;
    refreshPeople();
    m_openingDatabase = false;
    m_statsStale = true;
    m_dictionaryStale = true;
    refreshVisibleTab();
}

void MainWindow::fillFilters()
{
    m_filling = true;
    m_award->clear();
    m_award->addItem(QStringLiteral("<Все награды>"), -1);
    QVector<Award> awards = m_catalog.awards();
    std::sort(awards.begin(), awards.end(), [](const Award& a, const Award& b) {
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });
    for (const Award& award : awards)
        m_award->addItem(award.name, award.id);

    m_year->clear();
    m_year->addItem(QStringLiteral("<Все>"), -1);
    int latestYear = 0;
    // не stats(): иначе при запуске соберётся всё производное
    latestYear = qMax(latestYear, m_catalog.latestYear());
    const int currentYear = QDate::currentDate().year();
    for (int year = m_catalog.yearMin(); year <= currentYear; ++year) {
        m_year->addItem(QString::number(year), year);
        if (year == latestYear)
            m_year->setCurrentIndex(m_year->count() - 1);
    }
    m_filling = false;
}

void MainWindow::refreshPeople()
{
    if (m_filling || !m_catalog.isOpen())
        return;
    const QString name = m_name->text().trimmed();
    m_people->apply(
        m_award->currentData().toInt(),
        m_year->currentData().toInt(),
        name.size() >= 3 ? name : QString(),
        m_quick->text().trimmed(),
        m_needTranslation->isChecked(),
        m_quotes->isChecked(),
        m_group->isChecked());
    updateStatus();
}

void MainWindow::refreshStats()
{
    if (!m_catalog.isOpen())
        return;
    m_stats->reload(m_statsByMonth->isChecked());
    m_statsStale = false;
}

void MainWindow::refreshDictionary()
{
    if (!m_catalog.isOpen())
        return;
    m_dictionary->reload(m_dictFilter->text().trimmed());
    m_dictionaryStale = false;
}

void MainWindow::refreshVisibleTab()
{
    if (m_tabs->currentIndex() == 1 && m_statsStale)
        refreshStats();
    else if (m_tabs->currentIndex() == 2 && m_dictionaryStale)
        refreshDictionary();
}

void MainWindow::capturePeopleState()
{
    PeopleViewState state;
    state.valid = true;

    // ближайшая строка человека от index (строки групп пропускаются)
    const auto personId = [this](QModelIndex index, bool down) {
        for (int step = 0; index.isValid() && step < 64; ++step) {
            if (const Person* person = m_people->personAt(index))
                return person->id;
            index = down ? m_peopleView->indexBelow(index) : m_peopleView->indexAbove(index);
        }
        return 0;
    };

    const QModelIndex current = m_peopleView->currentIndex();
    if (const Person* person = current.isValid() ? m_people->personAt(current) : nullptr) {
        state.currentId = person->id;
        state.currentColumn = current.column();
        state.currentVisible = m_peopleView->viewport()->rect().intersects(m_peopleView->visualRect(current));
        state.nextId = personId(m_peopleView->indexBelow(current), true);
        state.previousId = personId(m_peopleView->indexAbove(current), false);
    }
    for (const QModelIndex& row : m_peopleView->selectionModel()->selectedRows()) {
        if (const Person* person = m_people->personAt(row))
            state.selectedIds.insert(person->id);
    }
    state.topId = personId(m_peopleView->indexAt(QPoint(1, 1)), true);
    state.scrollValue = m_peopleView->verticalScrollBar()->value();
    if (m_people->isGrouped()) {
        for (int row = 0; row < m_people->rowCount(); ++row) {
            const QModelIndex group = m_people->index(row, 0);
            state.expandedGroups.insert(m_people->groupAwardId(group), m_peopleView->isExpanded(group));
        }
    }
    m_peopleState = state;
}

void MainWindow::restorePeopleState()
{
    if (!m_peopleState.valid)
        return;
    const PeopleViewState state = m_peopleState;
    m_peopleState = {};

    if (m_people->isGrouped()) {
        for (auto it = state.expandedGroups.cbegin(); it != state.expandedGroups.cend(); ++it) {
            const QModelIndex group = m_people->groupIndex(it.key());
            if (group.isValid())
                m_peopleView->setExpanded(group, it.value());
        }
    }

    QSet<int> wanted = state.selectedIds;
    for (int id : {state.currentId, state.nextId, state.previousId, state.topId}) {
        if (id > 0)
            wanted.insert(id);
    }
    const QHash<int, QModelIndex> found = m_people->indexesOfPersons(wanted);
    const int lastColumn = m_people->columnCount() - 1;
    const auto shown = [this](const QModelIndex& index) {
        return index.isValid() && (!index.parent().isValid() || m_peopleView->isExpanded(index.parent()));
    };

    // соседние строки становятся одним диапазоном: вид сверяет каждую рисуемую ячейку
    // со всеми диапазонами, диапазон на строку тормозит большое выделение
    QModelIndexList rows;
    for (int id : state.selectedIds) {
        const QModelIndex index = found.value(id);
        if (index.isValid())
            rows.push_back(index);
    }
    std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) {
        if (a.parent() != b.parent())
            return a.parent().row() < b.parent().row();
        return a.row() < b.row();
    });
    QItemSelection selection;
    for (qsizetype i = 0; i < rows.size();) {
        qsizetype j = i;
        while (j + 1 < rows.size() && rows.at(j + 1).parent() == rows.at(i).parent()
               && rows.at(j + 1).row() == rows.at(j).row() + 1)
            ++j;
        selection.select(rows.at(i), rows.at(j).siblingAtColumn(lastColumn));
        i = j + 1;
    }

    // текущего человека может не быть (удалён, отфильтрован): берём соседа
    QModelIndex current = found.value(state.currentId);
    const bool replaced = !current.isValid();
    if (replaced) {
        current = found.value(state.nextId);
        if (!current.isValid())
            current = found.value(state.previousId);
    }
    QItemSelectionModel* selectionModel = m_peopleView->selectionModel();
    if (current.isValid()) {
        int column = state.currentColumn;
        if (m_peopleView->isColumnHidden(column))
            column = 1;
        selectionModel->setCurrentIndex(current.siblingAtColumn(column), QItemSelectionModel::NoUpdate);
        if (replaced && selection.isEmpty() && !state.selectedIds.isEmpty())
            selection.select(current, current.siblingAtColumn(lastColumn));
    }
    selectionModel->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    const QModelIndex top = found.value(state.topId);
    if (shown(top))
        m_peopleView->scrollTo(top, QAbstractItemView::PositionAtTop);
    else
        m_peopleView->verticalScrollBar()->setValue(state.scrollValue);
    if (shown(current) && (state.currentVisible || replaced))
        m_peopleView->scrollTo(current, QAbstractItemView::EnsureVisible);
}

void MainWindow::selectPerson(int personId)
{
    const QModelIndex index = m_people->indexesOfPersons({personId}).value(personId);
    if (!index.isValid())
        return;
    if (index.parent().isValid())
        m_peopleView->setExpanded(index.parent(), true);
    const QModelIndex current = index.siblingAtColumn(1);
    m_peopleView->selectionModel()->setCurrentIndex(current,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_peopleView->scrollTo(current, QAbstractItemView::EnsureVisible);
}

QVector<Person> MainWindow::selectedPeople() const
{
    QVector<Person> people;
    for (const QModelIndex& index : m_peopleView->selectionModel()->selectedRows()) {
        if (const Person* person = m_people->personAt(index))
            people.push_back(*person);
    }
    return people;
}

// одна транзакция и одно обновление на всё выделение
bool MainWindow::saveChanged(QVector<Person>& people)
{
    if (people.isEmpty())
        return true;
    QString error;
    if (m_catalog.saveMany(people, &error))
        return true;
    QMessageBox::critical(this, windowTitle(), error);
    return false;
}

void MainWindow::updateStatus()
{
    m_count->setText(QStringLiteral("Показано %1 из %2")
                         .arg(m_people->shownCount())
                         .arg(m_catalog.people().size()));
}

void MainWindow::editCurrent()
{
    const QModelIndex index = m_peopleView->currentIndex();
    const Person* person = index.isValid() ? m_people->personAt(index) : nullptr;
    if (!person)
        return;
    EditDialog dialog(&m_catalog, *person, this);
    dialog.exec();
}

void MainWindow::addPerson(const Person* copyFrom)
{
    if (!m_catalog.isOpen())
        return;
    Person person;
    person.language = kLangRussian;
    if (copyFrom) {
        person.awardId = copyFrom->awardId;
        person.awardedFor = copyFrom->awardedFor;
        person.ukaseDate = copyFrom->ukaseDate;
        person.ukaseNo = copyFrom->ukaseNo;
        person.personal = copyFrom->personal;
        person.personalBy = copyFrom->personalBy;
    } else {
        const QModelIndex index = m_peopleView->currentIndex();
        if (const Person* current = index.isValid() ? m_people->personAt(index) : nullptr)
            person.awardId = current->awardId;
        else if (m_award->currentData().toInt() > 0)
            person.awardId = m_award->currentData().toInt();
    }
    EditDialog dialog(&m_catalog, person, this);
    if (dialog.exec() == QDialog::Accepted)
        selectPerson(dialog.person().id);
}

void MainWindow::deleteSelected()
{
    const QModelIndexList rows = m_peopleView->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return;
    if (QMessageBox::question(this, windowTitle(), QStringLiteral("Удалить запись из базы?")) != QMessageBox::Yes)
        return;

    QList<int> ids;
    for (const QModelIndex& index : rows) {
        if (const Person* person = m_people->personAt(index))
            ids.push_back(person->id);
    }
    QString error;
    if (!m_catalog.remove(ids, &error))
        QMessageBox::critical(this, windowTitle(), error);
}

void MainWindow::showContextMenu(const QPoint& pos)
{
    const QModelIndex index = m_peopleView->indexAt(pos);
    if (!index.isValid())
        return;
    const Person* person = m_people->personAt(index);
    if (!person)
        return;

    QMenu menu(this);
    QAction* cloudAll = menu.addAction(QStringLiteral("Перевести все (Cloud)"));
    QAction* cloudName = menu.addAction(QStringLiteral("Перевести только имя (Cloud)"));
    menu.addSeparator();
    QAction* byDict = menu.addAction(QStringLiteral("Перевести имя по словарю"));
    QAction* prof = menu.addAction(QStringLiteral("Перевести профессию по словарю"));
    QAction* quotes = menu.addAction(QStringLiteral("Исправить кавычки"));
    menu.addSeparator();
    QMenu* variants = menu.addMenu(QStringLiteral("Варианты перевода"));
    const QStringList suggestions = m_people->suggestions(person->id);
    variants->setEnabled(!suggestions.isEmpty());
    for (const QString& suggestion : suggestions) {
        QAction* action = variants->addAction(suggestion);
        connect(action, &QAction::triggered, this, [this, id = person->id, suggestion] {
            applySuggestion(id, suggestion);
        });
    }

    QAction* chosen = menu.exec(m_peopleView->viewport()->mapToGlobal(pos));
    if (chosen == cloudAll)
        translateSelectedCloud(CloudScope::All);
    else if (chosen == cloudName)
        translateSelectedCloud(CloudScope::NameOnly);
    else if (chosen == byDict)
        translateSelectedByDictionary();
    else if (chosen == prof)
        translateSelectedProfession();
    else if (chosen == quotes)
        fixSelectedQuotes();
}

void MainWindow::translateSelectedCloud(CloudScope scope)
{
    QString failure;
    const QVector<Person> selected = selectedPeople();
    QVector<Person> translated = selected;
    // запросы крутят вложенный цикл событий: модальный прогресс не даёт
    // править или удалять эти записи, пока идёт перевод (копии сохраняются ниже)
    QProgressDialog progress(QStringLiteral("Перевод через Яндекс..."), QStringLiteral("Отмена"), 0,
                             int(selected.size()), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    const int done = translateWithCloud(translated, scope, false, &failure, [&progress](int count) {
        progress.setValue(count);
        return !progress.wasCanceled();
    }, [&progress] { return progress.wasCanceled(); });
    progress.reset();

    QVector<Person> changed;
    for (int i = 0; i < done; ++i) {
        const Person& person = translated.at(i);
        const Person& original = selected.at(i);
        if (person.name != original.name || person.personal != original.personal || person.rank != original.rank)
            changed.push_back(person);
    }
    // оставляем то, что успели перевести до сетевой ошибки
    const bool saved = saveChanged(changed);
    if (!failure.isEmpty()) {
        QMessageBox::critical(this, windowTitle(), saved && !changed.isEmpty()
            ? QStringLiteral("%1\n\nПереведено и сохранено записей: %2.").arg(failure).arg(changed.size())
            : failure);
    }
}

bool MainWindow::modifySelected(const std::function<bool(Person&)>& change, QVector<Person>* saved)
{
    QVector<Person> changed;
    for (Person person : selectedPeople()) {
        if (change(person))
            changed.push_back(person);
    }
    const bool ok = saveChanged(changed);
    if (saved)
        *saved = changed;
    return ok;
}

void MainWindow::translateSelectedByDictionary()
{
    QHash<int, QStringList> suggestions;
    QVector<Person> changed;
    const bool saved = modifySelected([&](Person& person) {
        if (!person.needTranslation() || !person.name.isEmpty())
            return false;
        const QStringList names = m_catalog.translateName(person.nameBy);
        if (names.size() != 1) {
            suggestions.insert(person.id, names);
            return false;
        }
        person.name = names.first();
        return true;
    }, &changed);
    m_people->setSuggestions(suggestions);
    if (saved) {
        for (const Person& person : changed)
            m_people->clearSuggestions(person.id);
    }
}

void MainWindow::translateSelectedProfession()
{
    modifySelected([this](Person& person) {
        if (!person.needTranslation() || !person.personal.isEmpty())
            return false;
        const QString profession = m_catalog.professionRu(person.personalBy);
        if (profession.isEmpty())
            return false;
        person.personal = profession;
        if (!person.rankBy.isEmpty() && person.rank.isEmpty()) {
            const QString rank = m_catalog.rankRu(person.rankBy);
            if (rank.isEmpty())
                return false;
            person.rank = rank;
        }
        return true;
    });
}

void MainWindow::applySuggestion(int personId, const QString& name)
{
    const Person* person = m_catalog.person(personId);
    if (!person)
        return;
    Person updated = *person;
    updated.name = name;
    QString error;
    if (!m_catalog.save(updated, &error))
        QMessageBox::critical(this, windowTitle(), error);
    else
        m_people->clearSuggestions(personId);
}

void MainWindow::fixSelectedQuotes()
{
    modifySelected([this](Person& person) { return m_catalog.fixQuotes(person); });
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (m_peopleView && watched == m_peopleView->viewport() && event->type() == QEvent::Resize)
        layoutPeopleColumns();
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::layoutPeopleColumns()
{
    if (m_adjustingColumns || !m_peopleView)
        return;
    const int available = m_peopleView->viewport()->width();
    if (available <= 0)
        return;

    QVector<int> visible;
    int total = 0;
    for (int column = 0; column < 6; ++column) {
        if (m_peopleView->isColumnHidden(column))
            continue;
        visible.push_back(column);
        total += qMax(1, m_peopleView->columnWidth(column));
    }
    if (visible.isEmpty() || total == available)
        return;

    m_adjustingColumns = true;
    int used = 0;
    for (int index = 0; index < visible.size(); ++index) {
        const int column = visible.at(index);
        const int weight = qMax(1, m_peopleView->columnWidth(column));
        const int rooms = visible.size() - index - 1;
        int width = (index + 1 == visible.size()) ? available - used : weight * available / total;
        width = qBound(20, width, available - used - 20 * rooms);
        m_peopleView->setColumnWidth(column, width);
        used += width;
    }
    m_adjustingColumns = false;
}

void MainWindow::adjustPeopleColumn(int logical, int newSize)
{
    if (m_adjustingColumns || !m_peopleView || m_peopleView->isColumnHidden(logical))
        return;
    const int available = m_peopleView->viewport()->width();
    if (available <= 0)
        return;

    QVector<int> left;
    QVector<int> right;
    int leftWidth = 0;
    int rightWidth = 0;
    for (int column = 0; column < 6; ++column) {
        if (column == logical || m_peopleView->isColumnHidden(column))
            continue;
        if (column < logical) {
            left.push_back(column);
            leftWidth += m_peopleView->columnWidth(column);
        } else {
            right.push_back(column);
            rightWidth += m_peopleView->columnWidth(column);
        }
    }

    QVector<int> flexible = right.isEmpty() ? left : right;
    int flexibleWidth = right.isEmpty() ? leftWidth : rightWidth;
    if (flexible.isEmpty())
        return;

    const int reserved = 20 * flexible.size();
    newSize = qBound(20, newSize, available - (right.isEmpty() ? 0 : leftWidth) - reserved);
    int rest = available - (right.isEmpty() ? 0 : leftWidth) - newSize;
    if (right.isEmpty())
        rest = available - newSize;

    m_adjustingColumns = true;
    m_peopleView->setColumnWidth(logical, newSize);
    int used = 0;
    for (int index = 0; index < flexible.size(); ++index) {
        const int column = flexible.at(index);
        const int weight = qMax(1, m_peopleView->columnWidth(column));
        const int rooms = flexible.size() - index - 1;
        int width = (index + 1 == flexible.size()) ? rest - used : weight * rest / qMax(1, flexibleWidth);
        width = qBound(20, width, rest - used - 20 * rooms);
        m_peopleView->setColumnWidth(column, width);
        used += width;
    }
    m_adjustingColumns = false;
}

void MainWindow::printCurrent()
{
    QAbstractItemView* view = m_peopleView;
    if (m_tabs->currentIndex() == 1)
        view = m_statsView;
    else if (m_tabs->currentIndex() == 2)
        view = m_dictView;

    const QAbstractItemModel* model = view->model();
    QString html = QStringLiteral("<table border='1' cellspacing='0' cellpadding='4' style='border-collapse:collapse'><tr>");
    for (int column = 0; column < model->columnCount(); ++column)
        html += QStringLiteral("<th>") + model->headerData(column, Qt::Horizontal).toString().toHtmlEscaped() + QStringLiteral("</th>");
    html += QStringLiteral("</tr>");
    const auto appendRows = [&](const auto& appendRows, const QModelIndex& parent) -> void {
        for (int row = 0; row < model->rowCount(parent); ++row) {
            const QModelIndex first = model->index(row, 0, parent);
            html += QStringLiteral("<tr>");
            for (int column = 0; column < model->columnCount(parent); ++column)
                html += QStringLiteral("<td>") + model->index(row, column, parent).data().toString().toHtmlEscaped() + QStringLiteral("</td>");
            html += QStringLiteral("</tr>");
            if (model->rowCount(first) > 0)
                appendRows(appendRows, first);
        }
    };
    appendRows(appendRows, QModelIndex());
    html += QStringLiteral("</table>");

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(QStringLiteral("Печать"));
    if (dialog.exec() != QDialog::Accepted)
        return;

    QTextDocument document;
    document.setHtml(html);
    document.print(&printer);
}

void MainWindow::exportCsv()
{
    if (!m_catalog.isOpen())
        return;

    QSettings settings;
    // на стеке: диалог и его виджеты удалятся, когда функция вернётся
    QDialog dialogObject(this);
    QDialog* dialog = &dialogObject;
    dialog->setWindowTitle(QStringLiteral("Экспорт награждений в CSV файл"));
    auto* all = new QRadioButton(QStringLiteral("Все"), dialog);
    auto* from = new QRadioButton(QStringLiteral("С даты:"), dialog);
    all->setChecked(true);
    auto* date = new QDateEdit(QDate::currentDate(), dialog);
    date->setCalendarPopup(true);
    date->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    date->setEnabled(false);
    connect(from, &QRadioButton::toggled, date, &QWidget::setEnabled);

    auto* path = new QLineEdit(settings.value(QStringLiteral("ExportPath")).toString(), dialog);
    auto* browse = new QPushButton(QStringLiteral("..."), dialog);
    connect(browse, &QPushButton::clicked, dialog, [dialog, path] {
        const QString file = QFileDialog::getSaveFileName(
            dialog, QStringLiteral("Файл экспорта"), path->text(), QStringLiteral("Текст (*.txt *.csv)"));
        if (!file.isEmpty())
            path->setText(file);
    });

    auto* row = new QHBoxLayout;
    row->addWidget(all);
    row->addWidget(from);
    row->addWidget(date);
    row->addStretch();
    auto* fileRow = new QHBoxLayout;
    fileRow->addWidget(path);
    fileRow->addWidget(browse);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto* layout = new QVBoxLayout(dialog);
    layout->addLayout(row);
    layout->addWidget(new QLabel(QStringLiteral("Имя файла:")));
    layout->addLayout(fileRow);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    if (dialog->exec() != QDialog::Accepted)
        return;
    if (path->text().isEmpty())
        return;

    settings.setValue(QStringLiteral("ExportPath"), path->text());
    QString error;
    const int count = m_catalog.exportCsv(path->text(), from->isChecked(), date->date(), &error);
    if (count < 0)
        QMessageBox::warning(this, windowTitle(), error);
    else
        QMessageBox::information(this, windowTitle(), QStringLiteral("Экспортировано %1 записей").arg(count));
}

void MainWindow::replaceText()
{
    if (!m_catalog.isOpen())
        return;

    // на стеке: диалог и его виджеты удалятся, когда функция вернётся
    QDialog dialogObject(this);
    QDialog* dialog = &dialogObject;
    dialog->setWindowTitle(QStringLiteral("Поиск и замена"));
    auto* name = new QRadioButton(QStringLiteral("Имя (RU)"), dialog);
    auto* nameBy = new QRadioButton(QStringLiteral("Имя (BY)"), dialog);
    auto* info = new QRadioButton(QStringLiteral("Данные (RU)"), dialog);
    auto* infoBy = new QRadioButton(QStringLiteral("Данные (BY)"), dialog);
    name->setChecked(true);
    auto* find = new QLineEdit(dialog);
    auto* replace = new QLineEdit(dialog);
    auto* exact = new QCheckBox(QStringLiteral("Точное совпадение"), dialog);

    auto* fields = new QHBoxLayout;
    fields->addWidget(name);
    fields->addWidget(nameBy);
    fields->addWidget(info);
    fields->addWidget(infoBy);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto* layout = new QVBoxLayout(dialog);
    layout->addLayout(fields);
    layout->addWidget(new QLabel(QStringLiteral("Искать:")));
    layout->addWidget(find);
    layout->addWidget(new QLabel(QStringLiteral("Заменить на:")));
    layout->addWidget(replace);
    layout->addWidget(exact);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    if (dialog->exec() != QDialog::Accepted || find->text().isEmpty())
        return;

    TextField field = TextField::NameRu;
    if (nameBy->isChecked())
        field = TextField::NameBy;
    else if (info->isChecked())
        field = TextField::PersonalRu;
    else if (infoBy->isChecked())
        field = TextField::PersonalBy;

    const int matches = m_catalog.countMatches(field, exact->isChecked(), find->text());
    if (matches == 0) {
        QMessageBox::information(this, windowTitle(), QStringLiteral("Совпадений не найдено!"));
        return;
    }
    if (QMessageBox::question(this, windowTitle(),
            QStringLiteral("Будет произведено %1 замен.\nХотите продолжить?").arg(matches))
        != QMessageBox::Yes)
        return;

    QString error;
    const int count = m_catalog.replaceText(field, exact->isChecked(), find->text(), replace->text(), &error);
    if (count < 0)
        QMessageBox::critical(this, windowTitle(), error);
    else
        QMessageBox::information(this, windowTitle(), QStringLiteral("%1 записей были обновлены.").arg(count));
}

// разовая чистка старых записей; новые и правленые исправляются при сохранении
void MainWindow::fixLatinBelarusianI()
{
    if (!m_catalog.isOpen())
        return;

    QVector<Person> fixed;
    const Catalog::LetterFix found = m_catalog.findLatinLetters(&fixed, 50);
    if (found.records == 0) {
        QMessageBox::information(this, windowTitle(),
            QStringLiteral("Латинских букв внутри кириллических слов нет."));
        return;
    }

    QStringList fields;
    for (auto it = found.fields.cbegin(); it != found.fields.cend(); ++it)
        fields.push_back(QStringLiteral("    %1: %2").arg(it.key()).arg(it.value()));
    const QFileInfo database(m_catalog.path());
    const QString backupPath = database.dir().filePath(QStringLiteral("%1.before-latin-fix-%2.sqlite")
        .arg(database.completeBaseName(), QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));

    QMessageBox question(QMessageBox::Question, windowTitle(),
        QStringLiteral("Латинские буквы внутри кириллических слов («Міxaйлаўна», «прядильщицa», «Iгар») "
                       "найдены в %1 записях:\n%2\n\n"
                       "Меняются только буквы-двойники внутри кириллических слов и набранные латиницей "
                       "союзы и предлоги («і», «а», «с», «у»); римские цифры («XXI», «I класа») "
                       "и латинские слова не трогаются.\n\n"
                       "Перед исправлением будет сделана копия базы:\n%3\n\nИсправить?")
            .arg(found.records).arg(fields.join(QLatin1Char('\n')), QDir::toNativeSeparators(backupPath)),
        QMessageBox::Yes | QMessageBox::No, this);
    question.setDefaultButton(QMessageBox::No);
    question.setDetailedText(QStringLiteral("Примеры:\n") + found.samples.join(QLatin1Char('\n')));
    if (question.exec() != QMessageBox::Yes)
        return;

    QString error;
    if (!m_catalog.backup(backupPath, &error)) {
        QMessageBox::critical(this, windowTitle(),
            QStringLiteral("Не удалось сделать копию базы, ничего не изменено.\n%1").arg(error));
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool saved = m_catalog.saveMany(fixed, &error);
    QApplication::restoreOverrideCursor();
    if (!saved) {
        QMessageBox::critical(this, windowTitle(),
            QStringLiteral("Исправление не сохранено (база не изменена):\n%1").arg(error));
        return;
    }
    QMessageBox::information(this, windowTitle(),
        QStringLiteral("Исправлено записей: %1.\nКопия базы до исправления:\n%2")
            .arg(fixed.size()).arg(QDir::toNativeSeparators(backupPath)));
}
