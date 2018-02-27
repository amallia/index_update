#ifndef QUERY_PRIMITIVE_HPP
#define QUERY_PRIMITIVE_HPP

#include <map>
#include <vector>

#include "../posting.hpp"
#include "../extended_lexicon.hpp"
#include "query_primitive_low.hpp"

class query_primitive {
public:
    query_primitive(unsigned int termID, std::map<unsigned int, std::vector<nPosting>>& index, ExtendedLexicon& exlex);

    unsigned int nextGEQ(unsigned int x);
    unsigned int getFreq();
private:
    std::vector<query_primitive_low> lists;
};

#endif