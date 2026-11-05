#include "EditDialog.h"
#include "Translator.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

EditDialog::EditDialog(Catalog* catalog, const Person& person, QWidget* parent)
    : QDialog(parent)
    , m_catalog(catalog)
    , m_person(person)
{
    setWindowTitle(QStringLiteral("Данные о награждении"));
    resize(920, 520);

    m_award = new QComboBox;
    QVector<Award> awards = catalog->awards();
    std::sort(awards.begin(), awards.end(), [](const Award& a, const Award& b) {
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });
    for (const Award& award : awards)
        m_award->addItem(award.name, award.id);
    const int awardIndex = m_award->findData(person.awardId);
    if (awardIndex >= 0)
        m_award->setCurrentIndex(awardIndex);

    m_ukaseNo = new QLineEdit(QString::number(person.ukaseNo));
    m_date = new QDateEdit(person.ukaseDate.isValid() ? person.ukaseDate : QDate::currentDate());
    m_date->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    m_date->setCalendarPopup(true);
    m_posthumous = new QCheckBox(QStringLiteral("посмертно"));
    m_posthumous->setChecked(person.posthumous != 0);

    m_name = new QLineEdit(person.name);
    m_nameBy = new QLineEdit(person.nameBy);
    m_personal = new QPlainTextEdit(person.personal);
    m_personalBy = new QPlainTextEdit(person.personalBy);
    m_rank = new QLineEdit(person.rank);
    m_rankBy = new QLineEdit(person.rankBy);
    m_awardedFor = new QPlainTextEdit(person.awardedFor);
    m_personal->setFixedHeight(90);
    m_personalBy->setFixedHeight(90);
    m_awardedFor->setFixedHeight(70);

    auto* ukaseRow = new QHBoxLayout;
    ukaseRow->addWidget(new QLabel(QStringLiteral("№")));
    ukaseRow->addWidget(m_ukaseNo);
    ukaseRow->addWidget(new QLabel(QStringLiteral("Дата")));
    ukaseRow->addWidget(m_date);
    ukaseRow->addWidget(m_posthumous);
    ukaseRow->addStretch();
    m_ukaseNo->setMaximumWidth(80);

    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel(QStringLiteral("Награда")), 0, 0);
    grid->addWidget(m_award, 0, 1);
    grid->addLayout(ukaseRow, 1, 1);
    grid->addWidget(new QLabel(QStringLiteral("Указ")), 1, 0);
    grid->addWidget(new QLabel(QStringLiteral("ФИО")), 2, 0);
    grid->addWidget(m_name, 2, 1);
    grid->addWidget(new QLabel(QStringLiteral("ФИО (белор.)")), 2, 2);
    grid->addWidget(m_nameBy, 2, 3);
    grid->addWidget(new QLabel(QStringLiteral("Личные данные")), 3, 0);
    grid->addWidget(m_personal, 3, 1);
    grid->addWidget(new QLabel(QStringLiteral("Личные данные (белор.)")), 3, 2);
    grid->addWidget(m_personalBy, 3, 3);
    grid->addWidget(new QLabel(QStringLiteral("Звание")), 4, 0);
    grid->addWidget(m_rank, 4, 1);
    grid->addWidget(new QLabel(QStringLiteral("Звание (бел.)")), 4, 2);
    grid->addWidget(m_rankBy, 4, 3);
    grid->addWidget(new QLabel(QStringLiteral("Награжден за")), 5, 0);
    grid->addWidget(m_awardedFor, 5, 1, 1, 3);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    auto* translate = new QPushButton(QStringLiteral("Перевести"));
    connect(translate, &QPushButton::clicked, this, [this] {
        Person person = m_person;
        person.name = m_name->text().trimmed();
        person.nameBy = m_nameBy->text().trimmed();
        person.personal = m_personal->toPlainText().trimmed();
        person.personalBy = m_personalBy->toPlainText().trimmed();
        person.rank = m_rank->text().trimmed();
        person.rankBy = m_rankBy->text().trimmed();
        QString error;
        if (!translateWithCloud(person, CloudScope::All, true, &error)) {
            QMessageBox::critical(this, windowTitle(), error);
            return;
        }
        m_catalog->fixQuotes(person);
        m_name->setText(person.name);
        m_personal->setPlainText(person.personal);
        m_rank->setText(person.rank);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Сохранить"));
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("Закрыть"));
    connect(buttons, &QDialogButtonBox::accepted, this, &EditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(translate);
    buttonRow->addStretch();
    buttonRow->addWidget(buttons);
    layout->addLayout(grid);
    layout->addLayout(buttonRow);
}

void EditDialog::accept()
{
    if (m_award->currentIndex() < 0) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("Не выбрана награда."));
        return;
    }
    if (m_name->text().trimmed().isEmpty() && m_nameBy->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("Не заполнено поле 'ФИО'."));
        return;
    }
    if (!m_date->date().isValid()) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("Неверная дата."));
        return;
    }

    m_person.awardId = m_award->currentData().toInt();
    m_person.ukaseNo = m_ukaseNo->text().toInt();
    m_person.ukaseDate = m_date->date();
    m_person.posthumous = m_posthumous->isChecked() ? 1 : 0;
    m_person.name = m_name->text().trimmed();
    m_person.nameBy = m_nameBy->text().trimmed();
    m_person.personal = m_personal->toPlainText().trimmed();
    m_person.personalBy = m_personalBy->toPlainText().trimmed();
    m_person.rank = m_rank->text().trimmed();
    m_person.rankBy = m_rankBy->text().trimmed();
    m_person.awardedFor = m_awardedFor->toPlainText().trimmed();

    QString error;
    if (!m_catalog->save(m_person, &error)) {
        QMessageBox::critical(this, windowTitle(), error);
        return;
    }
    QDialog::accept();
}
