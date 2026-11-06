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
        ListOfPersons = false;
    }

    int Number = 0;
    QDate Date;
    bool RussianLang = false;
    bool ListOfPersons = false;
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
    std::wstring OriginalText;
    // the name as written in the decree (dative or accusative) and its NounCase
    std::wstring SourceName;
    int SourceCase = 0;
    UkazInfo Ukaz;
    unsigned Flags = 0;
    bool Posthumous = false;
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
