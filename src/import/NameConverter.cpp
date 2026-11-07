#include "NameConverter.h"
#include "TextUtil.h"

#include <algorithm>

// restores nominative case of names

const RulesList CNameConverter::ExceptionsLastName = ExpandRules({
	{ SexMale, LangRU, L"вию,сою,цою,хою", L"вия,соя,цоя,хоя", L"-й" },
	{ SexMale, LangBY, L"дразду", L"дразда", L"----озд" },
	{ SexMale, LangBY, L"каралю", L"караля", L"---оль" },
	{ SexMale, LangBY, L"ваўку", L"ваўка", L"----оўк" },
	{ SexMale, LangBY, L"кату,крату", L"ката,крата", L"---от" },
	{ SexMale, LangBY, L"дзятлу", L"дзятла", L"---цел" },
	{ SexMale, LangBY, L"рогу", L"рога", L"-" },
	// fleeting vowels the general rules cannot restore
	{ SexMale, LangAny, L"зайцу", L"зайца", L"---яц" },
	{ SexMale, LangAny, L"буслу", L"бусла", L"--ел" },
	{ SexMale, LangBY, L"бугру", L"бугра", L"--ор" },
	{ SexMale, LangBY, L"арлу", L"арла", L"--ол" },
	{ SexMale, LangBY, L"бубну", L"бубна", L"--ен" },
	{ SexMale, LangBY, L"піўню", L"піўня", L"---вень" },
	{ SexMale, LangBY, L"полудню", L"полудня", L"---дзень" },
	{ SexMale, LangBY, L"баклажцу", L"баклажца", L"--ац" },
	{ SexMale, LangBY, L"забалотцу", L"забалотца", L"---цец" },
	{ SexMale, LangBY, L"маршалку,сідарку", L"маршалка,сідарка", L"--ок" },
	{ SexMale, LangBY, L"турку,запатылку", L"турка,запатылка", L"--ак" },
	{ SexMale, LangBY, L"кляшчу", L"кляшча", L"----ешч" },
	{ SexMale, LangBY, L"хвашчу", L"хвашча", L"----ошч" },
	{ SexMale, LangBY, L"лукашэню", L"лукашэню", L"-я" }
});

const RulesList CNameConverter::RulesLastName = ExpandRules({
	{ SexAny,	LangBY, L"дзе",			L"",			L"--а"},	// Калядзе
	{ SexAny,	LangAny,L"е",			L"у",			L"-а"},
	{ SexFemale,LangAny,L"кай,кой",		L"",			L"--ая"},	// Романской
	{ SexFemale,LangBY,	L"іной,ыной",	L"",			L"--а"},	// Кузьміной
	{ SexFemale,LangBY,	L"ой",			L"",			L"--ая?"},	// Лясной
	{ SexFemale,LangAny,L"ай,ой",		L"",			L"--а"},	// Никитиной
	{ SexFemale,LangAny,L"бей",			L"",			L""},		// Воробей: before "-ей"
	{ SexFemale,LangAny,L"ей",			L"ую",			L"--ая"},
	{ SexFemale,LangBY, L"а",			L"а",			L"" },
	{ SexFemale,LangBY, L"ня",			L"ня",			L"" },
	{ SexFemale,LangAny,L"бей",			L"бей",			L""},
	{ SexAny,	LangAny,L"і",			L"ю",			L"-я" },	// Кривеню, Крывеню
	{ SexMale,	LangRU, L"кому",		L"кого",		L"---ий" },
	{ SexMale,	LangBY, L"хаму,каму",	L"хага,кага",	L"---і" },
	{ SexMale,	LangRU, L"ому",			L"ого",			L"---ый?" },
	{ SexMale,	LangBY, L"рому",		L"",			L"-" },	   // Прыстрому
	{ SexMale,	LangBY, L"гаму,гому,кому,хому,няму", L"гага,гога,кога,хога,няга", L"---і" }, // Доўгі, Лугаўскі, Загародні
	{ SexMale,	LangBY, L"аму,ому",		L"ага,ога",		L"---ы" },
	{ SexMale,	LangRU, L"ему",			L"его",			L"---ий" },
	{ SexMale,	LangRU, L"иноя",		L"иноя",		L"-й"},
	{ SexMale,	LangAny,L"ую,ею,аю,ою,эю,ыю,iю,ію,яю", L"уя,ея,ая,оя,эя,ыя,iя,ія,яя", L"-й"},
	{ SexMale,	LangRU, L"ию",			L"",			L"-й"},		// Виолентию
	{ SexMale,	LangAny,L"йцу,льцу",	L"йца,льца",	L"---ец" },
	{ SexMale,	LangBY, L"рцу",			L"рца",			L"--эц" },	// Азярэц: "р" is always hard in Belarusian
	{ SexMale,	LangAny,L"пцу,бцу,вцу,нцу,мцу,рцу", L"пца,бца,вца,нца,мца,рца", L"--ец" },
	{ SexMale,	LangRU, L"енку,ёнку",	L"енка,ёнка",	L"--ок" },
	{ SexMale,	LangBY, L"онку,ёнку",	L"онка,ёнка",	L"--ак" },
	{ SexMale,	LangBY, L"анку,енку,энку",	L"",		L"-а" },// Краўчанку, Лукашэнку, Гардзеенку
	{ SexMale,	LangBY, L"чку,тку,пку,бку,нку", L"чка,тка,пка,бка,нка", L"--ок" },
	{ SexMale,	LangBY, L"",			L"шка,зка,жка,дка", L"--ок" },	// Мяшок, Палазок (dative "-шку" is mostly "-шка")
	{ SexMale,	LangBY, L"",			L"лька,нька,йка", L"---ёк" },	// Каралёк, Канёк, Кіёк
	{ SexMale,	LangAny,L"аку,іку,уку,яку,ыку,юку",	L"",L"-" },		// Климчуку, Антончыку, Прасняку, Бібіку, Манчаку, Панасюку
	{ SexMale,	LangBY, L"ўцу",			L"ўца",			L"---вец"}, // Грыцавец
	{ SexMale,	LangBY, L"цу,лу,ту,ку,пу", L"",		L"-а" },	// Кухту, Жыгалу ("-ру" is mostly "-р": Косару)
	{ SexAny,	LangAny,L"те",			L"",			L"-а" },	// Федуте
	{ SexAny,	LangAny,L"це",			L"",			L"--та" },	// Федуце
	{ SexAny,	LangRU, L"о",			L"о",			L"" },
	{ SexAny,	LangBY, L"о,е,э",		L"о,е,э",		L"" },
	{ SexAny,	LangBY, L"ы",			L"",			L"-а" },    // Рагойшы
	{ SexAny,	LangRU, L"ия",			L"ия",			L"" },
	{ SexAny,	LangRU, L"их,ых",		L"их,ых",		L"" },
	{ SexMale,	LangRU, L"ью",			L"ья",			L"--ей" },
	{ SexMale,	LangBY, L"'ю,’ю",		L"'я,’я",		L"--ей" },
	{ SexMale,	LangBY, L"ўю",			L"ўя",			L"--вей" },
	{ SexMale,	LangAny,L"ю",			L"я",			L"-ь" },
	{ SexMale,	LangBY,	L"аву,ову,ёву,еву", L"ава,ова,ёва,ева", L"--ў" },
	{ SexFemale,LangAny,L"б,в,г,д,ж,з,й,к,л,м,н,п,р,с,т,ф,х,ц,ч,ш,щ,ъ,ь", L"б,в,г,д,ж,з,й,к,л,м,н,п,р,с,т,ф,х,ц,ч,ш,щ,ъ,ь", L""},
	{ SexMale,	LangAny,L"бу,ву,гу,ду,жу,зу,ку,лу,му,ну,пу,ру,су,ту,фу,ху,цу,чу,шу,щу", L"ба,ва,га,да,жа,за,ка,ла,ма,на,па,ра,са,та,фа,ха,ца,ча,ша,ща", L"-" }
});

const RulesList CNameConverter::ExceptionsFirstName = ExpandRules({
	{ SexMale, LangRU, L"льву", L"льва", L"---ев" },
	{ SexMale, LangBY, L"льву", L"льва", L"---еў" },
	{ SexMale, LangRU, L"павлу", L"павла", L"--ел" },
	{ SexMale, LangBY, L"паўлу", L"паўла", L"---вел" },
	{ SexMale, LangBY, L"пятру", L"пятра", L"----ётр" },
	{ SexMale, LangRU, L"шота", L"шота", L"" },
	{ SexMale, LangAny, L"роберту,альберту,норберту,герберту", L"", L"-" },
	{ SexMale, LangBY, L"францішку", L"францішка", L"--ак" }
});

const RulesList CNameConverter::RulesFirstName = ExpandRules({
	{ SexAny,	LangAny, L"",			L"ю",		L"-я" },	// Наталью, Илью
	{ SexAny,	LangRU, L"ье,ое",		L"",		L"-я" },	// Наталье, Зое
	{ SexFemale,LangRU, L"ии",			L"",		L"-я" },	// Марии, Алиции
	// Latin "i" is already replaced with "і" here (ApplyRules)
	{ SexAny,	LangBY, L"еі,оі,ьі,лі,іі,ыі,сі,’і,'і",L"", L"-я" },	// Наталлi, Надзеi, Зоі, Алесі, Соф’і
	{ SexFemale,LangBY, L"дзе",			L"",		L"--а" },	// Зинаидзе
	{ SexFemale,LangBY, L"зе",			L"",		L"--га" },	// Ядвизе
	{ SexFemale,LangBY, L"це",			L"",		L"--та" },	// Маргарыце
	{ SexFemale,LangAny,L"е",			L"у",		L"-а" },	// Галина
	{ SexFemale,LangBY, L"цы",			L"",		L"--ка" },	// Анжалицы
	{ SexFemale,LangBY, L"ы",			L"",		L"-а" },	// Аляксандры
	{ SexFemale,LangBY, L"ві",			L"",		L"--ў" },	// Любові
	{ SexFemale,LangBY, L"аву",			L"ава",		L"--ў" },
	{ SexMale,  LangBY, L"ту",			L"ту",		L"-а" },
	{ SexMale,  LangBY, L"аву",			L"ава",		L"--ў" },
	{ SexMale,  LangBY, L"дзію",		L"дзія",	L"--ь" },
	{ SexMale,  LangAny,L"аю,ею,ию,ою,ію,ыю,эю,iю", L"ая,ея,ия,оя,ія,ыя,эя,iя", L"-й" },
	{ SexMale,  LangAny,L"ю",			L"я",		L"-ь" },	// Рамиль
	{ SexMale,  LangAny,L"е,ё,и,і,о,ы,э", L"е,ё,и,і,о,у,ы,э,ю", L"" },
	{ SexMale,  LangRU,	L"ге,ке,хе,че,ще,же",L"гу,ку,ху,чу,щу,жу", L"-а" },
	{ SexFemale,LangAny,L"б,в,г,д,ж,з,й,к,л,м,н,п,р,с,т,ў,ф,х,ц,ч,ш,щ,ъ,ь", L"б,в,г,д,ж,з,й,к,л,м,н,п,р,с,т,ў,ф,х,ц,ч,ш,щ,ъ,ь", L"" },
	{ SexMale,  LangAny,L"бу,ву,гу,ду,жу,зу,ку,лу,му,ну,пу,ру,су,ту,фу,ху,цу,чу,шу", L"ба,ва,га,да,жа,за,ка,ла,ма,на,па,ра,са,та,фа,ха,ца,ча,ша", L"-" },
});

const RulesList CNameConverter::RulesMiddleName = ExpandRules({
	{ SexMale, LangRU, L"ичу", L"ича", L"-" },
	{ SexMale, LangBY, L"ічу,iчу", L"іча,iча", L"-" },
	{ SexFemale, LangAny, L"не", L"ну", L"-а" },
});

RulesList CNameConverter::ExpandRules(const RulesList& rules)
{
	RulesList expanded;
	expanded.reserve(rules.size());
	for (const Rule& rule : rules)
	{
		if (rule.SuffixAcc.find(L',') == std::wstring::npos && rule.SuffixDat.find(L',') == std::wstring::npos)
		{
			expanded.push_back(rule);
			continue;
		}
		const std::vector<std::wstring> acc = SplitString(rule.SuffixAcc, L',');
		const std::vector<std::wstring> dat = SplitString(rule.SuffixDat, L',');
		for (size_t i = 0; i < std::max(dat.size(), acc.size()); i++)
		{
			Rule part = rule;
			part.SuffixAcc = i < acc.size() ? acc[i] : std::wstring();
			part.SuffixDat = i < dat.size() ? dat[i] : std::wstring();
			expanded.push_back(part);
		}
	}
	return expanded;
}

bool CNameConverter::ApplyRules(std::wstring& name, const RulesList& rules, SexType sex, LangType lang, NounCase nounCase, bool bFullWordMatch)
{
	if (lang == LangBY)
	{
		// replace Latin letters with Belorussian ("Iгар" is often typed with Latin I;
		// duplicates are found alphabet-blind, see Catalog::IdentityKey)
		ToCyrillicI(name, true);
	}

	const std::wstring nameLower = ToLower(name);

	// a full word (the exception tables) keeps the first match. An ordinary ending
	// keeps the longest one, so "Федуце" is "це", not the earlier "е". Equal length
	// stays with the earlier rule ("ому" is "ы" in Belarusian and "ый" in Russian).
	const Rule* matched = nullptr;
	size_t matchedLength = 0;
	for (const auto& rule : rules)
	{
		if (!rule.Match(nameLower, sex, lang, nounCase, bFullWordMatch))
			continue;
		if (bFullWordMatch)
		{
			matched = &rule;
			break;
		}
		const std::wstring& suffix = nounCase == Dative ? rule.SuffixDat : rule.SuffixAcc;
		if (!matched || suffix.length() > matchedLength)
		{
			matched = &rule;
			matchedLength = suffix.length();
		}
	}
	if (!matched || !matched->Apply(name))
		return false;
	return !matched->Uncertain;
}

// applies convert to every part of a hyphenated word; true if all parts were converted
bool CNameConverter::ForEachPart(std::wstring& word, const std::function<bool(std::wstring&)>& convert)
{
	if (word.find(L'-') == std::wstring::npos)
		return convert(word);

	bool bRes = true;
	std::wstring result;
	const std::vector<std::wstring> parts = SplitString(word, L'-'); // keeps empty parts
	for (size_t i = 0; i < parts.size(); i++)
	{
		std::wstring part = parts[i];
		if (!part.empty() && !convert(part))
			bRes = false;
		if (i > 0)
			result += L'-';
		result += part;
	}
	word = result;
	return bRes;
}

bool CNameConverter::IsMiddleName(const std::wstring& name, LangType lang, NounCase nounCase)
{
	const std::wstring nameLower = ToLower(name);
	for (const auto& rule : RulesMiddleName)
	{
		if (rule.Match(nameLower, SexMale, lang, nounCase, false) || rule.Match(nameLower, SexFemale, lang, nounCase, false))
			return true;
	}

	return false;
}

bool CNameConverter::NormalizeName(std::wstring& fullName, LangType lang, NounCase nounCase)
{
	bool bRes = false;

	// check for alias
	std::wstring strAlias;
	size_t posBracket2 = fullName.rfind(L')');
	size_t posBracket1 = fullName.rfind(L'(', posBracket2);
	if (posBracket2 != std::wstring::npos && posBracket1 != std::wstring::npos && posBracket1 < posBracket2)
	{
		strAlias = fullName.substr(posBracket1 + 1, posBracket2 - posBracket1 - 1);
		NormalizeName(strAlias, lang, nounCase);
		// remove " (alias)" together with the leading space
		const size_t eraseFrom = posBracket1 > 0 ? posBracket1 - 1 : 0;
		fullName.erase(eraseFrom, posBracket2 + 1 - eraseFrom);
	}

	std::wstring firstName, lastName, middleName;
	size_t pos1 = fullName.find(' ');
	size_t pos2 = fullName.find(' ', pos1 + 1);
	if (pos1 == std::wstring::npos)
		return false;
	lastName = fullName.substr(0, pos1);
	StrTrim(lastName);

	if (pos2 != std::wstring::npos)
	{
		middleName = fullName.substr(pos2 + 1);
		firstName = fullName.substr(pos1 + 1, pos2 - pos1 - 1);
	}
	else
		firstName = fullName.substr(pos1 + 1);
	StrTrim(firstName);

	// an unrecognized patronymic is a failed name: the import window marks NF_NAME
	bool middleNameOk = true;
	SexType sex = SexMale;	// male by default
	if (!middleName.empty())
	{
		sex = GetSex(middleName, nounCase);
		// double patronymic: "Гедзімінасавіча-Альбертасавіча"
		middleNameOk = ForEachPart(middleName, [&](std::wstring& part) {
			return ApplyRules(part, RulesMiddleName, sex, lang, nounCase, false);
		});
	}
	else
	{
		std::swap(firstName, lastName);
	}

	// double first name: "Хайнца-Рудольфа"
	bool bResFN = ForEachPart(firstName, [&](std::wstring& part) {
		return ApplyRules(part, ExceptionsFirstName, sex, lang, nounCase, true)
			|| ApplyRules(part, RulesFirstName, sex, lang, nounCase, false);
	});

	// double surname: each part is declined, "Карпінскую-Пешка", "Денисовой-Клебанович"
	bool bResLN = ForEachPart(lastName, [&](std::wstring& part) {
		bool bRes = ApplyRules(part, ExceptionsLastName, sex, lang, nounCase, true);
		if (!bRes)
			bRes = ApplyRules(part, RulesLastName, sex, lang, nounCase, false);
		return bRes;
	});

	if(middleName.empty())
		fullName = firstName + L' ' + lastName;
	else
		fullName = lastName + L' ' + firstName + L' ' + middleName;

	if (!strAlias.empty())
		fullName += L" (" + strAlias + L")";

	return bResFN && bResLN && middleNameOk;
}

SexType CNameConverter::GetSex(std::wstring& middleName, NounCase nounCase)
{
	if (middleName.length() > 2)
	{
		const std::wstring ending = ToLower(middleName.substr(middleName.length() - 2));
		if (ending == (nounCase == Dative ? L"не" : L"ну"))
			return SexFemale;
	}

	return SexMale;
}
