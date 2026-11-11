#include "RankConverter.h"
#include "TextUtil.h"

#include <algorithm>
#include <cwchar>

namespace {

const std::vector<RankInfo> MilitaryRanksRU = {
	{ L"рядовой", L"рядовому", L"рядового" },
	{ L"прапорщик", L"прапорщику", L"прапорщика"},
	{ L"старший прапорщик", L"старшему прапорщику", L"старшего прапорщика"},
	{ L"младший сержант", L"младшему сержанту", L"младшего сержанта" },
	{ L"старший сержант", L"старшему сержанту", L"старшего сержанта" },
	{ L"сержант", L"сержанту", L"сержанта" },
	{ L"старшина", L"старшине", L"старшину" },
	{ L"младший лейтенант", L"младшему лейтенанту", L"младшего лейтенанта" },
	{ L"старший лейтенант", L"старшему лейтенанту", L"старшего лейтенанта" },
	{ L"лейтенант", L"лейтенанту", L"лейтенанта" },
	{ L"капитан", L"капитану", L"капитана" },
	{ L"майор", L"майору", L"майора" },
	{ L"подполковник", L"подполковнику", L"подполковника" },
	{ L"полковник", L"полковнику", L"полковника" },
	{ L"генерал-майор", L"генерал-майору", L"генерал-майора" },
	{ L"генерал-лейтенант", L"генерал-лейтенанту", L"генерал-лейтенанта" },
	{ L"генерал-полковник", L"генерал-полковнику", L"генерал-полковника" },
	{ L"ефрейтор", L"ефрейтору", L"ефрейтора" },
	{ L"мичман", L"мичману", L"мичмана" },
	{ L"старший мичман", L"старшему мичману", L"старшего мичмана" }
};

const std::vector<RankInfo> MilitaryRanksBY = {
	{ L"радавы", L"радавому", L"радавога"},
	{ L"яфрэйтар", L"яфрэйтару", L"яфрэйтара"},
	{ L"прапаршчык", L"прапаршчыку", L"прапаршчыка"},
	{ L"мічман", L"мічману", L"мічмана"},
	{ L"старшы мічман", L"старшаму мічману", L"старшага мічмана"},
	{ L"старшы прапаршчык", L"старшаму прапаршчыку", L"старшага прапаршчыка"},
	{ L"малодшы сяржант", L"малодшаму сяржанту", L"малодшага сяржанта"},
	{ L"старшы сяржант", L"старшаму сяржанту", L"старшага сяржанта" },
	{ L"сяржант", L"сяржанту", L"сяржанта" },
	{ L"старшына", L"старшыне", L"старшыну" },
	{ L"малодшы лейтэнант", L"малодшаму лейтэнанту", L"малодшага лейтэнанта" },
	{ L"старшы лейтэнант", L"старшаму лейтэнанту", L"старшага лейтэнанта" },
	// в указе первая «е» набрана как «э»; именительный остаётся каталожным «лейтэнант»
	{ L"старшы лейтэнант", L"старшаму лэйтэнанту", L"старшага лэйтэнанта" },
	{ L"лейтэнант", L"лейтэнанту", L"лейтэнанта" },
	{ L"капітан", L"капітану", L"капітана" },
	{ L"маёр", L"маёру", L"маёра" },
	{ L"падпалкоўнік", L"падпалкоўніку", L"падпалкоўніка" },
	{ L"палкоўнік", L"палкоўніку", L"палкоўніка" },
	{ L"генерал-маёр", L"генерал-маёру", L"генерал-маёра" },
	{ L"генерал-лейтэнант", L"генерал-лейтэнанту", L"генерал-лейтэнанта" },
	{ L"генерал-палкоўнік", L"генерал-палкоўніку", L"генерал-палкоўніка" },
	{ L"саветнік", L"саветніку", L"саветніка" },
	{ L"старшы саветнік", L"старшаму саветніку", L"старшага саветніка" },
	{ L"малодшы саветнік", L"малодшаму саветніку", L"малодшага саветніка" },
	{ L"дзяржаўны саветнік", L"дзяржаўнаму саветніку", L"дзяржаўнага саветніка" }
};

// сначала более длинные звания: «старшему прапорщику» содержит «прапорщику»,
// «генерал-майору» содержит «майору»
std::vector<const RankInfo*> ranksLongestFirst(const std::vector<RankInfo>& allRanks, NounCase nounCase)
{
	std::vector<const RankInfo*> ranks;
	ranks.reserve(allRanks.size());
	for (const auto& rank : allRanks)
		ranks.push_back(&rank);
	std::stable_sort(ranks.begin(), ranks.end(), [nounCase](const RankInfo* a, const RankInfo* b) {
		const size_t lenA = (nounCase == Dative ? a->Dative : a->Accusative).length();
		const size_t lenB = (nounCase == Dative ? b->Dative : b->Accusative).length();
		return lenA > lenB;
	});
	return ranks;
}

// «старшага памочніка капітана»: звание после такого слова — чьё-то, это должность
bool afterAide(const std::wstring& text, size_t rankPos)
{
	if (rankPos < 2)
		return false;
	const size_t wordEnd = rankPos - 1;
	const size_t space = text.rfind(L' ', wordEnd - 1);
	const size_t wordBegin = space == std::wstring::npos ? 0 : space + 1;
	const std::wstring before = ToLower(text.substr(wordBegin, wordEnd - wordBegin));
	static const wchar_t* aides[] = { L"памочн", L"помощн", L"дублёр", L"дублер", L"ад'ютант", L"адъютант" };
	for (const wchar_t* word : aides)
	{
		if (before.compare(0, std::wcslen(word), word) == 0)
			return true;
	}
	return false;
}

// звание в самом конце без запятой: «…Камітэта дзяржаўнай бяспекі маёра»,
// «…Кітайскай Народнай Рэспублікі генерал-палкоўніка». «памочніка капітана» — должность.
bool extractTrailing(std::wstring& description, std::wstring& rank, const std::vector<const RankInfo*>& ranks, NounCase nounCase)
{
	std::wstring text = description;
	ToCyrillicI(text, false);
	for (const RankInfo* pRank : ranks)
	{
		if (pRank->Nominative.find(L"саветнік") != std::wstring::npos)
			continue;
		const std::wstring& form = nounCase == Dative ? pRank->Dative : pRank->Accusative;
		if (text.length() <= form.length() + 1
			|| text.compare(text.length() - form.length(), form.length(), form) != 0
			|| text[text.length() - form.length() - 1] != L' ')
			continue;
		const size_t wordEnd = text.length() - form.length() - 1;
		if (afterAide(text, wordEnd + 1))
			continue;
		rank = pRank->Nominative;
		description.erase(wordEnd);
		StrTrim(description);
		return true;
	}
	return false;
}

// звание в начале перед запятой: «полковнику, начальнику ...», «генерал-полковнику, Министру обороны»
bool extractLeading(std::wstring& description, std::wstring& rank, const std::vector<const RankInfo*>& ranks, NounCase nounCase)
{
	const size_t comma = description.find(L',');
	if (comma == std::wstring::npos)
		return false;
	std::wstring head = description.substr(0, comma);
	ToCyrillicI(head, false);
	for (const RankInfo* pRank : ranks)
	{
		const std::wstring& form = nounCase == Dative ? pRank->Dative : pRank->Accusative;
		if (head.compare(0, form.length(), form) != 0)
			continue;
		const std::wstring rest = head.substr(form.length());
		if (!rest.empty() && rest[0] != L' ')
			continue;
		// хвост звания: «милиции», «унутранай службы»; «старшине группы» — должность
		static const wchar_t* tails[] = {
			L" милиции", L" міліцыі", L" юстиции", L" юстыцыі", L" внутренней службы", L" унутранай службы",
			L" медицинской службы", L" медыцынскай службы",
		};
		bool knownTail = rest.empty();
		for (const wchar_t* tail : tails)
			knownTail = knownTail || rest == tail;
		if (!knownTail)
			continue;
		std::wstring job = description.substr(comma + 1);
		StrTrim(job);
		if (job.empty())
			return false;
		rank = pRank->Nominative + rest;
		StrTrim(rank);
		description = std::move(job);
		return true;
	}
	return false;
}

} // namespace

bool CRankConverter::Extract(std::wstring& description, std::wstring& rank, LangType lang, NounCase nounCase)
{
	size_t posComma = description.rfind(L',');
	size_t posStart = 0;
	if (posComma != std::wstring::npos)
		posStart = description.find_first_not_of(L" ,", posComma);

	if (posStart == std::wstring::npos || posStart >= description.length())
		return false;

	std::wstring strCheck = description.substr(posStart);
	ToCyrillicI(strCheck, false);

	static const std::vector<const RankInfo*> ranksByDative[2] = {
		ranksLongestFirst(MilitaryRanksRU, Dative), ranksLongestFirst(MilitaryRanksBY, Dative) };
	static const std::vector<const RankInfo*> ranksByAccusative[2] = {
		ranksLongestFirst(MilitaryRanksRU, Accusative), ranksLongestFirst(MilitaryRanksBY, Accusative) };
	const auto& ranks = (nounCase == Dative ? ranksByDative : ranksByAccusative)[lang == LangBY ? 1 : 0];

	for (const RankInfo* pRank : ranks)
	{
		const RankInfo& info = *pRank;
		const std::wstring& rankOriginal = (nounCase == Dative ? info.Dative : info.Accusative);
		if (rankOriginal.length() > strCheck.length())
			continue;

		// после звания слов немного («міліцыі», «унутранай службы ў адстаўцы»,
		// «медыцынскай службы запасу»): «капітана-механіка», «капітана – зменнага
		// механіка цеплахода», «капітана нацыянальнай каманды па тэнісу» — должности
		{
			const size_t posRank = strCheck.find(rankOriginal);
			if (posRank != std::wstring::npos)
			{
				const std::wstring rest = strCheck.substr(posRank + rankOriginal.length());
				if (!rest.empty() && (rest[0] == L'-' || rest.compare(0, 2, L" –") == 0
					|| std::count(rest.begin(), rest.end(), L' ') > 5))
					continue;
			}
		}

		// «саветнік» — звание только как «саветнік юстыцыі» или с классом
		// («дзяржаўны саветнік ... 3 класа»); «саветнік упраўлення ...» — должность
		if (info.Nominative.find(L"саветнік") != std::wstring::npos)
		{
			const size_t posRank = strCheck.find(rankOriginal);
			if (posRank == std::wstring::npos)
				continue;
			const std::wstring rest = strCheck.substr(posRank + rankOriginal.length());
			if (rest.compare(0, 8, L" юстыцыі") != 0 && rest.find(L"класа") == std::wstring::npos)
				continue;
		}

		if (posComma != std::wstring::npos)
		{
			if (strCheck.compare(0, rankOriginal.length(), rankOriginal) == 0)
			{
				rank = info.Nominative + strCheck.substr(rankOriginal.length());
				StrTrim(rank);
				description.erase(posComma, description.length() - posComma);
				return true;
			}
		}
		else
		{
			posStart = strCheck.find(rankOriginal.c_str());
			if (posStart != std::wstring::npos && !afterAide(strCheck, posStart))
			{
				rank = info.Nominative + strCheck.substr(posStart + rankOriginal.length());
				StrTrim(rank);
				description.erase(posStart, description.length() - posStart);
				StrTrim(description);
				return true;
			}
		}
	}

	return extractTrailing(description, rank, ranks, nounCase)
		|| extractLeading(description, rank, ranks, nounCase);
}
