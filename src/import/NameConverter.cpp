#include "NameConverter.h"
#include "TextUtil.h"

#include <algorithm>

// restores nominative case of names

RulesList CNameConverter::ExceptionsLastName = {
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
};

RulesList CNameConverter::RulesLastName = {
	{ SexAny,	LangBY, L"дзе",			L"",			L"--а"},	// Калядзе
	{ SexAny,	LangAny,L"е",			L"у",			L"-а"},
	{ SexFemale,LangAny,L"кай,кой",		L"",			L"--ая"},	// Романской
	{ SexFemale,LangBY,	L"іной,ыной",	L"",			L"--а"},	// Кузьміной
	{ SexFemale,LangBY,	L"ой",			L"",			L"--ая?"},	// Лясной
	{ SexFemale,LangAny,L"ай,ой",		L"",			L"--а"},	// Никитиной
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
	{ SexAny,	LangBY, L"о,е,э",		L"о,е,э,",		L"" },
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
};

RulesList CNameConverter::ExceptionsFirstName = {
	{ SexMale, LangRU, L"льву", L"льва", L"---ев" },
	{ SexMale, LangBY, L"льву", L"льва", L"---еў" },
	{ SexMale, LangRU, L"павлу", L"павла", L"--ел" },
	{ SexMale, LangBY, L"паўлу", L"паўла", L"---вел" },
	{ SexMale, LangBY, L"пятру", L"пятра", L"----ётр" },
	{ SexMale, LangRU, L"шота", L"шота", L"" },
	{ SexMale, LangAny, L"роберту,альберту,норберту,герберту", L"", L"-" },
	{ SexMale, LangBY, L"францішку", L"францішка", L"--ак" }
};

RulesList CNameConverter::RulesFirstName = {
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
	{ SexFemale,LangBY, L"вi,ві",		L"",		L"--ў" },	// Любовi
	{ SexFemale,LangBY, L"аву",			L"ава",		L"--ў" },
	{ SexMale,  LangBY, L"ту",			L"ту",		L"-а" },
	{ SexMale,  LangBY, L"аву",			L"ава",		L"--ў" },
	{ SexMale,  LangBY, L"дзію,дзiю",	L"дзія,дзiя", L"--ь" },
	{ SexMale,  LangAny,L"аю,ею,ию,ою,ію,ыю,эю,ію,iю", L"ая,ея,ия,оя,ія,ыя,эя,ія,iя", L"-й" },
	{ SexMale,  LangAny,L"ю",			L"я",		L"-ь" },	// Рамиль
	{ SexMale,  LangAny,L"е,ё,и,і,о,ы,э", L"е,ё,и,і,о,у,ы,э,ю", L"" },
	{ SexMale,  LangRU,	L"ге,ке,хе,че,ще,же",L"гу,ку,ху,чу,щу,жу", L"-а" },
	{ SexFemale,LangAny,L"б,в,г,д,ж,з,й,к,л,м,н,п,р,с,т,ў,ф,х,ц,ч,ш,щ,ъ,ь", L"б,в,г,д,ж,з,й,к,л,м,н,п,р,с,т,ў,ф,х,ц,ч,ш,щ,ъ,ь", L"" },
	{ SexMale,  LangAny,L"бу,ву,гу,ду,жу,зу,ку,лу,му,ну,пу,ру,су,ту,фу,ху,цу,чу,шу", L"ба,ва,га,да,жа,за,ка,ла,ма,на,па,ра,са,та,фа,ха,ца,ча,ша", L"-" },
};

RulesList CNameConverter::RulesMiddleName = {
	{ SexMale, LangRU, L"ичу", L"ича", L"-" },
	{ SexMale, LangBY, L"ічу,iчу", L"іча,iча", L"-" },
	{ SexFemale, LangAny, L"не", L"ну", L"-а" },
};

bool CNameConverter::m_bIsInitialized = false;

CNameConverter::CNameConverter()
{
	if (!m_bIsInitialized)
	{
		ExpandRules(ExceptionsFirstName);
		ExpandRules(ExceptionsLastName);
		ExpandRules(RulesFirstName);
		ExpandRules(RulesLastName);
		ExpandRules(RulesMiddleName);
		m_bIsInitialized = true;
	}
};

void CNameConverter::ExpandRules(RulesList& rules)
{
	for (size_t r = 0; r < rules.size(); r++)
	{
		if (rules[r].SuffixAcc.find(',') == -1 && rules[r].SuffixDat.find(',') == -1)
			continue;

		std::vector<std::wstring> strListAcc = SplitString(rules[r].SuffixAcc, L',');
		std::vector<std::wstring> strListDat = SplitString(rules[r].SuffixDat, L',');

		Rule rule = rules[r];
		rules.erase(rules.begin() + r);

		for (size_t i = 0; i < std::max(strListDat.size(), strListAcc.size()); i++)
		{
			rule.SuffixAcc = (i < strListAcc.size() ? strListAcc[i] : L"");
			rule.SuffixDat = (i < strListDat.size() ? strListDat[i] : L"");
			rules.insert(rules.begin() + r++, rule);
		}

		r--;
	}
}

bool CNameConverter::ApplyRules(std::wstring& name, RulesList& rules, SexType sex, LangType lang, NounCase nounCase, bool bFullWordMatch)
{
	if (lang == LangBY)
	{
		// replace Latin letters with Belorussian ("Iгар" is often typed with Latin I;
		// duplicates are found alphabet-blind, see Catalog::IdentityKey)
		std::replace(name.begin(), name.end(), L'i', L'і');
		std::replace(name.begin(), name.end(), L'I', L'І');
	}

	const std::wstring nameLower = ToLower(name);

	for (const auto& rule : rules)
	{
		if (rule.Match(nameLower, sex, lang, nounCase, bFullWordMatch))
			return rule.Apply(name);
	}

	return false;
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
	if (pos1 == -1)
		return false;
	lastName = fullName.substr(0, pos1);
	StrTrim(lastName);

	if (pos2 != -1)
	{
		middleName = fullName.substr(pos2 + 1);
		firstName = fullName.substr(pos1 + 1, pos2 - pos1 - 1);
	}
	else
		firstName = fullName.substr(pos1 + 1);
	StrTrim(firstName);

	SexType sex = SexMale;	// male by default
	if (!middleName.empty())
	{
		sex = GetSex(middleName, nounCase);
		ApplyRules(middleName, RulesMiddleName, sex, lang, nounCase, false);
	}
	else
	{
		std::swap(firstName, lastName);
	}

	bool bResFN = ApplyRules(firstName, ExceptionsFirstName, sex, lang, nounCase, true);
	if (!bResFN)
		bResFN = ApplyRules(firstName, RulesFirstName, sex, lang, nounCase, false);

	bool bResLN = ApplyRules(lastName, ExceptionsLastName, sex, lang, nounCase, true);
	if (!bResLN)
	{
		bResLN = ApplyRules(lastName, RulesLastName, sex, lang, nounCase, false);
		if (!lastName.empty() && lastName.back() == '?')
		{
			lastName.pop_back();
			bResLN = false;
		}
	}

	if(middleName.empty())
		fullName = firstName + L' ' + lastName;
	else
		fullName = lastName + L' ' + firstName + L' ' + middleName;

	if (!strAlias.empty())
		fullName += L" (" + strAlias + L")";

	return bResFN && bResLN;
}

SexType CNameConverter::GetSex(std::wstring& middleName, NounCase nounCase)
{
	if (middleName.length() > 2 && middleName.substr(middleName.length() - 2) == (nounCase == Dative? L"не" : L"ну"))
		return SexFemale;

	return SexMale;
}