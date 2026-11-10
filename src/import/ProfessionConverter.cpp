#include "ProfessionConverter.h"
#include "TextUtil.h"

#include <QFile>
#include <QSettings>
#include <QTextStream>
#include <algorithm>
#include <cwchar>

// должности приводятся с SexAny: правило SexFemale или SexMale здесь никогда не совпадёт
const RulesList CProfessionConverter::RulesAdjectives = CNameConverter::ExpandRules({
	// раньше общего «-ому» → «-ый»: «былому» — это «былы», а не «былый».
	// «каму»/«кому» стоит выше «ому» («такому» → «такі»)
	// после г, к, х — «і»: «другога пілота» → «другі пілот»
	{ SexAny, LangBY, L"каму,кому,гаму,гому,хаму,хому", L"кага,кога,гага,гога,хага,хога", L"---і"},
	{ SexAny, LangBY, L"аму,ому", L"ага,ога", L"---ы"},	// былога, былому
	{ SexAny, LangAny, L"ему", L"его", L"---ий"},
	// после г, к, х — «-ий»: «техническому директору» → «технический директор».
	// Ударное «-ой» («буровой») правило не угадает: такие слова есть в словаре
	{ SexAny, LangRU, L"кому,гому,хому", L"кого,гого,хого", L"---ий"},
	{ SexAny, LangAny, L"ому", L"ого", L"---ый"},
	{ SexAny, LangAny, L"ой", L"ую", L"--ая"},
	// после шипящих — «-ая»: «старшей медицинской сестре» → «старшая …»
	{ SexAny, LangRU, L"шей,жей,чей,щей", L"", L"--ая"},
	{ SexAny, LangAny, L"ей", L"юю", L"--яя"},	// хатнюю → хатняя, раньше белорусского «-юю»
	// женский род (мужской дательный — «-аму», «-ай»/«-яй» всегда женские)
	{ SexAny, LangBY, L"ай,яй", L"ую,юю", L"--ая"},
});

const RulesList CProfessionConverter::RulesProfession = CNameConverter::ExpandRules({
	{ SexAny, LangAny, L"у", L"а", L"-"},
	{ SexAny, LangAny, L"е", L"у", L"-а"},
	{ SexAny, LangAny, L"ю", L"я", L"-ь"}
});

namespace {

// слово кончается одним из окончаний и длиннее его хотя бы на три буквы
bool EndsWithAny(const std::wstring& word, std::initializer_list<const wchar_t*> endings)
{
	for (const wchar_t* ending : endings)
	{
		const size_t length = std::wcslen(ending);
		if (word.length() > length + 2 && word.compare(word.length() - length, length, ending) == 0)
			return true;
	}
	return false;
}

// именительный прилагательного («вядучы», «дзяжурны», «старшая», «главный»)
bool IsAdjective(const std::wstring& word)
{
	return EndsWithAny(word, { L"ы", L"і", L"ый", L"ий", L"ой", L"ая", L"яя" });
}

// вид, который ищет Lookup: нижний регистр и белорусская «і» вместо латинской «i»
std::wstring dictionaryKey(std::wstring word, LangType lang)
{
	if (lang == LangBY)
		ToCyrillicI(word, true);
	return ToLower(word);
}

} // namespace

void CProfessionConverter::Initialize()
{
	QSettings settings;
	SetDictionaryPath(settings.value(QStringLiteral("DictionaryRU")).toString().toStdWString(), LangRU);
	SetDictionaryPath(settings.value(QStringLiteral("DictionaryBY")).toString().toStdWString(), LangBY);
}

CProfessionConverter::LanguageData& CProfessionConverter::languageData(LangType lang)
{
	return lang == LangRU ? m_russian : m_belarusian;
}

const CProfessionConverter::LanguageData& CProfessionConverter::languageData(LangType lang) const
{
	return lang == LangRU ? m_russian : m_belarusian;
}

const std::wstring& CProfessionConverter::DictionaryPath(LangType lang) const
{
	return languageData(lang).path;
}

bool CProfessionConverter::SetDictionaryPath(const std::wstring& filePath, LangType lang)
{
	LanguageData& data = languageData(lang);
	data.accusative.clear();
	data.dative.clear();
	data.nominatives.clear();
	data.path = filePath;
	if (filePath.empty())
		return false;
	// отсутствующий файл нормален: он создастся при первом сохранении
	if (!QFile::exists(QString::fromStdWString(filePath)))
		return true;
	return LoadDictionary(filePath, lang);
}

bool CProfessionConverter::LoadDictionary(const std::wstring& filePath, LangType lang)
{
	QFile file(QString::fromStdWString(filePath));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;

	LanguageData& data = languageData(lang);
	Dictionary* dictionaryAcc = &data.accusative;
	Dictionary* dictionaryDat = &data.dative;

	QTextStream input(&file);
	input.setEncoding(QStringConverter::Utf8);
	while (!input.atEnd()) {
		const QString line = input.readLine();
		const int pos1 = line.indexOf(QLatin1Char(';'));
		const int pos2 = line.indexOf(QLatin1Char(';'), pos1 + 1);
		// пустые слова пропускаются: преобразователь оставляет первую букву
		// замены, и пустая его уронит. Ключ и именительный хранятся так, как
		// ищет Lookup, поэтому «Мацi» и «маці» — одно слово
		const auto add = [lang](Dictionary* dictionary, const QString& form, const QString& nominative) {
			const std::wstring key = dictionaryKey(form.toStdWString(), lang);
			const std::wstring value = dictionaryKey(nominative.toStdWString(), lang);
			if (!key.empty() && !value.empty())
				(*dictionary)[key] = value;
		};
		if (pos2 != -1) {
			add(dictionaryDat, line.mid(pos1 + 1, pos2 - pos1 - 1), line.left(pos1));
			add(dictionaryAcc, line.mid(pos2 + 1), line.left(pos1));
		} else if (pos1 != -1) {
			add(dictionaryAcc, line.left(pos1), line.mid(pos1 + 1));
		}
	}
	rebuildNominatives(lang);
	return true;
}

void CProfessionConverter::rememberNominative(LangType lang, std::wstring nominative)
{
	nominative = dictionaryKey(std::move(nominative), lang);
	if (!nominative.empty())
		languageData(lang).nominatives.insert(std::move(nominative));
}

void CProfessionConverter::rebuildNominatives(LangType lang)
{
	LanguageData& data = languageData(lang);
	data.nominatives.clear();
	const Dictionary* maps[] = { &data.dative, &data.accusative };
	for (const Dictionary* dictionary : maps) {
		for (const auto& item : *dictionary)
			rememberNominative(lang, item.second);
	}
}

bool CProfessionConverter::nominativeFromRules(std::wstring word, LangType lang, NounCase nounCase, std::wstring& nominative) const
{
	if (!CNameConverter::ApplyRules(word, RulesProfession, SexAny, lang, nounCase, false) || word.empty())
		return false;
	word = ToLower(word);
	const auto& known = languageData(lang).nominatives;
	if (known.find(word) == known.end())
		return false;
	nominative = word;
	return true;
}

bool CProfessionConverter::SaveDictionary(const std::wstring& filePath, LangType lang)
{
	if (filePath.empty())
		return false;
	QFile file(QString::fromStdWString(filePath));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return false;

	// у одного именительного бывает несколько форм («суддзі» и «суддзям»,
	// «артыста» и «артыстаў»): каждая пара идёт своей строкой, иначе
	// сохранение оставило бы одну и потеряло остальные
	std::map<std::wstring, std::pair<std::vector<std::wstring>, std::vector<std::wstring>>> dictionary;
	LanguageData& data = languageData(lang);
	for (const auto& item : data.dative)
		dictionary[item.second].first.push_back(item.first);
	for (const auto& item : data.accusative)
		dictionary[item.second].second.push_back(item.first);

	QTextStream output(&file);
	output.setEncoding(QStringConverter::Utf8);
	output.setGenerateByteOrderMark(true);
	for (const auto& item : dictionary)
	{
		const std::vector<std::wstring>& dative = item.second.first;
		const std::vector<std::wstring>& accusative = item.second.second;
		for (size_t i = 0; i < std::max(dative.size(), accusative.size()); ++i)
			output << QString::fromStdWString(item.first) << ';'
			       << (i < dative.size() ? QString::fromStdWString(dative[i]) : QString()) << ';'
			       << (i < accusative.size() ? QString::fromStdWString(accusative[i]) : QString()) << '\n';
	}
	return true;
}

bool CProfessionConverter::UpdateDictionary(std::vector<NewDictItem>& newItems)
{
	bool bSaveBY = false, bSaveRU = false;
	for (const auto& item : newItems)
	{
		if (!item.Checked || (item.Language != LangRU && item.Language != LangBY))
			continue;

		std::wstring original = item.Original, word = item.Word;
		if (item.Language == LangBY)
		{
			ToCyrillicI(original, true);
			ToCyrillicI(word, true);
		}
		Dictionary& dictionary = item.Case == Dative
			? languageData(item.Language).dative
			: languageData(item.Language).accusative;
		dictionary[std::move(original)] = std::move(word);
		if (item.Language == LangBY)
			bSaveBY = true;
		else
			bSaveRU = true;
	}

	// словари в памяти уже обновлены; сообщаем, удалось ли сохранить
	if (bSaveBY)
		rebuildNominatives(LangBY);
	if (bSaveRU)
		rebuildNominatives(LangRU);
	bool bRes = true;
	if (bSaveBY && !SaveDictionary(languageData(LangBY).path, LangBY))
		bRes = false;
	if (bSaveRU && !SaveDictionary(languageData(LangRU).path, LangRU))
		bRes = false;
	return bRes;
}

std::wstring CProfessionConverter::GetNextWord(const std::wstring& strSource, size_t& posStart, size_t* posEndOut)
{
	posStart = strSource.find_first_not_of(L" ,-–", posStart);
	if (posStart == std::wstring::npos)
		return L"";

	// те же разделители с обоих концов: длинное тире начинало слово, но не кончало,
	// и «былому–інспектару» оставалось одним словом
	size_t posEnd = strSource.find_first_of(L" ,-–", posStart);
	if (posEnd == std::wstring::npos)
		posEnd = strSource.length();
	if (posEndOut)
		*posEndOut = posEnd;

	std::wstring word;
	if(posEnd > posStart)
		word = strSource.substr(posStart, posEnd - posStart);
	return word;
}

const Dictionary& CProfessionConverter::DictionaryFor(LangType lang, NounCase nounCase) const
{
	const LanguageData& data = languageData(lang);
	return nounCase == Dative ? data.dative : data.accusative;
}

CProfessionConverter::WordForm CProfessionConverter::findForm(const std::wstring& word, LangType lang, NounCase nounCase) const
{
	WordForm form;
	form.declinedLength = word.length();
	const Dictionary& dictionary = DictionaryFor(lang, nounCase);
	const auto found = Lookup(dictionary, word, lang);
	if (found != dictionary.end())
	{
		form.found = true;
		form.inDictionary = true;
		form.nominative = found->second;
		form.declinedLength = found->first.length();
		return form;
	}
	if (nominativeFromRules(word, lang, nounCase, form.nominative))
		form.found = true;
	return form;
}

// в указе ещё может быть заглавная или латинская «i»; в ключе словаря их нет
Dictionary::const_iterator CProfessionConverter::Lookup(const Dictionary& dictionary, std::wstring word, LangType lang)
{
	return dictionary.find(dictionaryKey(std::move(word), lang));
}

// заменяет слово длины oldLength в pos, оставляя первую букву (она может быть заглавной)
void CProfessionConverter::ReplaceKeepingFirstLetter(std::wstring& text, size_t pos, size_t oldLength, const std::wstring& replacement)
{
	text.replace(pos + 1, oldLength - 1, replacement, 1, std::wstring::npos);
}

// приводит словарное слово в posStart и то, что к нему относится:
// остальные части составного («урача-анестэзіёлага-рэаніматолага») и, после
// прилагательного, следующие слова должности («дзяжурнага памочніка», «вядучага
// навуковага супрацоўніка», «вядучага ветэрынарнага ўрача»); возвращает позицию
// сразу после приведённых слов
size_t CProfessionConverter::ConvertWordChain(std::wstring& strSource, size_t posStart, size_t declinedLength,
                                              std::wstring nominative, LangType lang, NounCase nounCase)
{
	for (;;)
	{
		ReplaceKeepingFirstLetter(strSource, posStart, declinedLength, nominative);

		// остальные части: «урача-анестэзіёлага-рэаніматолага»
		posStart += nominative.length();
		while (posStart < strSource.length() && strSource[posStart] == L'-')
		{
			std::wstring word = ToLower(GetNextWord(strSource, posStart));
			if (word.empty())
				break;
			const WordForm part = findForm(word, lang, nounCase);
			size_t partLength = word.length();
			if (part.found)
			{
				ReplaceKeepingFirstLetter(strSource, posStart, part.declinedLength, part.nominative);
				partLength = part.nominative.length();
			}
			else
			{
				const size_t wordLength = word.length();
				if (AddNewWord(word, lang, nounCase))
				{
					ReplaceKeepingFirstLetter(strSource, posStart, wordLength, word);
					partLength = word.length();
				}
			}
			posStart += partLength;
		}

		if (!IsAdjective(nominative) || posStart >= strSource.length() || strSource[posStart] != L' ')
			return posStart;
		size_t posNext = posStart;
		const std::wstring next = ToLower(GetNextWord(strSource, posNext));
		if (next.empty() || posNext != posStart + 1)
			return posStart;

		const WordForm nextForm = findForm(next, lang, nounCase);
		// не «следчага следчага аддзялення»: второе относится к «аддзялення»
		if (nextForm.found && nextForm.nominative == nominative)
			return posStart;
		if (!nextForm.found)
		{
			// одно прилагательное не из словаря, затем должность:
			// «вядучага ветэрынарнага ўрача» (но не «следчага 2-га следчага»)
			std::wstring adjective = strSource.substr(posNext, next.length());
			const size_t posNoun = posNext + next.length() + 1;
			if (posNoun < strSource.length() && strSource[posNoun - 1] == L' '
				&& CNameConverter::ApplyRules(adjective, RulesAdjectives, SexAny, lang, nounCase, false))
			{
				size_t posWord = posNoun;
				const std::wstring noun = ToLower(GetNextWord(strSource, posWord));
				if (!noun.empty() && posWord == posNoun)
				{
					const WordForm nounForm = findForm(noun, lang, nounCase);
					if (nounForm.found && !IsAdjective(nounForm.nominative))
					{
						strSource.replace(posNext, next.length(), adjective);
						posStart = posNoun + adjective.length() - next.length();
						declinedLength = nounForm.declinedLength;
						nominative = nounForm.nominative;
						continue;
					}
				}
			}
			return posStart;
		}
		posStart = posNext;
		declinedLength = nextForm.declinedLength;
		nominative = nextForm.nominative;
	}
}

// должность в скобках после основной: «артыст-вакаліст (саліста)»,
// «артыст балета (салісту)», «начальнік упраўлення (статс-сакратара)»; только
// сама должность: «(загадчыка аддзялення)» → «(загадчык аддзялення)»,
// «(галоўнага ўрача)» → «(галоўны ўрач)», остальное — родительный. Возвращает
// позицию после скобок (posStart, если скобок нет)
namespace {

// «(псіхолага)», «(статс-сакратара)» стоят после трёх слов родительного, и каталог
// хранит их в именительном. Это правило каталога, словарём его не заменить: другие
// скобки из одного слова («(вадзіцеля)», «(урача)», «(начальніка)») он держит как в указе.
bool bracketAllowsLongGap(const std::wstring& text, size_t open, size_t close)
{
	std::wstring inside = ToLower(text.substr(open + 1, close - open - 1));
	const size_t cut = inside.find_first_of(L" )");
	if (cut != std::wstring::npos)
		inside.resize(cut);
	return inside == L"псіхолага" || inside == L"псіхолагу"
		|| inside == L"статс-сакратара" || inside == L"статс-сакратару";
}

} // namespace

size_t CProfessionConverter::ConvertBracketed(std::wstring& strSource, size_t posStart, LangType lang, NounCase nounCase)
{
	const size_t posOpen = strSource.find(L'(', posStart);
	if (posOpen == std::wstring::npos)
		return posStart;
	const std::wstring between = strSource.substr(posStart, posOpen - posStart);
	const size_t posClose = strSource.find(L')', posOpen);
	if (posClose == std::wstring::npos)
		return posStart;
	// «урач хуткай медыцынскай дапамогі (загадчыку цэнтра …)», «(урача-псіхіятра)»: скобка
	// далеко, но начинается с должности из словаря, за которой скобка продолжается.
	// Одно слово в скобке каталог держит в родительном: «сродкаў аховы (начальніка)»,
	// «разліку (вадзіцеля)»; исключения — bracketAllowsLongGap
	const size_t posWordEnd = strSource.find_first_of(L" ,-–)", posOpen + 1);
	const bool professionInside = posWordEnd != std::wstring::npos && strSource[posWordEnd] != L')'
		&& findForm(ToLower(strSource.substr(posOpen + 1, posWordEnd - posOpen - 1)), lang, nounCase).found;
	const bool longGap = professionInside || bracketAllowsLongGap(strSource, posOpen, posClose);
	if (between.find_first_of(L",–«»;") != std::wstring::npos
		|| std::count(between.begin(), between.end(), L' ') > (longGap ? 6 : 3))
		return posStart;

	// каждая часть «статс-сакратара» отдельно («урача-тэрапеўта»: часть после
	// уже приведённой приводится правилами)
	size_t posPart = posOpen + 1;
	bool converted = false;
	for (;;)
	{
		const size_t posCloseNow = strSource.find(L')', posOpen);
		if (posPart >= posCloseNow)
			break;
		const size_t posPartEnd = std::min(strSource.find_first_of(L"- )", posPart), posCloseNow);
		std::wstring part = ToLower(strSource.substr(posPart, posPartEnd - posPart));
		const size_t originalLength = part.length();
		const bool afterHyphen = strSource[posPart - 1] == L'-';
		if (part.empty() || (converted && !afterHyphen))
			break;
		const WordForm partForm = findForm(part, lang, nounCase);
		size_t partLength = originalLength;
		if (partForm.found)
		{
			ReplaceKeepingFirstLetter(strSource, posPart, originalLength, partForm.nominative);
			partLength = partForm.nominative.length();
			converted = true;
		}
		else if (converted && afterHyphen && AddNewWord(part, lang, nounCase))
		{
			ReplaceKeepingFirstLetter(strSource, posPart, originalLength, part);
			partLength = part.length();
		}
		else if (!converted && !afterHyphen && posPartEnd < posCloseNow && strSource[posPartEnd] == L' '
			&& CNameConverter::ApplyRules(part, RulesAdjectives, SexAny, lang, nounCase, false))
		{
			// прилагательное перед должностью
			const size_t posNext = posPartEnd + 1;
			const size_t posNextEnd = std::min(strSource.find_first_of(L"- )", posNext), posCloseNow);
			const std::wstring nextWord = ToLower(strSource.substr(posNext, posNextEnd - posNext));
			if (!findForm(nextWord, lang, nounCase).found)
				break;
			strSource.replace(posPart, originalLength, part);
			partLength = part.length();
		}
		else if (!converted && posPartEnd < posCloseNow && strSource[posPartEnd] == L'-')
		{
			// «статс-сакратара»: в словаре только последняя часть
		}
		else
			break;
		posPart += partLength + 1;
	}
	return strSource.find(L')', posOpen) + 1;
}

size_t CProfessionConverter::ConvertTrailingWord(std::wstring& strSource, size_t posStart, LangType lang, NounCase nounCase)
{
	if (posStart == 0 || posStart + 1 >= strSource.length() || strSource[posStart] != L' ')
		return posStart;
	const size_t posHead = strSource.find_last_of(L" -", posStart - 1);
	const std::wstring head = ToLower(strSource.substr(posHead == std::wstring::npos ? 0 : posHead + 1,
		posHead == std::wstring::npos ? posStart : posStart - posHead - 1));
	if (head.empty() || IsAdjective(head))
		return posStart;

	const bool bracket = strSource[posStart + 1] == L'(';
	const size_t posWord = posStart + (bracket ? 2 : 1);
	const size_t posWordEnd = strSource.find_first_of(L" ,-–)«»", posWord);
	if (posWordEnd == std::wstring::npos || posWordEnd == posWord)
		return posStart;
	std::wstring word = strSource.substr(posWord, posWordEnd - posWord);
	const std::wstring lower = ToLower(word);

	// «медыцынскую сястру (палатную)», «санітарку палатную»: прилагательное женского
	// рода после должности; в скобке оно одно
	const wchar_t last = head.back();
	const bool feminineHead = last == L'а' || last == L'я';
	// дательный совпадает с родительным («старшыні Пастаяннай камісіі»): только в скобке
	const bool feminineAdjective = nounCase == Dative ? bracket && EndsWithAny(lower, { L"ай", L"яй", L"ой", L"ей" })
		: EndsWithAny(lower, { L"ую", L"юю" });
	if (feminineHead && feminineAdjective && (!bracket || strSource[posWordEnd] == L')')
		&& CNameConverter::ApplyRules(word, RulesAdjectives, SexAny, lang, nounCase, false))
	{
		strSource.replace(posWord, posWordEnd - posWord, word);
		return posWord + word.length() + (bracket ? 1 : 0);
	}

	// «урача анестэзіёлага-рэаніматолага», «урача псіхіятра-нарколага»: за врачом
	// его специальность через дефис. После других должностей это родительный:
	// «памочніка ўрача-гігіеніста», «намесніка акадэміка-сакратара»
	if (bracket || strSource[posWordEnd] != L'-' || (head != L"урач" && head != L"врач"))
		return posStart;
	std::wstring compound = lower;
	size_t posEnd = posWordEnd;
	while (posEnd < strSource.length() && strSource[posEnd] == L'-')
	{
		const size_t next = strSource.find_first_of(L" ,–)«»-", posEnd + 1);
		const size_t partEnd = next == std::wstring::npos ? strSource.length() : next;
		compound += L"-" + ToLower(strSource.substr(posEnd + 1, partEnd - posEnd - 1));
		posEnd = partEnd;
	}
	std::wstring converted;
	for (size_t from = 0; from <= compound.length(); )
	{
		size_t to = compound.find(L'-', from);
		if (to == std::wstring::npos)
			to = compound.length();
		// первая часть — словарная должность, остальные как в ConvertWordChain
		std::wstring word = compound.substr(from, to - from);
		const WordForm part = findForm(word, lang, nounCase);
		const bool first = converted.empty();
		if (first && (!part.inDictionary || part.declinedLength != word.length() || IsAdjective(part.nominative)))
			return posStart;
		// «анестэзіёлага-рэанімацыйнага аддзялення» — составное прилагательное
		// (не «рэаніматолага»: у существительного перед «-ага» нет «н», «к», «в», «ч»)
		if (!first && EndsWithAny(word, { L"нага", L"кага", L"вага", L"чага", L"ога", L"ого", L"его", L"аму", L"ому", L"ему",
			L"ай", L"ой", L"ей", L"яй", L"ую", L"юю", L"ых", L"іх", L"их" }))
			return posStart;
		if (part.found && part.declinedLength == word.length())
			word = part.nominative;
		else if (!AddNewWord(word, lang, nounCase))
			return posStart;
		if (!first)
			converted += L'-';
		converted += word;
		from = to + 1;
	}
	ReplaceKeepingFirstLetter(strSource, posWord, posEnd - posWord, converted);
	return posWord + converted.length();
}

// вторая должность после запятой: «саліст оперы, вядучага майстра сцэны»,
// «кампазітар, члена Саюза кампазітараў» (не внутри кавычек)
namespace {

bool isConjunctionWord(const std::wstring& word)
{
	return word == L"і" || word == L"и" || word == L"i";
}

// «…, цяпер генеральнага дырэктара …»: связка перед второй должностью
bool isLinkWord(const std::wstring& word)
{
	return isConjunctionWord(word) || word == L"цяпер" || word == L"теперь";
}

} // namespace

bool CProfessionConverter::StartsWithProfession(const std::wstring& strSource, size_t posWord, LangType lang, NounCase nounCase) const
{
	// должность может идти после одного-двух прилагательных: «, галоўнага навуковага супрацоўніка»,
	// после незнакомого слова и «і»: «, каскадзёра і пастаноўшчыка», или после «цяпер»
	for (int w = 0; w < 3; ++w)
	{
		std::wstring next = ToLower(GetNextWord(strSource, posWord));
		if (next.empty())
			return false;
		posWord += next.length();
		if (isLinkWord(next))
			continue;
		// не «механічнай, хіміка-тэрмічнай апрацоўкі»: это составное прилагательное
		bool compoundAdjective = false;
		if (posWord < strSource.length() && strSource[posWord] == L'-')
		{
			size_t posPart = posWord;
			const std::wstring part = ToLower(GetNextWord(strSource, posPart));
			compoundAdjective = EndsWithAny(part, { L"ай", L"ой", L"ых", L"іх", L"их", L"ым", L"ае", L"ое", L"ыя", L"ые" });
		}
		// прилагательное из словаря перед «і»/«и» — часть списка
		// прилагательных, не должность: «грамадзянскага, працоўнага і сацыяльнага
		// заканадаўства» (а «заведующего кафедрой» — одна должность)
		const WordForm found = findForm(next, lang, nounCase);
		if (found.inDictionary && IsAdjective(found.nominative))
		{
			size_t posAfter = posWord;
			if (isConjunctionWord(ToLower(GetNextWord(strSource, posAfter))))
				return false;
		}
		if (found.found && !compoundAdjective)
			return true;
		std::wstring adjective = next;
		if (!CNameConverter::ApplyRules(adjective, RulesAdjectives, SexAny, lang, nounCase, false))
		{
			size_t posAfter = posWord;
			if (!isConjunctionWord(ToLower(GetNextWord(strSource, posAfter))))
				return false;
		}
	}
	return false;
}

// вторая должность после запятой: «саліст оперы, вядучага майстра сцэны»,
// «кампазітар, члена Саюза кампазітараў», «…, цяпер намесніка Міністра»
// (не внутри кавычек). Остальные запятые разбирает вложенный ConvertProfession.
void CProfessionConverter::ConvertAfterComma(std::wstring& strSource, size_t posStart, LangType lang, NounCase nounCase)
{
	for (size_t posComma = strSource.find(L',', posStart); posComma != std::wstring::npos;
		 posComma = strSource.find(L',', posComma + 1))
	{
		if (InsideQuotes(strSource, posComma) || !StartsWithProfession(strSource, posComma + 1, lang, nounCase))
			continue;
		std::wstring rest = strSource.substr(posComma + 1);
		ConvertProfession(rest, lang, nounCase);
		strSource.replace(posComma + 1, std::wstring::npos, rest);
		return;
	}
}

void CProfessionConverter::ConvertAfterConjunction(std::wstring& strSource, size_t posStart, LangType lang, NounCase nounCase)
{
	size_t posWord = posStart;
	const std::wstring next = ToLower(GetNextWord(strSource, posWord));
	// «і» сразу за должностью, без запятой и кавычек между ними
	if (!isConjunctionWord(next) || strSource.find_first_of(L",«»\"", posStart) < posWord)
		return;
	posWord += next.length();
	if (!StartsWithProfession(strSource, posWord, lang, nounCase))
		return;
	std::wstring rest = strSource.substr(posWord);
	ConvertProfession(rest, lang, nounCase);
	strSource.replace(posWord, std::wstring::npos, rest);
}

// слова перед основным, достаточно далеко от начала, — прилагательные:
// «старшага інспектара» → «старшы інспектар»
void CProfessionConverter::ConvertLeadingAdjectives(std::wstring& strSource, size_t posMainWord, LangType lang, NounCase nounCase)
{
	size_t posStart = 0;
	while (posStart < posMainWord)
	{
		size_t posEnd = posStart;
		std::wstring word = GetNextWord(strSource, posStart, &posEnd);
		if (word.empty() || posStart >= posMainWord)
			break;

		const size_t wordLength = word.length();
		if (!CNameConverter::ApplyRules(word, RulesAdjectives, SexAny, lang, nounCase, false)
			    && ToLower(word) == L"научном")
			word = word[0] == L'Н' ? L"Научный" : L"научный";
		if (ToLower(word) != ToLower(strSource.substr(posStart, wordLength)))
		{
			strSource.replace(posStart, wordLength, word);
			// более короткое прилагательное не должно затянуть следующее слово под posMainWord
			if (word.length() >= wordLength) {
				const size_t grown = word.length() - wordLength;
				posEnd += grown;
				posMainWord += grown;
			} else {
				const size_t shrunk = wordLength - word.length();
				posEnd -= shrunk;
				posMainWord -= shrunk;
			}
		}
		posStart = posEnd;
	}
}

namespace {

bool endsWith(const std::wstring& word, const wchar_t* suffix)
{
	const size_t length = std::wcslen(suffix);
	return word.length() >= length && word.compare(word.length() - length, length, suffix) == 0;
}

// Формы, которые общее правило портит, а словарь не лечит.
// Косвенный падеж здесь тот же, что в указе и в каталоге. Именительный
// в словаре («лозапляцельшчык») переписал бы совпавшую строку, а косвенный
// на месте именительного словарь запомнил бы и начал подставлять,
// как раньше «падраздзяленнь».
// «оптыка» уже именительный, но в словаре её нет: окончание «-а» дало бы «оптык».
bool keepCatalogForm(const std::wstring& word)
{
	static const wchar_t* keep[] = {
		L"оптыка",
		L"лозапляцельшчыка",
		L"апрацоўшчыцу",
		L"трубаправодчыка",
		L"фрэзероўшчыцу",
		L"транспарціроўшчыка",
		L"ад’юнкта",
		L"ад'юнкта",
		L"праглядчыка",
		L"экскурсавода",
		L"нумара",
	};
	for (const wchar_t* item : keep)
		if (word == item)
			return true;
	return false;
}

} // namespace

bool CProfessionConverter::ruleNominative(std::wstring word, LangType lang, NounCase nounCase, std::wstring& nominative) const
{
	const std::wstring original = ToLower(word);
	if (original.length() < 4 || keepCatalogForm(original))
		return false;
	// «оптика» уже именительный в словаре: окончание «-а» не отрезаем
	if (languageData(lang).nominatives.find(original) != languageData(lang).nominatives.end())
		return false;
	// «падраздзялення» — родительный среднего рода, не «падраздзяленнь».
	// «іканапісца» уже именительный. «-я» → «-ь» для незнакомого слова
	// даёт «кіруючаь» и «музеь».
	if (endsWith(original, L"ння") || endsWith(original, L"нния") || endsWith(original, L"ения")
		|| endsWith(original, L"енья") || endsWith(original, L"ца")
		|| endsWith(original, L"ая") || endsWith(original, L"яя") || endsWith(original, L"яе"))
		return false;
	if (!CNameConverter::ApplyRules(word, RulesProfession, SexAny, lang, nounCase, false) || word.empty())
		return false;
	if (ToLower(word) == original || endsWith(ToLower(word), L"ь"))
		return false;
	nominative = std::move(word);
	return true;
}

bool CProfessionConverter::restoreUnknownHead(std::wstring& strSource, size_t& posMainWord, LangType lang, NounCase nounCase)
{
	const std::vector<std::wstring> endings = nounCase == Dative
		? std::vector<std::wstring>{ L"му", L"ай", L"яй", L"ом" }
		: std::vector<std::wstring>{ L"га", L"го", L"ую", L"юю", L"ом" };
	const auto adjectiveEnding = [&endings](const std::wstring& candidate) {
		const std::wstring lower = ToLower(candidate);
		if (lower.length() < 4)
			return false;
		const std::wstring ending = lower.substr(lower.length() - 2);
		return std::find(endings.begin(), endings.end(), ending) != endings.end();
	};

	size_t cursor = 0;
	for (int i = 0; i < 5 && cursor < strSource.length(); ++i)
	{
		size_t wordPos = cursor;
		size_t wordEnd = 0;
		const std::wstring word = GetNextWord(strSource, wordPos, &wordEnd);
		if (word.empty())
			return false;
		cursor = wordEnd;
		if (adjectiveEnding(word))
			continue;

		std::wstring nominative;
		if (!ruleNominative(word, lang, nounCase, nominative))
			return false;

		ReplaceKeepingFirstLetter(strSource, wordPos, word.length(), nominative);
		size_t after = wordPos + nominative.length();
		while (after < strSource.length() && (strSource[after] == L'-' || strSource[after] == L'\u2013'))
		{
			size_t partPos = after;
			size_t partEnd = 0;
			const std::wstring part = GetNextWord(strSource, partPos, &partEnd);
			if (part.empty() || partPos != after + 1)
				break;
			std::wstring partNominative;
			if (!ruleNominative(part, lang, nounCase, partNominative))
				break;
			ReplaceKeepingFirstLetter(strSource, partPos, part.length(), partNominative);
			after = partPos + partNominative.length();
		}

		std::wstring offered = word;
		AddNewWord(offered, lang, nounCase);
		posMainWord = wordPos;
		return true;
	}
	return false;
}

namespace {

bool isJobPreposition(const std::wstring& word)
{
	return word == L"па" || word == L"по" || word == L"на" || word == L"у" || word == L"ў"
		|| word == L"в" || word == L"ва" || word == L"з" || word == L"с" || word == L"за"
		|| word == L"да" || word == L"для" || word == L"при" || word == L"пры"
		|| word == L"і" || word == L"и" || word == L"от" || word == L"ад"
		|| word == L"над" || word == L"аб" || word == L"об" || word == L"к" || word == L"ко"
		|| word == L"до" || word == L"из" || word == L"са" || word == L"со" || word == L"во";
}

bool startsWithSenior(const std::wstring& text)
{
	size_t pos = 0;
	const std::wstring word = ToLower(CProfessionConverter::GetNextWord(text, pos));
	return word == L"старшага" || word == L"старшего" || word == L"старшаму" || word == L"старшему";
}

} // namespace

bool CProfessionConverter::convertInitialAdjectives(std::wstring& strSource, LangType lang, NounCase nounCase)
{
	const std::vector<std::wstring> endings = nounCase == Dative
		? std::vector<std::wstring>{ L"му", L"ай", L"яй", L"ом" }
		: std::vector<std::wstring>{ L"га", L"го", L"ую", L"юю", L"ом" };
	const auto adjectiveEnding = [&endings](const std::wstring& candidate) {
		const std::wstring lower = ToLower(candidate);
		if (lower.length() < 4)
			return false;
		const std::wstring ending = lower.substr(lower.length() - 2);
		return std::find(endings.begin(), endings.end(), ending) != endings.end();
	};

	struct Piece { size_t pos; std::wstring word; std::wstring adjusted; };
	std::vector<Piece> pieces;
	size_t pos = 0;
	while (pos < strSource.length())
	{
		size_t wordPos = pos;
		size_t wordEnd = 0;
		const std::wstring word = GetNextWord(strSource, wordPos, &wordEnd);
		if (word.empty() || !adjectiveEnding(word))
			break;
		std::wstring adjusted = word;
		bool rules = CNameConverter::ApplyRules(adjusted, RulesAdjectives, SexAny, lang, nounCase, false);
		if (!rules && ToLower(word) == L"научном")
		{
			adjusted = word[0] == L'Н' ? L"Научный" : L"научный";
			rules = true;
		}
		if (!rules)
			break;
		pieces.push_back({ wordPos, word, adjusted });
		pos = wordEnd;
	}
	size_t nounPos = pos;
	size_t nounEnd = 0;
	const std::wstring noun = GetNextWord(strSource, nounPos, &nounEnd);
	std::wstring nounNominative;
	const bool nounOk = !noun.empty() && ruleNominative(noun, lang, nounCase, nounNominative);
	// голое «філіяла» и одно «адасобленага падраздзялення» не должность;
	// «старшага па корпусу» — да: после прилагательного сразу предлог
	const std::wstring firstAdjective = pieces.empty() ? std::wstring() : ToLower(pieces.front().word);
	const bool seniorBeforePreposition = pieces.size() == 1 && !noun.empty()
		&& isJobPreposition(ToLower(noun))
		&& (firstAdjective == L"старшага" || firstAdjective == L"старшего"
			|| firstAdjective == L"старшаму" || firstAdjective == L"старшему");
	if (pieces.empty() || (pieces.size() < 2 && !nounOk && !seniorBeforePreposition))
		return false;

	for (auto piece = pieces.rbegin(); piece != pieces.rend(); ++piece)
		strSource.replace(piece->pos, piece->word.length(), piece->adjusted);
	if (nounOk)
	{
		const size_t shift = [&] {
			size_t delta = 0;
			for (const Piece& piece : pieces)
				delta += piece.word.length() - piece.adjusted.length();
			return delta;
		}();
		ReplaceKeepingFirstLetter(strSource, nounPos - shift, noun.length(), nounNominative);
	}
	return true;
}

bool CProfessionConverter::ConvertProfession(std::wstring& strSource, LangType lang, NounCase nounCase, bool allowUnknownHead)
{
	bool bResult = false;

	// основное слово ищется среди первых слов. Предлог не занимает место только
	// после «старшага»: иначе «дэпартамента» в конце длинной должности становится
	// её главным словом. «следчага» как раз за таким предлогом.
	const int maxProfessionWords = 5;
	size_t posStart = 0, posMainWord = 0;
	const bool skipPrepositions = startsWithSenior(strSource);
	for (int i = 0; i < maxProfessionWords && posStart < strSource.length(); )
	{
		size_t posEnd = posStart;
		const std::wstring word = ToLower(GetNextWord(strSource, posStart, &posEnd));
		if (word.empty())
			break;
		if (skipPrepositions && isJobPreposition(word))
		{
			posStart = posEnd;
			continue;
		}
		++i;

		// «исполняющего обязанности начальника»: должность — прилагательное
		// перед этим, остальное родительный (приводится как начальные прилагательные)
		if (i > 1 && (word == L"обязанности" || word == L"абавязкі" || word == L"абавязкi"))
		{
			bResult = true;
			posMainWord = posStart;
			break;
		}

		const WordForm form = findForm(word, lang, nounCase);
		if (form.found)
		{
			bResult = true;
			posMainWord = posStart;
			posStart = ConvertWordChain(strSource, posStart, form.declinedLength, form.nominative, lang, nounCase);
			posStart = ConvertTrailingWord(strSource, posStart, lang, nounCase);
			posStart = ConvertBracketed(strSource, posStart, lang, nounCase);
			ConvertAfterConjunction(strSource, posStart, lang, nounCase);
			ConvertAfterComma(strSource, posStart, lang, nounCase);
			break;
		}

		posStart = posEnd;
	}

	if (!bResult && allowUnknownHead)
		bResult = restoreUnknownHead(strSource, posMainWord, lang, nounCase);
	// «старшего пожарного пожарной части»; после тире только «старшага паветранага радыста»,
	// не «філіяла» и не «Баранавіцкага станкабудаўнічага завода»
	if (!bResult && (allowUnknownHead || startsWithSenior(strSource)))
		bResult = convertInitialAdjectives(strSource, lang, nounCase);
	if (!bResult)
	{
		FindNewWords(strSource, lang, nounCase);
		// «работніцы …, майстру спорту»: первая должность незнакома, вторая — нет
		ConvertAfterComma(strSource, 0, lang, nounCase);
	}

	// у основного слова в самом начале прилагательных перед ним нет
	const size_t minAdjectivesLength = 5;
	if (posMainWord >= minAdjectivesLength)
		ConvertLeadingAdjectives(strSource, posMainWord, lang, nounCase);

	// «начальнік … (статс-сакратара)»: голова уже именительная, скобка далеко,
	// обычное окно её не берёт. Каталог здесь именительный.
	const std::pair<std::wstring, std::wstring> secretaries[] = {
		{ L"(статс-сакратара)", L"(статс-сакратар)" },
		{ L"(статс-сакратару)", L"(статс-сакратар)" },
	};
	for (const auto& form : secretaries)
	{
		size_t pos = 0;
		while ((pos = strSource.find(form.first, pos)) != std::wstring::npos)
		{
			strSource.replace(pos, form.first.length(), form.second);
			pos += form.second.length();
			bResult = true;
		}
	}

	SeparateLeadingDegree(strSource, lang, nounCase);
	SeparateTrailingTitle(strSource, lang, nounCase);
	return bResult;
}

namespace {

// именительные званий, которые указ ставит перед именем без запятой
bool isTitleNominative(const std::wstring& word)
{
	static const wchar_t* titles[] = {
		L"профессор", L"академик", L"композитор", L"доцент", L"член-корреспондент",
		L"прафесар", L"акадэмік", L"кампазітар", L"дацэнт", L"член-карэспандэнт",
	};
	for (const wchar_t* title : titles)
	{
		if (word == title)
			return true;
	}
	return false;
}

bool isDegreeNominative(const std::wstring& word)
{
	return word == L"доктор" || word == L"кандидат" || word == L"доктар" || word == L"кандыдат";
}

bool isSciencesWord(const std::wstring& word)
{
	return word == L"наук" || word == L"навук";
}

// слова и их начала: «доктару медыцынскіх навук» → {доктару, медыцынскіх, навук}
std::vector<std::pair<size_t, std::wstring>> splitWords(const std::wstring& text)
{
	std::vector<std::pair<size_t, std::wstring>> words;
	size_t pos = 0;
	while (pos < text.length())
	{
		pos = text.find_first_not_of(L' ', pos);
		if (pos == std::wstring::npos)
			break;
		const size_t end = std::min(text.find(L' ', pos), text.length());
		words.push_back({ pos, text.substr(pos, end - pos) });
		pos = end;
	}
	return words;
}

} // namespace

// «…університэта профессору», «…акадэміі навук Беларусі акадэміка», «…інстытута доктару
// медыцынскіх навук»: указ ставит звание перед именем без запятой, каталог отделяет
// его запятой и хранит в именительном
void CProfessionConverter::SeparateTrailingTitle(std::wstring& strSource, LangType lang, NounCase nounCase)
{
	const auto words = splitWords(strSource);
	if (words.size() < 3)
		return;
	// звание — последнее слово, учёная степень — три последних («доктару медыцынскіх навук»)
	size_t titleIndex = words.size() - 1;
	if (isSciencesWord(ToLower(words.back().second)))
		titleIndex = words.size() - 3;
	if (titleIndex < 2)
		return;
	const std::wstring& before = words[titleIndex - 1].second;
	if (before.empty() || before.back() == L',' || before == L"–" || before == L"-")
		return;
	const std::wstring title = ToLower(words[titleIndex].second);
	const WordForm form = findForm(title, lang, nounCase);
	if (!form.found)
		return;
	const bool degree = titleIndex == words.size() - 3 && isDegreeNominative(form.nominative);
	if (!degree && !(titleIndex == words.size() - 1 && isTitleNominative(form.nominative)))
		return;
	const size_t pos = words[titleIndex].first;
	ReplaceKeepingFirstLetter(strSource, pos, form.declinedLength, form.nominative);
	strSource.insert(pos - 1, L",");
}

// «доктар тэхнічных навук прарэктару па вучэбнай рабоце»: степень в начале, а должность
// за ней осталась в косвенном падеже; каталог пишет «доктар тэхнічных навук, прарэктар …»
void CProfessionConverter::SeparateLeadingDegree(std::wstring& strSource, LangType lang, NounCase nounCase)
{
	const auto words = splitWords(strSource);
	if (words.size() < 4 || !isDegreeNominative(ToLower(words[0].second)) || !isSciencesWord(ToLower(words[2].second)))
		return;
	const size_t posJob = words[3].first;
	if (!findForm(ToLower(words[3].second), lang, nounCase).found)
		return;
	std::wstring rest = strSource.substr(posJob);
	ConvertProfession(rest, lang, nounCase);
	strSource.replace(posJob, std::wstring::npos, rest);
	strSource.insert(posJob - 1, L",");
}

void CProfessionConverter::FindNewWords(std::wstring& strSource, LangType lang, NounCase nounCase)
{
	if (strSource.empty())
		return;

	// окончания существительного по падежу
	std::vector<std::wstring> strEndings;
	if (nounCase == Dative)
		strEndings = { L"му",L"ай", L"яй" };
	else
		strEndings = { L"га", L"го" ,L"ую", L"юю" };

	// первым словом может быть новая должность. Начальная цепочка прилагательных — нет:
	// «вялікаму сіняму новаму брынзалу» предлагает «брынзалу», а не «сіняму»
	size_t posStart = 0;
	size_t posEnd = 0;
	std::wstring word = GetNextWord(strSource, posStart, &posEnd);
	if (word.length() <= 2)
		return;

	const auto adjectiveEnding = [&strEndings](const std::wstring& candidate) {
		if (candidate.length() < 2)
			return false;
		const std::wstring ending = candidate.substr(candidate.length() - 2);
		return std::find(strEndings.begin(), strEndings.end(), ending) != strEndings.end();
	};
	while (adjectiveEnding(word))
	{
		posStart = posEnd;
		if (posStart >= strSource.length())
			return;
		word = GetNextWord(strSource, posStart, &posEnd);
		if (word.empty())
			return;
	}

	const std::wstring lower = ToLower(word);
	if (endsWith(lower, L"ння") || endsWith(lower, L"нния") || endsWith(lower, L"ения") || endsWith(lower, L"енья"))
		return;
	AddNewWord(word, lang, nounCase);
}

bool CProfessionConverter::AddNewWord(std::wstring& word, LangType lang, NounCase nounCase)
{
	// ключи словаря в нижнем регистре (поиск понижает слово), вызывающие оставляют
	// исходную первую букву при подстановке
	word = ToLower(word);

	NewDictItem item;
	item.Original = word;
	item.Language = lang;
	item.Case = nounCase;
	item.Checked = true;
	// однобуквенное слово стало бы пустым («у» → «»): это не новое слово
	if (CNameConverter::ApplyRules(word, RulesProfession, SexAny, lang, nounCase, false) && !word.empty())
	{
		// другой падеж уже хранит этот именительный: подставляем, не предлагаем
		const auto& known = languageData(lang).nominatives;
		if (known.find(word) != known.end())
			return true;

		for (size_t i = 0; i < m_NewWords.size(); i++)
		{
			if (m_NewWords[i].Original == item.Original)	// уже есть
				return true;
		}

		item.Word = word;
		m_NewWords.push_back(item);
		return true;
	}

	return false;
}

