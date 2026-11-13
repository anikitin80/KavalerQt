#pragma once

#include "Catalog.h"
#include "Models.h"
#include "Translator.h"

#include <QHash>
#include <functional>
#include <QMainWindow>
#include <QSet>

class QEvent;

class QComboBox;
class QLineEdit;
class QCheckBox;
class QTableView;
class QTreeView;
class QRadioButton;
class QTabWidget;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void openDatabase(const QString& path);
    void chooseDatabase();
    void fillFilters();
    void refreshPeople();
    void refreshStats();
    void refreshDictionary();
    void updateStatus();
    void editCurrent();
    void addPerson(const Person* copyFrom);
    void deleteSelected();
    void showContextMenu(const QPoint& pos);
    void translateSelectedCloud(CloudScope scope);
    void translateSelectedByDictionary();
    void translateSelectedProfession();
    void applySuggestion(int personId, const QString& name);
    void confirmSelectedSuggestions();
    void closeEvent(QCloseEvent* event) override;
    void fixSelectedQuotes();
    void exportCsv();
    void replaceText();
    void fixLatinBelarusianI();
    void printCurrent();
    void layoutPeopleColumns();
    void adjustPeopleColumn(int logical, int newSize);
    bool eventFilter(QObject* watched, QEvent* event) override;
    QString storedDatabasePath() const;
    void rememberDatabasePath(const QString& path);
    bool saveChanged(QVector<Person>& people);
    // change() правит копию каждого выбранного и говорит, изменился ли он;
    // изменённые сохраняются вместе (saved: что сохранилось)
    bool modifySelected(const std::function<bool(Person&)>& change, QVector<Person>* saved = nullptr);
    QVector<Person> selectedPeople() const;
    void capturePeopleState();
    void restorePeopleState();
    void selectPerson(int personId);
    void refreshVisibleTab();

    // то, что пользователь видит в списке людей, по номеру человека: переживает сброс модели
    struct PeopleViewState
    {
        bool valid = false;
        int currentId = 0;
        int currentColumn = 1;
        bool currentVisible = false;
        int nextId = 0;     // соседи текущей строки: нужны, когда она пропадает
        int previousId = 0;
        int topId = 0;      // первая видимая строка
        int scrollValue = 0;
        QSet<int> selectedIds;
        QHash<int, bool> expandedGroups; // номер награды -> раскрыта
    };

    Catalog m_catalog;
    PersonModel* m_people = nullptr;
    StatsModel* m_stats = nullptr;
    DictionaryModel* m_dictionary = nullptr;

    QComboBox* m_award = nullptr;
    QComboBox* m_year = nullptr;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_quick = nullptr;
    QCheckBox* m_needTranslation = nullptr;
    QCheckBox* m_translationErrors = nullptr; // только розовые строки
    QCheckBox* m_nameVariants = nullptr;      // только голубые строки
    QAction* m_groupAction = nullptr; // «Вид → Группировать по награде»
    QTreeView* m_peopleView = nullptr;

    QCheckBox* m_statsByMonth = nullptr;
    QRadioButton* m_statsByAward = nullptr;
    QRadioButton* m_statsByYear = nullptr;
    QTreeView* m_statsView = nullptr;
    QLineEdit* m_dictFilter = nullptr;
    QCheckBox* m_dictVariantsOnly = nullptr;
    QTreeView* m_dictView = nullptr;
    QTabWidget* m_tabs = nullptr;
    QLabel* m_count = nullptr;
    bool m_filling = false;
    bool m_adjustingColumns = false;
    bool m_openingDatabase = false;
    bool m_statsStale = true;
    bool m_dictionaryStale = true;
    PeopleViewState m_peopleState;
};
