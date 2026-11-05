#pragma once

#include "Catalog.h"
#include "Models.h"
#include "Translator.h"

#include <QMainWindow>

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
    void printCurrent();
    void layoutPeopleColumns();
    void adjustPeopleColumn(int logical, int newSize);
    bool eventFilter(QObject* watched, QEvent* event) override;
    QString storedDatabasePath() const;
    void rememberDatabasePath(const QString& path);

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
};
