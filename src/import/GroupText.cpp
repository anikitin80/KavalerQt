#include "GroupText.h"

#include "TextUtil.h"

#include <cwchar>

namespace decree {

bool isGroupLetter(wchar_t c)
{
	return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z')
		|| (c >= L'А' && c <= L'я') || c == L'Ё' || c == L'ё'
		|| c == L'І' || c == L'і' || c == L'Ў' || c == L'ў';
}

// точка кончает предложение, но не «В.И.Ленина» и не «г. Минска»
size_t groupSentenceEnd(const std::wstring& text)
{
	for (size_t i = 0; i < text.length(); ++i)
	{
		const wchar_t c = text[i];
		if (c == L':' || c == L'\t' || c == L'\n' || c == L'\r')
			return i;
		if (c != L'.')
			continue;
		const bool before = i > 0 && isGroupLetter(text[i - 1]);
		const bool after = i + 1 < text.length() && isGroupLetter(text[i + 1]);
		if (before && after)
			continue;
		const bool singleLetter = before && (i == 1 || !isGroupLetter(text[i - 2]));
		if (singleLetter)
			continue;
		return i;
	}
	return std::wstring::npos;
}

// запятая, после которой учёная степень, а не продолжение должности
size_t academicTitleComma(const std::wstring& text)
{
	static const wchar_t* titles[] = {
		L"профессор", L"доктор", L"кандидат", L"академик", L"доцент", L"лауреат",
		L"прафесар", L"доктар", L"кандыдат", L"акадэмік", L"дацэнт",
	};
	for (size_t comma = text.find(L','); comma != std::wstring::npos; comma = text.find(L',', comma + 1))
	{
		if (InsideQuotes(text, comma))
			continue;
		size_t pos = comma + 1;
		while (pos < text.length() && text[pos] == L' ')
			++pos;
		const std::wstring rest = ToLower(text.substr(pos));
		for (const wchar_t* title : titles)
		{
			const size_t length = std::wcslen(title);
			if (rest.compare(0, length, title) == 0)
				return comma;
		}
	}
	return std::wstring::npos;
}

// самое правое слово группы: «группу работников завода» — завод, а не «работников завода»
bool findLastToken(const std::wstring& str, const std::vector<std::wstring>& tokens, size_t& start, size_t& end)
{
	bool found = false;
	for (const auto& token : tokens)
	{
		for (size_t pos = str.find(token); pos != std::wstring::npos; pos = str.find(token, pos + 1))
		{
			const size_t tokenEnd = pos + token.size();
			if (!found || tokenEnd > end || (tokenEnd == end && pos < start))
			{
				start = pos;
				end = tokenEnd;
				found = true;
			}
		}
	}
	return found;
}

// пустая должность: в каталоге стоит само слово списка в именительном единственном
std::wstring groupRole(const std::wstring& token)
{
	if (token == L"работников" || token == L"работникам")
		return L"работник";
	if (token == L"работнікаў" || token == L"работнікамі")
		return L"работнік";
	if (token == L"военнослужащих" || token == L"военнослужащими")
		return L"военнослужащий";
	if (token == L"ваеннаслужачых" || token == L"ваеннаслужачымі")
		return L"ваеннаслужачы";
	return std::wstring();
}

// у автомобильного завода каталог не держит родительское объединение;
// у рессорного и кузнечного заводов того же объединения — держит
void cutTrailingAssociation(std::wstring& text)
{
	if (text.find(L"«Мінскі аўтамабільны завод»") == std::wstring::npos
		&& text.find(L"«Минский автомобильный завод»") == std::wstring::npos
		&& text.find(L"Минского автомобильного завода") == std::wstring::npos)
		return;
	const wchar_t* tails[] = {
		L" вытворчага аб'яднання «",
		L" вытворчага аб\u2019яднання «",
		L" вытворчага аб\u02BCяднання «",
		L" производственного объединения «",
	};
	for (const wchar_t* tail : tails)
	{
		const size_t pos = text.rfind(tail);
		if (pos == std::wstring::npos || pos == 0)
			continue;
		const size_t open = text.find(L'«', pos);
		const size_t close = open == std::wstring::npos ? std::wstring::npos : text.find(L'»', open);
		if (close == std::wstring::npos)
			continue;
		size_t after = close + 1;
		while (after < text.size() && text[after] == L' ')
			++after;
		if (after != text.size())
			continue;
		text.erase(pos);
		StrTrim(text);
		return;
	}
}

// «завод» і акадэмія: к должности идёт завод, акадэмия остаётся тем, кто её уже назвал
std::wstring cutOtherOrganisation(std::wstring& groupInfo)
{
	bool inQuote = false;
	for (size_t i = 0; i + 3 <= groupInfo.size(); ++i)
	{
		if (groupInfo[i] == L'«')
			inQuote = true;
		else if (groupInfo[i] == L'»')
			inQuote = false;
		if (inQuote)
			continue;
		const bool conjunction = groupInfo.compare(i, 3, L" і ") == 0 || groupInfo.compare(i, 3, L" и ") == 0;
		if (!conjunction)
			continue;
		size_t next = i + 3;
		while (next < groupInfo.size() && groupInfo[next] == L' ')
			++next;
		if (next >= groupInfo.size())
			break;
		const wchar_t character = groupInfo[next];
		if (character != L'«' && !IsCapital(character))
			continue;
		std::wstring other = groupInfo.substr(next);
		groupInfo.erase(i);
		StrTrim(groupInfo);
		StrTrim(other);
		return other;
	}
	return std::wstring();
}

// «работников Белорусского государственного университета»: головное слово —
// это сами награждённые, к должности добавляется организация
void stripGroupHead(std::wstring& groupInfo)
{
	static const wchar_t* heads[] = {
		L"наградить", L"узнагародзіць", L"группы", L"групы", L"творческих", L"творчых",
		L"работников", L"работникам", L"работнікаў", L"сотрудников", L"супрацоўнікаў", L"супрацоўнікам",
		L"артистов", L"артистам", L"артыстаў", L"артыстам", L"і", L"и",
		L"прадпрыемстваў", L"предприятий",
	};
	for (bool stripped = true; stripped;)
	{
		stripped = false;
		StrTrim(groupInfo);
		for (const wchar_t* head : heads)
		{
			const size_t length = std::wcslen(head);
			if (groupInfo.compare(0, length, head) == 0 && groupInfo.length() > length && groupInfo[length] == L' ')
			{
				groupInfo.erase(0, length + 1);
				stripped = true;
			}
		}
	}
}

} // namespace decree
