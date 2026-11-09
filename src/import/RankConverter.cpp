#include "RankConverter.h"
#include "TextUtil.h"

#include <algorithm>

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
			if (posStart != std::wstring::npos)
			{
				rank = info.Nominative + strCheck.substr(posStart + rankOriginal.length());
				StrTrim(rank);
				description.erase(posStart, description.length() - posStart);
				StrTrim(description);
				return true;
			}
		}
	}

	return false;
}
