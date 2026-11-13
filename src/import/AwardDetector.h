#pragma once

#include "ParserTypes.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

// Награда по тексту указа («ордэнам Пашаны», «медалью «За отвагу»», «почетное
// звание «Заслуженный ...»») — номер награды каталога.
class CAwardDetector
{
public:
	// названия наград каталога без вида награды («орден», «медаль») и кавычек
	void Initialize(const std::map<long, DecreeAward>& awards);

	// phraseLength, если задан, получает, сколько начальных символов award —
	// сама фраза награды (с кавычками и суффиксом страны), дальше идёт человек.
	// 0, если ничего не совпало. bPochetNameOnly: в award только название звания.
	int Find(std::wstring award, bool bRussian, bool bPochetNameOnly = false, size_t* phraseLength = nullptr) const;

private:
	typedef std::vector<std::pair<std::wstring, int>> Entries;
	Entries m_MedalDetector;
	Entries m_OrdenDetector;
	Entries m_PochetDetector;

	Entries m_MedalDetectorBY;
	Entries m_OrdenDetectorBY;
	Entries m_PochetDetectorBY;
};
