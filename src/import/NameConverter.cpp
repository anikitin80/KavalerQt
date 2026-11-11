#include "NameConverter.h"
#include "TextUtil.h"

#include <algorithm>
#include <cwchar>

// приводит имена к именительному падежу

const RulesList CNameConverter::ExceptionsLastName = ExpandRules({
	// общее «-ой» даёт «Окружна», в каталоге «Окружная»
	{ SexFemale, LangRU, L"окружной", L"окружную", L"--ая" },
	// «-у» общего правила съедает гласную: Лесечк, Ханк, Гребнь
	{ SexMale, LangRU, L"лесечку", L"лесечка", L"-о" },
	{ SexMale, LangRU, L"ханку", L"ханка", L"--ок" },
	{ SexMale, LangRU, L"гребню", L"гребня", L"--ень" },
	// дательный «Волчку» — Волчок; винительный «Волчка» в каталоге остаётся Волчк
	{ SexMale, LangRU, L"волчку", L"", L"--ок" },
	// несклоняемое: общее «-е» превращает в «Брауда»
	{ SexAny, LangRU, L"брауде", L"брауде", L"" },
	{ SexMale, LangRU, L"вию,сою,цою,хою", L"вия,соя,цоя,хоя", L"-й" },
	{ SexMale, LangBY, L"дразду", L"дразда", L"----озд" },
	{ SexMale, LangBY, L"каралю", L"караля", L"---оль" },
	{ SexMale, LangBY, L"ваўку", L"ваўка", L"----оўк" },
	{ SexMale, LangBY, L"кату,крату", L"ката,крата", L"---от" },
	{ SexMale, LangBY, L"дзятлу", L"дзятла", L"---цел" },
	{ SexMale, LangBY, L"рогу", L"рога", L"-" },
	// беглые гласные, которые общие правила не восстанавливают
	// дательный «зайцу» — Заец; винительный «зайца» остаётся Заяц
	{ SexMale, LangAny, L"зайцу", L"", L"---ец" },
	{ SexMale, LangAny, L"", L"зайца", L"---яц" },
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
	// Корж: в косвенных падежах безударное «а» («Каржа», «Каржу»)
	{ SexMale, LangBY, L"каржу", L"каржа", L"----орж" },
	{ SexMale, LangBY, L"лукашэню", L"лукашэню", L"-я" }
});

const RulesList CNameConverter::RulesLastName = ExpandRules({
	{ SexAny,	LangBY, L"дзе",			L"",			L"--а"},	// Калядзе
	{ SexAny,	LangAny,L"е",			L"у",			L"-а"},
	{ SexFemale,LangAny,L"кай,кой",		L"",			L"--ая"},	// Романской
	{ SexFemale,LangBY,	L"іной,ыной",	L"",			L"--а"},	// Кузьміной
	{ SexFemale,LangBY,	L"ой",			L"",			L"--ая?"},	// Лясной
	{ SexFemale,LangAny,L"ай,ой",		L"",			L"--а"},	// Никитиной
	{ SexFemale,LangAny,L"бей",			L"",			L""},		// Воробей: раньше «-ей»
	{ SexFemale,LangAny,L"ей",			L"ую",			L"--ая"},
	{ SexFemale,LangBY, L"",			L"юю",			L"--яя"},	// Паследнюю
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
	{ SexMale,	LangBY, L"рцу",			L"рца",			L"--эц" },	// Азярэц: «р» в белорусском всегда твёрдое
	{ SexMale,	LangAny,L"пцу,бцу,вцу,нцу,мцу,рцу", L"пца,бца,вца,нца,мца,рца", L"--ец" },
	{ SexMale,	LangRU, L"енку,ёнку",	L"енка,ёнка",	L"--ок" },
	{ SexMale,	LangBY, L"онку,ёнку",	L"онка,ёнка",	L"--ак" },
	{ SexMale,	LangBY, L"анку,енку,энку",	L"",		L"-а" },// Краўчанку, Лукашэнку, Гардзеенку
	{ SexMale,	LangBY, L"чку,тку,пку,бку,нку", L"чка,тка,пка,бка,нка", L"--ок" },
	{ SexMale,	LangBY, L"",			L"шка,зка,жка,дка", L"--ок" },	// Мяшок, Палазок (дательный «-шку» чаще «-шка»)
	{ SexMale,	LangBY, L"",			L"лька,нька,йка", L"---ёк" },	// Каралёк, Канёк, Кіёк
	{ SexMale,	LangAny,L"аку,іку,уку,яку,ыку,юку",	L"",L"-" },		// Климчуку, Антончыку, Прасняку, Бібіку, Манчаку, Панасюку
	{ SexMale,	LangBY, L"ўцу",			L"ўца",			L"---вец"}, // Грыцавец
	{ SexMale,	LangBY, L"цу,лу,ту,ку,пу", L"",		L"-а" },	// Кухту, Жыгалу («-ру» чаще «-р»: Косару)
	{ SexAny,	LangAny,L"те",			L"",			L"-а" },	// Федуте
	{ SexAny,	LangAny,L"це",			L"",			L"--та" },	// Федуце
	// несклоняемые на гласную и «у»: «Сюй Цайхоу»
	{ SexAny,	LangAny,L"оу,эу,ау,еу",	L"оу,эу,ау,еу",	L"" },
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
	{ SexMale, LangBY, L"францішку", L"францішка", L"--ак" },
	// без отчества китайская фамилия и японское имя стоят на месте имени: «У Хунбіня», «Мінору Камату»
	{ SexMale, LangAny, L"", L"у,лу,ху,жу,чжу,мінору,минору", L"" },
	// дагестанское Хизри: указ пишет винительный «Хізру», общее правило дало бы «Хізра»
	{ SexMale, LangBY, L"хізру", L"хізру", L"-ы" },
	{ SexMale, LangRU, L"хизру", L"хизру", L"-и" },
	// дательный «Нікіту» — Нікіта, но «Марату» — Марат: имена на согласную «т»
	{ SexMale, LangBY, L"марату,талгату,ігнату,айрату,рафату,ахмату,мурату,булату,ізату,рахмату", L"", L"-" },
	// «Шабанаву Алі»: «-лі» общего правила — женская «Наталлі», «Алі» не склоняется
	{ SexMale, LangAny, L"алі,али", L"алі,али", L"" },
	// «Беловой Любови»: беглая «о» не выпадает, «-ь» возвращается
	{ SexFemale, LangRU, L"любови", L"", L"--вь" }
});

const RulesList CNameConverter::RulesFirstName = ExpandRules({
	{ SexAny,	LangAny, L"",			L"ю",		L"-я" },	// Наталью, Илью
	{ SexAny,	LangRU, L"ье,ое,йе",	L"",		L"-я" },	// Наталье, Зое, Майе
	{ SexFemale,LangRU, L"ии",			L"",		L"-я" },	// Марии, Алиции
	// латинская «i» здесь уже заменена на «і» (ApplyRules)
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
	{ SexMale,  LangAny,L"е,ё,и,і,о,ы,э", L"е,ё,и,і,о,ы,э,ю", L"" },
	// винительный «Кузьму», «Данілу», «Саву», «Славу» — мужские имена на «-а»
	{ SexMale,  LangAny,L"",			L"у",		L"-а" },
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
		// латинские буквы заменяем белорусскими («Iгар» часто набран латинской I;
		// дубли ищутся без учёта алфавита, см. Catalog::IdentityKey)
		ToCyrillicI(name, true);
	}

	const std::wstring nameLower = ToLower(name);

	// целое слово (таблицы исключений) берёт первое совпадение. Обычное окончание
	// берёт самое длинное, поэтому «Федуце» — это «це», а не более раннее «е».
	// При равной длине остаётся более раннее правило («ому» — «ы» в белорусском и «ый» в русском).
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

// применяет преобразование к каждой части слова через дефис; истина, если преобразованы все
bool CNameConverter::ForEachPart(std::wstring& word, const std::function<bool(std::wstring&)>& convert)
{
	if (word.find(L'-') == std::wstring::npos)
		return convert(word);

	bool bRes = true;
	std::wstring result;
	const std::vector<std::wstring> parts = SplitString(word, L'-'); // пустые части сохраняются
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
	// псевдоним в скобках
	std::wstring strAlias;
	size_t posBracket2 = fullName.rfind(L')');
	size_t posBracket1 = fullName.rfind(L'(', posBracket2);
	if (posBracket2 != std::wstring::npos && posBracket1 != std::wstring::npos && posBracket1 < posBracket2)
	{
		strAlias = fullName.substr(posBracket1 + 1, posBracket2 - posBracket1 - 1);
		NormalizeName(strAlias, lang, nounCase);
		// убираем « (псевдоним)» вместе с пробелом перед скобкой
		const size_t eraseFrom = posBracket1 > 0 ? posBracket1 - 1 : 0;
		fullName.erase(eraseFrom, posBracket2 + 1 - eraseFrom);
	}

	std::wstring firstName, lastName, middleName;
	size_t pos1 = fullName.find(' ');
	// одно имя без фамилии: «Филарета», когда «Беларуси» не взяли за фамилию
	if (pos1 == std::wstring::npos)
	{
		const bool ok = ApplyRules(fullName, ExceptionsFirstName, SexMale, lang, nounCase, true)
			|| ApplyRules(fullName, RulesFirstName, SexMale, lang, nounCase, false);
		if (!strAlias.empty())
			fullName += L" (" + strAlias + L")";
		return ok;
	}
	size_t pos2 = fullName.find(' ', pos1 + 1);
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

	// неузнанное отчество — неразобранное имя: окно импорта смотрит на NF_NAME
	bool middleNameOk = true;
	SexType sex = SexMale;	// по умолчанию мужской
	// фамилия-прилагательное: по ней видны пол и место фамилии, когда отчества
	// нет («Сычэўскага Яна», «Быкоўскую Алену») или оно с ошибкой («Міколу»)
	const SexType adjectiveSex = AdjectiveSurnameSex(lastName, nounCase);
	bool surnameFirst = false;
	if (!middleName.empty())
	{
		sex = GetSex(middleName, nounCase);
		if (sex == SexMale && adjectiveSex == SexFemale && !IsMiddleName(middleName, lang, nounCase))
			sex = SexFemale;
		// двойное отчество: «Гедзімінасавіча-Альбертасавіча»
		middleNameOk = ForEachPart(middleName, [&](std::wstring& part) {
			return ApplyRules(part, RulesMiddleName, sex, lang, nounCase, false);
		});
	}
	else if (adjectiveSex != SexAny)
	{
		sex = adjectiveSex;
		surnameFirst = true;
	}
	else
	{
		std::swap(firstName, lastName);
	}

	// двойное имя: «Хайнца-Рудольфа»
	bool bResFN = ForEachPart(firstName, [&](std::wstring& part) {
		return ApplyRules(part, ExceptionsFirstName, sex, lang, nounCase, true)
			|| ApplyRules(part, RulesFirstName, sex, lang, nounCase, false);
	});

	// двойная фамилия: склоняется каждая часть, «Карпінскую-Пешка», «Денисовой-Клебанович»
	const std::wstring obliqueLastName = lastName;
	bool bResLN = ForEachPart(lastName, [&](std::wstring& part) {
		return ApplyRules(part, ExceptionsLastName, sex, lang, nounCase, true)
			|| ApplyRules(part, RulesLastName, sex, lang, nounCase, false);
	});
	// у имени с отчеством фамилия — первое слово; иностранные имена не трогаем
	if (!middleName.empty() && nounCase != Nominative && lastName.find(L'-') == std::wstring::npos
		&& ChooseKnownSurname(lastName, obliqueLastName, sex, lang))
		bResLN = true;

	if (surnameFirst)
		fullName = lastName + L' ' + firstName;
	else if(middleName.empty())
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

SexType CNameConverter::AdjectiveSurnameSex(const std::wstring& surname, NounCase nounCase)
{
	const std::wstring lower = ToLower(surname);
	const auto endsWith = [&lower](const wchar_t* ending) {
		const size_t length = std::wcslen(ending);
		return lower.length() > length + 1 && lower.compare(lower.length() - length, length, ending) == 0;
	};
	static const wchar_t* female[] = { L"скую", L"цкую", L"ская", L"цкая", L"скай", L"цкай", L"ской", L"цкой" };
	static const wchar_t* maleAccusative[] = { L"скага", L"цкага", L"ского", L"цкого" };
	static const wchar_t* maleDative[] = { L"скаму", L"цкаму", L"скому", L"цкому" };
	for (const wchar_t* ending : female)
	{
		if (endsWith(ending))
			return SexFemale;
	}
	for (const wchar_t* ending : nounCase == Dative ? maleDative : maleAccusative)
	{
		if (endsWith(ending))
			return SexMale;
	}
	return SexAny;
}

void CNameConverter::SetKnownSurnames(LangType lang, std::unordered_map<std::wstring, int> counts)
{
	(lang == LangRU ? m_KnownSurnamesRU : m_KnownSurnamesBY) = std::move(counts);
}

namespace {

bool isNameVowel(wchar_t c)
{
	return std::wstring(L"аеёиоуыэюяіАЕЁИОУЫЭЮЯІ").find(c) != std::wstring::npos;
}

bool isNameConsonant(wchar_t c)
{
	return !isNameVowel(c) && std::wstring(L"йўьъ'’-").find(c) == std::wstring::npos && c >= L'А' && c <= L'я';
}

} // namespace

std::vector<std::wstring> CNameConverter::SurnameVariants(const std::wstring& oblique, const std::wstring& ruleResult, SexType sex)
{
	std::vector<std::wstring> bases{ ruleResult };
	const std::wstring lower = ToLower(oblique);
	// несклоняемая фамилия: «Мдзівані», «Бачарышвілі», «Жигэу» стоят в указе как есть
	if (lower.length() > 3 && (lower.back() == L'і' || lower.back() == L'и'
		|| (lower.back() == L'у' && isNameVowel(lower[lower.length() - 2]))))
		bases.push_back(oblique);
	const auto endsWith = [&lower](const wchar_t* ending) {
		const std::wstring e(ending);
		return lower.length() > e.length() + 2 && lower.compare(lower.length() - e.length(), e.length(), e) == 0;
	};
	if (sex != SexFemale)
	{
		// «Склюту» — Склют или Склюта, «Ардзяка» — Ардзяк, «Колесня» — Колеснь (беглая ниже)
		if (endsWith(L"у") || endsWith(L"а") || endsWith(L"ю") || endsWith(L"я"))
		{
			const std::wstring stem = oblique.substr(0, oblique.length() - 1);
			bases.push_back(stem);
			bases.push_back(stem + L"а");
			if (endsWith(L"ю") || endsWith(L"я"))
			{
				bases.push_back(stem + L"ь");
				bases.push_back(stem + L"й"); // «Неглюя» — Неглюй
			}
			// «Ціву» — Ціў: в конце белорусского слова «в» становится «ў»
			if (!stem.empty() && stem.back() == L'в')
				bases.push_back(stem.substr(0, stem.length() - 1) + L"ў");
		}
	}
	else if (endsWith(L"ой") || endsWith(L"ай") || endsWith(L"ей") || endsWith(L"ую"))
	{
		// «Гаявой» — Гаява или Гаявая
		const std::wstring stem = oblique.substr(0, oblique.length() - 2);
		bases.push_back(stem + L"а");
		bases.push_back(stem + L"ая");
	}

	std::vector<std::wstring> variants;
	const auto add = [&variants, &ruleResult](const std::wstring& word) {
		if (word.length() > 1 && ToLower(word) != ToLower(ruleResult)
			&& std::find(variants.begin(), variants.end(), word) == variants.end())
			variants.push_back(word);
	};
	for (const std::wstring& base : bases)
	{
		add(base);
		const size_t n = base.length();
		// беглая гласная: «Лобанк» — Лобанок, «Глушц» — Глушец, «Колеснь» — Колесень
		if (n >= 4 && base.compare(n - 2, 2, L"нь") == 0 && isNameConsonant(base[n - 3]))
		{
			for (const wchar_t* vowel : { L"е", L"а", L"э", L"ё" })
				add(base.substr(0, n - 2) + vowel + L"нь");
		}
		else if (n >= 3 && isNameConsonant(base[n - 1]) && isNameConsonant(base[n - 2]))
		{
			for (const wchar_t* vowel : { L"о", L"е", L"а", L"э", L"ё" })
				add(base.substr(0, n - 1) + vowel + base.substr(n - 1));
		}
		// одна гласная, безударная в косвенном падеже: «Валк» — Волк/Воўк, «Бабр» — Бобр, «Ярш» — Ёрш
		size_t vowels = 0, at = 0;
		for (size_t i = 0; i < n; ++i)
		{
			if (isNameVowel(base[i]))
			{
				++vowels;
				at = i;
			}
		}
		if (vowels == 1 && (base[at] == L'а' || base[at] == L'я' || base[at] == L'Я' || base[at] == L'А'))
		{
			std::wstring stressed = base;
			stressed[at] = base[at] == L'а' ? L'о' : base[at] == L'А' ? L'О' : base[at] == L'я' ? L'ё' : L'Ё';
			add(stressed);
			if (at + 1 < n && stressed[at + 1] == L'л')
				add(stressed.substr(0, at + 1) + L"ў" + stressed.substr(at + 2));
		}
	}
	return variants;
}

bool CNameConverter::ChooseKnownSurname(std::wstring& surname, const std::wstring& oblique, SexType sex, LangType lang) const
{
	const auto& known = lang == LangRU ? m_KnownSurnamesRU : m_KnownSurnamesBY;
	if (known.empty() || surname.empty() || known.count(ToLower(surname)))
		return false;
	std::wstring source = oblique;
	if (lang == LangBY)
		ToCyrillicI(source, true);
	int best = 0;
	std::wstring chosen;
	for (const std::wstring& variant : SurnameVariants(source, surname, sex))
	{
		const auto found = known.find(ToLower(variant));
		if (found != known.end() && found->second > best)
		{
			best = found->second;
			chosen = variant;
		}
	}
	if (chosen.empty())
		return false;
	surname = chosen;
	return true;
}
