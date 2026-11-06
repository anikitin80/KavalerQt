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
    // change() edits a copy of each selected person and says whether it changed;
    // the changed ones are saved together (saved: what was saved)
    bool modifySelected(const std::function<bool(Person&)>& change, QVector<Person>* saved = nullptr);
    QVector<Person> selectedPeople() const;
    void capturePeopleState();
    void restorePeopleState();
    void selectPerson(int personId);
    void refreshVisibleTab();

    // what the user sees in the people list, by person id: survives model resets
    struct PeopleViewState
    {
        bool valid = false;
        int currentId = 0;
        int currentColumn = 1;
        bool currentVisible = false;
        int nextId = 0;     // neighbours of the current row: used when it disappears
        int previousId = 0;
        int topId = 0;      // first visible row
        int scrollValue = 0;
        QSet<int> selectedIds;
        QHash<int, bool> expandedGroups; // award id -> expanded
    };

    Catalog m_catalog;
    PersonModel* m_people = nullptr;
    StatsModel* m_stats = nullptr;
    DictionaryModel* m_dictionary = nullptr;

    QComboBox* m_award = nullptr;
    QComboBox* m_year = nullptr;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_quick = nullptr;
    QCheckBox* m_quotes = nullptr;
    QCheckBox* m_needTranslation = nullptr;
    QCheckBox* m_group = nullptr;
    QTreeView* m_peopleView = nullptr;

    QCheckBox* m_statsByMonth = nullptr;
    QTableView* m_statsView = nullptr;
    QLineEdit* m_dictFilter = nullptr;
    QTableView* m_dictView = nullptr;
    QTabWidget* m_tabs = nullptr;
    QLabel* m_count = nullptr;
    bool m_filling = false;
    bool m_adjustingColumns = false;
    bool m_openingDatabase = false;
    bool m_statsStale = true;
    bool m_dictionaryStale = true;
    PeopleViewState m_peopleState;
};
