#include "ImportDialog.h"

#include "DocxText.h"
#include "PdfText.h"

#include <QAction>
#include <QCheckBox>
#include <QColor>
#include <QDesktopServices>
#include <QFont>
#include <QMenu>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTableWidget>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QApplication>

namespace {

QString fromWide(const std::wstring& text)
{
    return QString::fromStdWString(text);
}

std::wstring toWide(const QString& text)
{
    return text.toStdWString();
}

void replaceStoredFile(CDocumentParser& parser, const std::wstring& from, const std::wstring& to)
{
    for (PersonInfo& person : parser.m_Persons) {
        if (person.FileName == from)
            person.FileName = to;
    }
    for (GroupInfo& group : parser.m_GroupsInfo) {
        if (group.FileName == from)
            group.FileName = to;
    }
    for (AwardInfo& award : parser.m_AwardsStat) {
        auto found = award.CountByFiles.find(from);
        if (found == award.CountByFiles.end())
            continue;
        award.CountByFiles[to] = found->second;
        award.CountByFiles.erase(found);
    }
}

class NewWordsDialog : public QDialog
{
public:
    NewWordsDialog(std::vector<NewDictItem>& items, QWidget* parent)
        : QDialog(parent)
        , m_items(items)
    {
        setWindowTitle(QStringLiteral("Новые слова словаря"));
        resize(720, 420);
        m_table = new QTableWidget(int(items.size()), 4, this);
        m_table->setHorizontalHeaderLabels({QStringLiteral("Добавить"), QStringLiteral("Как в указе"),
            QStringLiteral("Словарная форма"), QStringLiteral("Язык")});
        for (int row = 0; row < int(items.size()); ++row) {
            auto* check = new QTableWidgetItem;
            check->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            check->setCheckState(items[size_t(row)].Checked ? Qt::Checked : Qt::Unchecked);
            m_table->setItem(row, 0, check);
            m_table->setItem(row, 1, new QTableWidgetItem(fromWide(items[size_t(row)].Original)));
            m_table->item(row, 1)->setFlags(Qt::ItemIsEnabled);
            m_table->setItem(row, 2, new QTableWidgetItem(fromWide(items[size_t(row)].Word)));
            m_table->setItem(row, 3, new QTableWidgetItem(items[size_t(row)].Language == LangBY
                ? QStringLiteral("белор.") : QStringLiteral("рус.")));
            m_table->item(row, 3)->setFlags(Qt::ItemIsEnabled);
        }
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setColumnWidth(1, 220);
        m_table->setColumnWidth(2, 220);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            for (int row = 0; row < m_table->rowCount(); ++row) {
                m_items[size_t(row)].Checked = m_table->item(row, 0)->checkState() == Qt::Checked;
                m_items[size_t(row)].Word = toWide(m_table->item(row, 2)->text());
            }
            QDialog::accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(m_table);
        layout->addWidget(buttons);
    }

private:
    std::vector<NewDictItem>& m_items;
    QTableWidget* m_table = nullptr;
};

} // namespace

ImportDialog::ImportDialog(Catalog* catalog, QWidget* parent)
    : QDialog(parent)
    , m_catalog(catalog)
{
    setWindowTitle(QStringLiteral("Импорт указов"));
    resize(1100, 720);

    std::map<long, DecreeAward> awards;
    for (const Award& award : catalog->awards()) {
        DecreeAward item;
        item.ID = award.id;
        item.Name = toWide(award.name);
        item.NameBY = toWide(award.nameBy);
        item.Type = award.type;
        item.Priority = award.priority;
        awards[item.ID] = item;
    }
    m_parser.Initialize(awards);

    m_fileMode = new QRadioButton(QStringLiteral("Файл"));
    auto* folderMode = new QRadioButton(QStringLiteral("Папка"));
    m_fileMode->setChecked(true);
    m_path = new QLineEdit;
    auto* browse = new QPushButton(QStringLiteral("Обзор..."));
    m_rename = new QCheckBox(QStringLiteral("Переименовать файл по номеру указа"));
    auto* start = new QPushButton(QStringLiteral("Разобрать"));
    auto* dictionaries = new QPushButton(QStringLiteral("Словари..."));
    dictionaries->setToolTip(QStringLiteral("Файлы словарей профессий"));
    m_progress = new QLabel;

    m_awards = new QTreeWidget;
    m_awards->setHeaderLabels({QStringLiteral("Награда"), QStringLiteral("Количество")});
    m_awards->setColumnWidth(0, 520);
    m_persons = new QTableWidget;
    m_persons->setColumnCount(2);
    m_persons->setHorizontalHeaderLabels({QStringLiteral("ФИО"), QStringLiteral("Должность")});
    m_persons->horizontalHeader()->setStretchLastSection(true);
    m_persons->setColumnWidth(0, 280);
    m_persons->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_persons->verticalHeader()->setVisible(false);
    m_personCount = new QLabel(QStringLiteral("Награжденные (0):"));
    m_useGroup = new QCheckBox(QStringLiteral("Добавлять сведения о группе"));
    m_useGroup->setEnabled(false);
    m_group = new QLineEdit;
    m_group->setEnabled(false);

    auto* importSelected = new QPushButton(QStringLiteral("Импорт выбранных"));
    auto* importAll = new QPushButton(QStringLiteral("Импорт всех"));
    auto* close = new QPushButton(QStringLiteral("Закрыть"));

    auto* pathRow = new QHBoxLayout;
    pathRow->addWidget(m_fileMode);
    pathRow->addWidget(folderMode);
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browse);
    pathRow->addWidget(m_rename);
    pathRow->addWidget(start);
    pathRow->addWidget(dictionaries);

    auto* groupRow = new QHBoxLayout;
    groupRow->addWidget(m_useGroup);
    groupRow->addWidget(m_group, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_personCount);
    buttons->addStretch();
    buttons->addWidget(importSelected);
    buttons->addWidget(importAll);
    buttons->addWidget(close);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(pathRow);
    layout->addWidget(m_progress);
    layout->addWidget(m_awards, 2);
    layout->addWidget(m_persons, 3);
    layout->addLayout(groupRow);
    layout->addLayout(buttons);

    connect(browse, &QPushButton::clicked, this, &ImportDialog::choosePath);
    connect(start, &QPushButton::clicked, this, &ImportDialog::startParse);
    connect(dictionaries, &QPushButton::clicked, this, [this] { chooseDictionaries(); });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(importSelected, &QPushButton::clicked, this, [this] { importRows(false); });
    connect(importAll, &QPushButton::clicked, this, [this] { importRows(true); });
    connect(m_awards, &QTreeWidget::itemSelectionChanged, this, &ImportDialog::showPersons);
    connect(m_awards, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        const QString fileName = item->data(0, Qt::UserRole + 1).toString();
        if (!fileName.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(m_folder).filePath(fileName)));
    });
    connect(m_useGroup, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_filling || m_fileName.isEmpty())
            return;
        m_parser.CheckGroupInfo(toWide(m_fileName).c_str(), checked);
        showPersons();
    });
    connect(m_group, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (m_filling || m_fileName.isEmpty())
            return;
        m_parser.UpdateGroupInfo(toWide(m_fileName).c_str(), toWide(text).c_str());
        showPersons();
    });
    connect(m_persons, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (m_filling || item->column() != 0)
            return;
        const int index = item->data(Qt::UserRole).toInt();
        if (index < 0 || index >= int(m_parser.m_Persons.size()))
            return;
        m_parser.m_Persons[size_t(index)].Name = toWide(item->text());
        const QFont font = alreadyKnown(personFromInfo(m_parser.m_Persons[size_t(index)]), {})
            ? QFont(item->font().family(), -1, QFont::Bold) : item->font();
        QFont updated = item->font();
        updated.setBold(alreadyKnown(personFromInfo(m_parser.m_Persons[size_t(index)]), {}));
        item->setFont(updated);
        Q_UNUSED(font);
    });
    m_persons->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_persons, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const int row = m_persons->rowAt(pos.y());
        if (row < 0)
            return;
        QMenu menu(this);
        QAction* split = menu.addAction(QStringLiteral("Разделить награждения"));
        if (menu.exec(m_persons->viewport()->mapToGlobal(pos)) != split)
            return;
        const int index = m_persons->item(row, 0)->data(Qt::UserRole).toInt();
        if (index < 0 || index >= int(m_parser.m_Persons.size()))
            return;
        if (!m_parser.SpltPersonInfo(m_parser.m_Persons[size_t(index)]))
            return;
        const std::wstring fileName = toWide(m_fileName);
        for (AwardInfo& award : m_parser.m_AwardsStat) {
            auto found = award.CountByFiles.find(fileName);
            if (found == award.CountByFiles.end())
                continue;
            found->second++;
            award.TotalCount++;
        }
        fillAwards();
        showPersons();
    });
}

void ImportDialog::choosePath()
{
    const QString selected = m_fileMode->isChecked()
        ? QFileDialog::getOpenFileName(this, QStringLiteral("Указ"), m_path->text(),
            QStringLiteral("Указы (*.docx *.pdf);;Word (*.docx);;PDF (*.pdf)"))
        : QFileDialog::getExistingDirectory(this, QStringLiteral("Папка с указами"), m_path->text());
    if (!selected.isEmpty())
        m_path->setText(QDir::toNativeSeparators(selected));
}

void ImportDialog::startParse()
{
    const QString selected = m_path->text().trimmed();
    if (selected.isEmpty())
        return;

    QStringList files;
    if (m_fileMode->isChecked()) {
        m_folder = QFileInfo(selected).absolutePath();
        files.push_back(selected);
    } else {
        m_folder = selected;
        const QFileInfoList found = QDir(selected).entryInfoList(QDir::Files);
        for (const QFileInfo& info : found) {
            if (info.fileName().startsWith(QStringLiteral("~$")))
                continue;
            const QString suffix = info.suffix().toLower();
            if (suffix == QLatin1String("docx") || suffix == QLatin1String("pdf"))
                files.push_back(info.absoluteFilePath());
        }
        if (files.isEmpty()) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("Файлы не обнаружены"));
            return;
        }
    }

    m_parser.ResetData();
    int parsed = 0;
    for (const QString& filePath : files) {
        const QFileInfo info(filePath);
        m_progress->setText(info.fileName());
        QApplication::processEvents();

        QString error;
        QStringList paragraphs;
        const bool pdf = info.suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) == 0;
        if (pdf)
            paragraphs = readPdfLines(filePath, &error);
        else if (info.suffix().compare(QLatin1String("docx"), Qt::CaseInsensitive) == 0)
            paragraphs = readDocxParagraphs(filePath, &error);
        else
            continue;

        std::vector<std::wstring> lines;
        lines.reserve(size_t(paragraphs.size()));
        for (const QString& paragraph : paragraphs)
            lines.push_back(toWide(paragraph));

        QString storedName = info.fileName();
        const bool foundAwards = !lines.empty() && m_parser.ProcessFile(pdf, toWide(storedName), lines);
        if (!foundAwards) {
            QMessageBox::warning(this, windowTitle(),
                error.isEmpty() ? QStringLiteral("В файле %1 награждений не найдено.").arg(info.fileName()) : error);
            continue;
        }
        ++parsed;

        if (!pdf && m_rename->isChecked() && m_parser.m_UkazInfo.Number && m_parser.m_UkazInfo.Date.isValid()
            && !storedName.startsWith(QLatin1Char('P'))) {
            QString renamed = QDir(m_folder).filePath(QStringLiteral("P%1_%2 %3")
                .arg(m_parser.m_UkazInfo.Date.toString(QStringLiteral("yyyyMMdd")))
                .arg(m_parser.m_UkazInfo.Number, 3, 10, QLatin1Char('0'))
                .arg(storedName));
            if (renamed.size() > 245)
                renamed = renamed.left(245) + QStringLiteral(".docx");
            if (QFile::rename(filePath, renamed)) {
                replaceStoredFile(m_parser, toWide(storedName), toWide(QFileInfo(renamed).fileName()));
            }
        }
    }

    m_progress->setText(QStringLiteral("Обработано %1 файлов").arg(parsed));
    offerNewWords();
    fillAwards();
    showPersons();
}

void ImportDialog::offerNewWords()
{
    std::vector<NewDictItem> items = m_parser.m_ProfessionConverter.newWords();
    if (items.empty())
        return;
    NewWordsDialog dialog(items, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    CProfessionConverter& converter = m_parser.m_ProfessionConverter;
    bool needRu = false;
    bool needBy = false;
    for (const NewDictItem& item : items) {
        if (!item.Checked)
            continue;
        needRu = needRu || item.Language == LangRU;
        needBy = needBy || item.Language == LangBY;
    }
    const auto missingPath = [&] {
        return (needRu && converter.DictionaryPath(LangRU).empty())
            || (needBy && converter.DictionaryPath(LangBY).empty());
    };
    if (missingPath()) {
        QMessageBox::information(this, windowTitle(),
            QStringLiteral("Не заданы файлы словарей профессий. Укажите их, чтобы сохранить новые слова."));
        chooseDictionaries();
    }

    // words are applied to this session even if the dictionary can't be saved
    if (!converter.UpdateDictionary(items)) {
        QMessageBox::warning(this, windowTitle(), missingPath()
            ? QStringLiteral("Файлы словарей не заданы: новые слова будут использованы только до закрытия окна.")
            : QStringLiteral("Не удалось сохранить словарь профессий."));
    }
    converter.clearNewWords();
    for (PersonInfo& person : m_parser.m_Persons) {
        if (person.Flags & PersonInfo::NF_PROFESSION)
            continue;
        const NounCase nounCase = m_parser.GetNounCaseByAwardId(person.AwardId);
        if (m_parser.m_ProfessionConverter.ConvertProfession(person.Description,
                person.Ukaz.RussianLang ? LangRU : LangBY, nounCase))
            person.Flags |= PersonInfo::NF_PROFESSION;
    }
}

bool ImportDialog::chooseDictionaries()
{
    CProfessionConverter& converter = m_parser.m_ProfessionConverter;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Словари профессий"));
    dialog.resize(640, dialog.sizeHint().height());

    auto* grid = new QGridLayout;
    const auto addRow = [&](int row, const QString& label, LangType lang) {
        auto* edit = new QLineEdit(QDir::toNativeSeparators(fromWide(converter.DictionaryPath(lang))));
        auto* browse = new QPushButton(QStringLiteral("..."));
        connect(browse, &QPushButton::clicked, &dialog, [&dialog, edit, label] {
            // an existing file is loaded, a new one is created on the first save
            const QString file = QFileDialog::getSaveFileName(&dialog, label, edit->text(),
                QStringLiteral("Словарь (*.txt *.csv);;Все файлы (*.*)"), nullptr,
                QFileDialog::DontConfirmOverwrite);
            if (!file.isEmpty())
                edit->setText(QDir::toNativeSeparators(file));
        });
        grid->addWidget(new QLabel(label), row, 0);
        grid->addWidget(edit, row, 1);
        grid->addWidget(browse, row, 2);
        return edit;
    };
    QLineEdit* russian = addRow(0, QStringLiteral("Русский:"), LangRU);
    QLineEdit* belarusian = addRow(1, QStringLiteral("Белорусский:"), LangBY);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addLayout(grid);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    QSettings settings;
    QStringList failed;
    const auto apply = [&](QLineEdit* edit, LangType lang, const QString& key) {
        const QString path = QDir::fromNativeSeparators(edit->text().trimmed());
        if (fromWide(converter.DictionaryPath(lang)) == path)
            return;
        settings.setValue(key, path);
        if (!converter.SetDictionaryPath(toWide(path), lang) && !path.isEmpty())
            failed.push_back(QDir::toNativeSeparators(path));
    };
    apply(russian, LangRU, QStringLiteral("DictionaryRU"));
    apply(belarusian, LangBY, QStringLiteral("DictionaryBY"));
    if (!failed.isEmpty())
        QMessageBox::warning(this, windowTitle(),
            QStringLiteral("Не удалось прочитать словарь:\n%1").arg(failed.join(QLatin1Char('\n'))));
    return true;
}

void ImportDialog::fillAwards()
{
    const int selectedAward = m_hasSelection ? m_awardId : -1;
    const QString selectedFile = m_fileName;
    m_filling = true;
    m_awards->clear();
    QTreeWidgetItem* reselect = nullptr;
    for (const AwardInfo& info : m_parser.m_AwardsStat) {
        QString title = fromWide(info.CustomName);
        if (info.Id != 0) {
            if (const Award* award = m_catalog->award(info.Id))
                title = award->name;
        }
        auto* parent = new QTreeWidgetItem(m_awards, {title, QString::number(info.TotalCount)});
        parent->setData(0, Qt::UserRole, info.Id);
        parent->setData(0, Qt::UserRole + 1, QString());
        if (info.NeedReview || info.Id == 0)
            parent->setForeground(0, QColor(200, 0, 0));
        if (m_hasSelection && selectedFile.isEmpty() && info.Id == selectedAward)
            reselect = parent;
        for (const auto& file : info.CountByFiles) {
            const int count = int(file.second & ~kFlagNeedReview);
            auto* child = new QTreeWidgetItem(parent, {fromWide(file.first), QString::number(count)});
            child->setData(0, Qt::UserRole, info.Id);
            child->setData(0, Qt::UserRole + 1, fromWide(file.first));
            if (file.second & kFlagNeedReview)
                child->setForeground(1, QColor(200, 0, 0));
            if (m_hasSelection && fromWide(file.first) == selectedFile && info.Id == selectedAward)
                reselect = child;
        }
        parent->setExpanded(true);
    }
    m_filling = false;
    if (reselect)
        m_awards->setCurrentItem(reselect);
}

void ImportDialog::showPersons()
{
    m_awardId = 0;
    m_fileName.clear();
    m_filling = true;
    m_useGroup->setEnabled(false);
    m_useGroup->setChecked(false);
    m_group->setEnabled(false);
    m_group->clear();

    if (QTreeWidgetItem* item = m_awards->currentItem()) {
        m_hasSelection = true;
        m_awardId = item->data(0, Qt::UserRole).toInt();
        m_fileName = item->data(0, Qt::UserRole + 1).toString();
        if (!m_fileName.isEmpty()) {
            GroupInfo group;
            std::wstring fileName = toWide(m_fileName);
            if (m_parser.GetGroupInfo(fileName, group)) {
                m_useGroup->setEnabled(true);
                m_group->setEnabled(true);
                m_useGroup->setChecked(group.Checked);
                m_group->setText(fromWide(group.Info));
            }
        }
    }

    m_persons->setRowCount(0);
    int shown = 0;
    for (int index = 0; index < int(m_parser.m_Persons.size()); ++index) {
        const PersonInfo& person = m_parser.m_Persons[size_t(index)];
        if (m_awardId && m_awardId != person.AwardId)
            continue;
        if (!m_fileName.isEmpty() && m_fileName != fromWide(person.FileName))
            continue;
        const int row = m_persons->rowCount();
        m_persons->insertRow(row);
        auto* name = new QTableWidgetItem(fromWide(person.Name));
        name->setData(Qt::UserRole, index);
        if ((person.Flags & PersonInfo::NF_NAME) == 0)
            name->setForeground(QColor(200, 0, 0));
        QFont font = name->font();
        font.setBold(alreadyKnown(personFromInfo(person), {}));
        name->setFont(font);
        m_persons->setItem(row, 0, name);

        auto* job = new QTableWidgetItem(fromWide(m_parser.GetPersonDescription(size_t(index))));
        job->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        const bool rankMissing = person.MilitaryRank.empty() && CDocumentParser::IsMilitaryAward(person.AwardId);
        if ((person.Flags & PersonInfo::NF_PROFESSION) == 0 || rankMissing)
            job->setForeground(QColor(200, 0, 0));
        m_persons->setItem(row, 1, job);
        ++shown;
    }
    m_personCount->setText(QStringLiteral("Награжденные (%1):").arg(shown));
    m_filling = false;
}

Person ImportDialog::personFromInfo(const PersonInfo& info)
{
    Person person;
    person.awardId = info.AwardId;
    person.language = info.Ukaz.RussianLang ? kLangRussian : kLangBelarusian;
    if (info.Ukaz.RussianLang) {
        person.name = fromWide(info.Name);
        person.personal = fromWide(m_parser.GetPersonDescription(const_cast<PersonInfo&>(info), false));
        person.rank = fromWide(info.MilitaryRank);
    } else {
        person.nameBy = fromWide(info.Name);
        person.personalBy = fromWide(m_parser.GetPersonDescription(const_cast<PersonInfo&>(info), false));
        person.rankBy = fromWide(info.MilitaryRank);
    }
    person.ukaseDate = info.Ukaz.Date;
    person.ukaseNo = info.Ukaz.Number;
    person.posthumous = info.Posthumous ? 1 : 0;
    return person;
}

bool ImportDialog::alreadyKnown(const Person& person, const QVector<Person>& batch) const
{
    if (m_catalog->contains(person))
        return true;
    for (const Person& existing : batch) {
        if (existing.ukaseNo == person.ukaseNo && existing.ukaseDate == person.ukaseDate
            && ((!existing.name.isEmpty() && existing.name == person.name)
                || (!existing.nameBy.isEmpty() && existing.nameBy == person.nameBy)))
            return true;
    }
    return false;
}

void ImportDialog::importRows(bool all)
{
    QVector<Person> batch;
    for (const PersonInfo& info : m_parser.m_Persons) {
        if (!all) {
            if (m_awardId && m_awardId != info.AwardId)
                continue;
            if (!m_fileName.isEmpty() && m_fileName != fromWide(info.FileName))
                continue;
        }
        const Person person = personFromInfo(info);
        if (alreadyKnown(person, batch))
            continue;
        batch.push_back(person);
    }

    if (QMessageBox::question(this, windowTitle(),
            QStringLiteral("Будет проимпортировано %1 записей. Сохранить?").arg(batch.size()))
        != QMessageBox::Yes)
        return;

    QString error;
    if (!m_catalog->saveMany(batch, &error)) {
        QMessageBox::critical(this, windowTitle(), error);
        return;
    }
    if (!batch.isEmpty())
        QMessageBox::information(this, windowTitle(), QStringLiteral("Сохранено записей: %1").arg(batch.size()));
    showPersons();
}
