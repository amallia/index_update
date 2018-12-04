#pragma once

#include <vector>

#include "diff.hpp"
#include "landmark/landmarkarray.hpp"

struct EditEntry {
    enum EditOperation {
        insertion,
        deletion,
        shift
    };

    EditEntry(bool l, EditOperation e, unsigned int a, unsigned int p, int t)
        : isLandEdit(l), editop(e), landID(a), pos(p), termID(t) {}

    bool isLandEdit; //false for Posting

    EditOperation editop; // 0 = insert, 1 = delete, 2 = shift
    unsigned int landID;
    int pos; // offset for posting, location/amount for landmarks
    int termID; // -1 for landmarks
};

void transform(std::vector<int>& olddoc, std::vector<Landmark>& landmarks, std::vector<DiffEntry>& editscript);