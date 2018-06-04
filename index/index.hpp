#ifndef INDEX_HPP
#define INDEX_HPP

#include <vector>
#include <map>
#include <unordered_map>

#include "lexicon.hpp"
#include "posting.hpp"
#include "meta.hpp"
#include "extended_lexicon.hpp"
#include "Structures/documentstore.h"
#include "Structures/translationtable.h"
#include "static_index.hpp"
#include "global_parameters.hpp"

//This index does not use compression
class Index {
public:
    //Directory is simply a name that the index will save all of its files under
    Index(std::string directory);
    void insert_document(std::string& url, std::string& newpage);
    //Temporary return type: returns docIDs for now
    std::vector<unsigned int> query(std::vector<std::string> words);

    void dump();
    void restore();
    void clear();

private:
    template<typename T>
    void insert_posting(std::vector<T>& postinglist, T posting);

    //Data structures
    //Note: Posting lists are *lazily sorted*, that is, docIDs are stored randomly until they need to be sorted.
    //Indexes on disk are guaranteed to be sorted (due to delta compression)
    GlobalType::PosIndex positional_index;
    GlobalType::NonPosIndex nonpositional_index;
    //TODO: Can this be obtained from docstore?
    std::unordered_map<unsigned int, unsigned int> doclength;
    double avgdoclength;

    unsigned long positional_size;
    unsigned long nonpositional_size;

    std::string working_dir;

    Structures::DocumentStore docstore;
    Structures::TranslationTable transtable;
    Lexicon lex;
    StaticIndex staticwriter;
};

#endif
