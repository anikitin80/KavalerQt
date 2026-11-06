#pragma once

// Which parsed persons are already in the catalog and which ones to import.
//
// A person is identified by decree number and date, award and name. Namesakes:
// the k-th person of a decree file with the same key is known when the catalog
// has at least k such records, so two different people with one name in one
// decree are both imported, while the same decree in two files is imported once.

#include "ParserTypes.h"

#include <QVector>

#include <vector>

class Catalog;

struct ImportMatch
{
    bool known = false; // already in the catalog
    int namesakes = 1;  // persons with this decree, award and name in the same file
};

QVector<ImportMatch> matchImported(const std::vector<PersonInfo>& persons, const Catalog& catalog);

// indexes of the persons to import, in order; wanted[i] is false for persons
// hidden by the current filter (they still count as namesakes)
QVector<int> personsToImport(const std::vector<PersonInfo>& persons, const QVector<bool>& wanted,
                             const Catalog& catalog);
