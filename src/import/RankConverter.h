#pragma once

#include "NameConverter.h"

#include <string>
#include <vector>

// именительный и две формы из указа. Тот же вид у невоинского звания
// в словаре должностей («прафесар»).
struct RankInfo
{
	std::wstring Nominative;
	std::wstring Dative;
	std::wstring Accusative;
};

// воинское звание из текста должности: склонённая форма из указа
// становится именительной и вырезается из строки
class CRankConverter
{
public:
	// rank — именительный плюс хвост («милиции», «в отставке»).
	// description теряет звание. Ложь, если звания нет: строки не меняются.
	static bool Extract(std::wstring& description, std::wstring& rank, LangType lang, NounCase nounCase);
};
