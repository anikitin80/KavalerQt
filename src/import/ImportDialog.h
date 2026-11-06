#pragma once

#include "Catalog.h"
#include "DocumentParser.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QTableWidget;
class QTreeWidget;

class ImportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImportDialog(Catalog* catalog, QWidget* parent = nullptr);

private:
    void choosePath();
    void startParse();
    void showPersons();
    void importRows(bool all);
    void offerNewWords();
    bool chooseDictionaries();
    void fillAwards();
    Person personFromInfo(const PersonInfo& info);
    void markKnown(); // see ImportMatch.h

    Catalog* m_catalog;
    CDocumentParser m_parser;
    QString m_folder;

    QRadioButton* m_fileMode = nullptr;
    QLineEdit* m_path = nullptr;
    QCheckBox* m_rename = nullptr;
    QLabel* m_progress = nullptr;
    QTreeWidget* m_awards = nullptr;
    QTableWidget* m_persons = nullptr;
    QLabel* m_personCount = nullptr;
    QCheckBox* m_useGroup = nullptr;
    QLineEdit* m_group = nullptr;
    bool m_filling = false;
    bool m_hasSelection = false;
    int m_awardId = 0;
    QString m_fileName;
};
