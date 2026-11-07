#pragma once

// Какие разобранные люди уже есть в каталоге и кого импортировать.
//
// Человека узнаём по номеру и дате указа, награде и имени. Тёзки:
// k-й человек файла указа с тем же ключом известен, когда в каталоге
// не меньше k таких записей, поэтому двое разных людей с одним именем в одном
// указе импортируются оба, а один указ в двух файлах — один раз.

#include "ParserTypes.h"

#include <QVector>

#include <vector>

class Catalog;

struct ImportMatch
{
    bool known = false; // уже в каталоге
    int namesakes = 1;  // люди с этим указом, наградой и именем в том же файле
};

QVector<ImportMatch> matchImported(const std::vector<PersonInfo>& persons, const Catalog& catalog);

// индексы людей к импорту, по порядку; wanted[i] ложен для людей,
// скрытых текущим фильтром (тёзок они всё равно считают)
QVector<int> personsToImport(const std::vector<PersonInfo>& persons, const QVector<bool>& wanted,
                             const Catalog& catalog);
