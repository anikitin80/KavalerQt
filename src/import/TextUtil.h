#pragma once

#include <string>
#include <vector>

bool IsWordDivision(wchar_t character);
bool IsCapital(wchar_t character);
class QDate;
QDate ParseDate(std::wstring strDate, bool russian);
bool FindTokens(const std::wstring& str, std::vector<std::wstring>& tokens, size_t& start, size_t& end);
std::vector<std::wstring> SplitString(const std::wstring& str, wchar_t sep);
void StrTrim(std::wstring& str);
std::wstring ToLower(const std::wstring& str);
