#pragma once

#include "Catalog.h"

#include <QDialog>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QPlainTextEdit;
class QCheckBox;
class QSpinBox;

class EditDialog : public QDialog
{
    Q_OBJECT
public:
    EditDialog(Catalog* catalog, const Person& person, QWidget* parent = nullptr);

    Person person() const { return m_person; }

private:
    void accept() override;
    void done(int result) override;

    Catalog* m_catalog;
    Person m_person;
    QComboBox* m_award;
    QSpinBox* m_ukaseNo;
    QDateEdit* m_date;
    QCheckBox* m_posthumous;
    QLineEdit* m_name;
    QLineEdit* m_nameBy;
    QPlainTextEdit* m_personal;
    QPlainTextEdit* m_personalBy;
    QLineEdit* m_rank;
    QLineEdit* m_rankBy;
    QPlainTextEdit* m_awardedFor;
};
