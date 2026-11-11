#pragma once

#include <QDate>
#include <map>
#include <string>

constexpr unsigned kFlagNeedReview = 0x8000000;

struct AwardInfo
{
    int Id = 0;
    int TotalCount = 0;
    std::wstring CustomName;
    std::map<std::wstring, unsigned> CountByFiles;
    bool NeedReview = false;
};

struct UkazInfo
{
    void Clear()
    {
        Number = 0;
        Date = {};
        RussianLang = false;
    }

    int Number = 0;
    QDate Date;
    bool RussianLang = false;
};

struct PersonInfo
{
    enum NormalizationFlags
    {
        NF_NAME = 1,
        NF_PROFESSION = 2,
        NF_RANK = 4
    };

    int AwardId = 0;
    std::wstring FileName;
    std::wstring Name;
    std::wstring Description;
    std::wstring MilitaryRank;
    // имя, как написано в указе (дательный или винительный), и его NounCase
    std::wstring SourceName;
    int SourceCase = 0;
    UkazInfo Ukaz;
    unsigned Flags = 0;
    bool Posthumous = false;
    // награждена сама организация: Name пустое, название и район лежат в Description
    bool Organization = false;
    // стоит там, где действуют сведения о группе указа (см. CDocumentParser::m_GroupActive)
    bool InGroup = false;
    // должность из строки пункта («Узнагародзіць спартсменаў-інструктараў …:»),
    // если своей в списке нет; в Description попадает в конце разбора
    std::wstring PointJob;
};

struct DecreeAward
{
    enum AwardType
    {
        Hero = 1,
        Orden = 2,
        Medal = 3,
        Pochet = 4
    };

    long ID = 0;
    std::wstring Name;
    std::wstring NameBY;
    int Type = 0;
    int Priority = 0;
};
