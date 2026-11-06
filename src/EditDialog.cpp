#include "EditDialog.h"
#include "Translator.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSpinBox>
#include <algorithm>

namespace {

const char kGeometryKey[] = "EditDialog/Geometry";

// Two language panels side by side when there is enough room, one above
// the other in a narrow window.
class LanguagePanels : public QWidget
{
public:
    LanguagePanels(QWidget* first, QWidget* second, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_first(first)
        , m_second(second)
    {
        m_layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->addWidget(first, 1);
        m_layout->addWidget(second, 1);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        const auto direction = event->size().width() >= threshold()
            ? QBoxLayout::LeftToRight
            : QBoxLayout::TopToBottom;
        if (m_layout->direction() != direction)
            m_layout->setDirection(direction);
    }

private:
    // must stay above the side-by-side minimum width, otherwise the window
    // could not be narrowed enough to reach the switch point
    int threshold() const
    {
        const int sideBySide = m_first->minimumSizeHint().width() + m_second->minimumSizeHint().width()
            + m_layout->spacing();
        return std::max(760, sideBySide + 40);
    }

    QWidget* m_first;
    QWidget* m_second;
    QBoxLayout* m_layout;
};

QLabel* rowLabel(const QString& text, bool multiline)
{
    auto* label = new QLabel(text);
    if (multiline) {
        // line up with the first text line of the multi-line editor
        label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        label->setContentsMargins(0, 4, 0, 0);
    } else {
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return label;
}

QPlainTextEdit* textBox(const QString& text, int minLines)
{
    auto* edit = new QPlainTextEdit(text);
    edit->setTabChangesFocus(true);
    const int lineHeight = edit->fontMetrics().lineSpacing();
    edit->setMinimumHeight(lineHeight * minLines + 2 * int(edit->document()->documentMargin()) + 6);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    return edit;
}

// ФИО / Личные данные / Звание for one language
QGroupBox* languagePanel(const QString& title, QLineEdit* name, QPlainTextEdit* personal, QLineEdit* rank)
{
    auto* box = new QGroupBox(title);
    auto* grid = new QGridLayout(box);
    grid->addWidget(rowLabel(QStringLiteral("ФИО:"), false), 0, 0);
    grid->addWidget(name, 0, 1);
    grid->addWidget(rowLabel(QStringLiteral("Личные данные:"), true), 1, 0);
    grid->addWidget(personal, 1, 1);
    grid->addWidget(rowLabel(QStringLiteral("Звание:"), false), 2, 0);
    grid->addWidget(rank, 2, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(1, 1);
    return box;
}

} // namespace

EditDialog::EditDialog(Catalog* catalog, const Person& person, QWidget* parent)
    : QDialog(parent)
    , m_catalog(catalog)
    , m_person(person)
{
    setWindowTitle(QStringLiteral("Данные о награждении"));
    setSizeGripEnabled(true);

    m_award = new QComboBox;
    m_award->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_award->setMinimumContentsLength(20);
    m_award->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QVector<Award> awards = catalog->awards();
    std::sort(awards.begin(), awards.end(), [](const Award& a, const Award& b) {
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });
    for (const Award& award : awards)
        m_award->addItem(award.name, award.id);
    const int awardIndex = m_award->findData(person.awardId);
    if (awardIndex >= 0)
        m_award->setCurrentIndex(awardIndex);

    m_ukaseNo = new QSpinBox;
    m_ukaseNo->setRange(0, 999999);
    m_ukaseNo->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_ukaseNo->setValue(person.ukaseNo);
    m_date = new QDateEdit(person.ukaseDate.isValid() ? person.ukaseDate : QDate::currentDate());
    m_date->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    m_date->setCalendarPopup(true);
    m_posthumous = new QCheckBox(QStringLiteral("посмертно"));
    m_posthumous->setChecked(person.posthumous != 0);

    m_name = new QLineEdit(person.name);
    m_nameBy = new QLineEdit(person.nameBy);
    m_personal = textBox(person.personal, 3);
    m_personalBy = textBox(person.personalBy, 3);
    m_rank = new QLineEdit(person.rank);
    m_rankBy = new QLineEdit(person.rankBy);
    m_awardedFor = textBox(person.awardedFor, 2);

    auto* ukaseRow = new QHBoxLayout;
    ukaseRow->addWidget(new QLabel(QStringLiteral("№")));
    ukaseRow->addWidget(m_ukaseNo);
    ukaseRow->addSpacing(12);
    ukaseRow->addWidget(new QLabel(QStringLiteral("от")));
    ukaseRow->addWidget(m_date);
    ukaseRow->addSpacing(24);
    ukaseRow->addWidget(m_posthumous);
    ukaseRow->addStretch();

    auto* panels = new LanguagePanels(
        languagePanel(QStringLiteral("Русский"), m_name, m_personal, m_rank),
        languagePanel(QStringLiteral("Белорусский"), m_nameBy, m_personalBy, m_rankBy));

    auto* form = new QGridLayout;
    form->setVerticalSpacing(8);
    form->addWidget(rowLabel(QStringLiteral("Награда:"), false), 0, 0);
    form->addWidget(m_award, 0, 1);
    form->addWidget(rowLabel(QStringLiteral("Указ:"), false), 1, 0);
    form->addLayout(ukaseRow, 1, 1);
    form->addWidget(panels, 2, 0, 1, 2);
    form->addWidget(rowLabel(QStringLiteral("Награжден за:"), true), 3, 0);
    form->addWidget(m_awardedFor, 3, 1);
    form->setColumnStretch(1, 1);
    form->setRowStretch(2, 3);
    form->setRowStretch(3, 1);

    auto* translate = new QPushButton(QStringLiteral("Перевести"));
    translate->setToolTip(QStringLiteral("Перевести белорусские поля через Яндекс"));
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

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(translate);
    buttonRow->addStretch();
    buttonRow->addWidget(buttons);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form, 1);
    layout->addSpacing(4);
    layout->addLayout(buttonRow);

    // Russian fields first, then Belarusian, whatever the panel arrangement
    const QWidget* order[] = {m_award, m_ukaseNo, m_date, m_posthumous, m_name, m_personal, m_rank,
                              m_nameBy, m_personalBy, m_rankBy, m_awardedFor, translate, buttons};
    for (size_t i = 1; i < std::size(order); ++i)
        setTabOrder(const_cast<QWidget*>(order[i - 1]), const_cast<QWidget*>(order[i]));

    if (!restoreGeometry(QSettings().value(QLatin1String(kGeometryKey)).toByteArray()))
        resize(980, 560);
}

void EditDialog::done(int result)
{
    QSettings().setValue(QLatin1String(kGeometryKey), saveGeometry());
    QDialog::done(result);
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
    m_person.ukaseNo = m_ukaseNo->value();
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
