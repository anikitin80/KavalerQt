#pragma once

#include <string>
#include <vector>

// Сведения о группе в заголовке или пункте указа («группу работников ...»).
namespace decree {

bool isGroupLetter(wchar_t c);

// точка кончает предложение, но не «В.И.Ленина» и не «г. Минска»
size_t groupSentenceEnd(const std::wstring& text);

// запятая, после которой учёная степень, а не продолжение должности
size_t academicTitleComma(const std::wstring& text);

// самое правое слово группы: «группу работников завода» — завод, а не «работников завода»
bool findLastToken(const std::wstring& str, const std::vector<std::wstring>& tokens, size_t& start, size_t& end);

// пустая должность: в каталоге стоит само слово списка в именительном единственном
std::wstring groupRole(const std::wstring& token);

// у автомобильного завода каталог не держит родительское объединение;
// у рессорного и кузнечного заводов того же объединения — держит
void cutTrailingAssociation(std::wstring& text);

// «завод» і акадэмія: к должности идёт завод, акадэмия остаётся тем, кто её уже назвал
std::wstring cutOtherOrganisation(std::wstring& groupInfo);

// «работников Белорусского государственного университета»: головное слово —
// это сами награждённые, к должности добавляется организация
void stripGroupHead(std::wstring& groupInfo);

} // namespace decree
