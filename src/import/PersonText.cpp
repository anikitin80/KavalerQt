#include "PersonText.h"

#include "TextUtil.h"

#include <QString>

namespace decree {

bool isLowerCyrillic(wchar_t c)
{
	return (c >= L'а' && c <= L'я') || c == L'ё' || c == L'і' || c == L'ў';
}

bool isUpperCyrillic(wchar_t c)
{
	return (c >= L'А' && c <= L'Я') || c == L'Ё' || c == L'І' || c == L'Ў';
}

bool isWordLetter(wchar_t c)
{
	return isLowerCyrillic(c) || isUpperCyrillic(c) || c == L'\'' || c == L'\u2019' || c == L'\u02BC';
}

// сколько раз каталог видел слово; слово, которое каталог знает только в другой
// форме («прапаршчыка» при «прапаршчык»), считается один раз
int knownCount(const std::map<std::wstring, int>& words, const std::wstring& word)
{
	const auto exact = words.find(word);
	if (exact != words.end())
		return exact->second;
	if (word.length() < 7)
		return 0;
	const std::wstring stem = word.substr(0, word.length() - 2);
	const auto next = words.lower_bound(stem);
	return next != words.end() && next->first.compare(0, stem.length(), stem) == 0 ? 1 : 0;
}

// латинские двойники в кириллических словах («пaдпaлкоўнiка», «Віктаровічa»)
void toCyrillicLetters(std::wstring& text, LangType lang)
{
	text = NormalizeCyrillicText(QString::fromStdWString(text), lang == LangBY).toStdWString();
}

// «”Ушацкі лясгас“», «“…”»: типографские кавычки — ёлочки, как в каталоге
void toGuillemets(std::wstring& text)
{
	for (size_t i = 0; i < text.size(); ++i)
	{
		const wchar_t c = text[i];
		if (c != L'\u201C' && c != L'\u201D' && c != L'\u201E')
			continue;
		const bool letterAfter = i + 1 < text.size() && (isWordLetter(text[i + 1]) || iswalnum(text[i + 1]));
		const bool letterBefore = i > 0 && (isWordLetter(text[i - 1]) || iswalnum(text[i - 1]));
		if (letterAfter && !letterBefore)
			text[i] = L'«';
		else if (letterBefore && !letterAfter)
			text[i] = L'»';
	}
}

// «АксануУладзіміраўну»: отчество слиплось с именем
void splitGluedPatronymic(CNameConverter& names, std::wstring& name, LangType lang, NounCase nounCase)
{
	for (size_t i = 1; i < name.size(); ++i)
	{
		if (!isLowerCyrillic(name[i - 1]) || !isUpperCyrillic(name[i]))
			continue;
		size_t end = name.find(L' ', i);
		if (end == std::wstring::npos)
			end = name.size();
		if (names.IsMiddleName(name.substr(i, end - i), lang, nounCase))
			name.insert(i, 1, L' ');
	}
}

// «Алексия ІІ»: римская цифра в русском имени латиницей, как в каталоге
void latinRomanNumerals(std::wstring& name)
{
	size_t begin = 0;
	while (begin < name.size())
	{
		size_t end = name.find_first_of(L" ()", begin);
		if (end == std::wstring::npos)
			end = name.size();
		bool roman = end - begin >= 2;
		for (size_t i = begin; i < end && roman; ++i)
			roman = std::wstring(L"ІIVХX").find(name[i]) != std::wstring::npos;
		if (roman)
		{
			for (size_t i = begin; i < end; ++i)
			{
				if (name[i] == L'І')
					name[i] = L'I';
				else if (name[i] == L'Х')
					name[i] = L'X';
			}
		}
		begin = end + 1;
	}
}

} // namespace decree
