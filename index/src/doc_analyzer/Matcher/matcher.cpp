#include "matcher.h"

#include <algorithm>
#include <iostream>

#include "distancetable.h"
#include "blockmatching.hpp"

using namespace std;

//Using pointers for now to avoid having to write a hash function for a block (used to be due to memory constraints)
vector<std::shared_ptr<Block>> getOptimalBlocks(StringEncoder& se, int minblocksize, int maxblockcount, int selectionparameter) {
    //Find common blocks between the two files
    vector<shared_ptr<Block>> commonblocks = getCommonBlocks(minblocksize, se);
    extendBlocks(commonblocks, se);
    resolveIntersections(commonblocks);

    cout << "Got " << commonblocks.size() << " blocks" << endl;
    if(commonblocks.size() > 2000) {
        sort(commonblocks.begin(), commonblocks.end(), compareSizeGreater);
        commonblocks.resize(2000);
    }

    //Create a graph of the common blocks
    BlockGraph G(commonblocks);
    vector<shared_ptr<Block>> topsort = topologicalSort(G);

    size_t sum = 0;
    for(shared_ptr<Block>& b : commonblocks) {
        sum += G.getAdjacencyList(b).size();
    }

    cout << "Generated " << sum << " edges in the block graph" << endl;

    //Get the optimal set of blocks to select
    DistanceTable disttable(maxblockcount, G, topsort);
    vector<shared_ptr<Block>> finalpath = disttable.findOptimalPath(selectionparameter);

    cout << "Selected " << finalpath.size() << " blocks" << endl;

    return finalpath;
}

//Helper functions to make block traversal more clean
bool skipBlock(int beginloc, size_t blocklength, int& index, size_t& blockindex);

pair<unordered_map<string, ExternNPposting>, vector<ExternPposting>>
getPostings(vector<std::shared_ptr<Block>>& commonblocks, unsigned int doc_id, unsigned int &fragID, StringEncoder& se) {
    //Which block to skip next
    size_t blockindex = 0;
    unordered_map<string, ExternNPposting> nppostingsmap;
    vector<ExternPposting> ppostingslist;

    //Sort blocks based on oldindex first
    sort(commonblocks.begin(), commonblocks.end(), compareOld);

    //Set index to either the end of the first block or to 0
    int index = 0;
    if(commonblocks.size() > 0 && index == commonblocks[0]->oldloc) {
        index = commonblocks[blockindex]->len;
        blockindex++;
    }
    
    while(index < se.getOldSize()) {
        string decodedword = se.decodeNum(*(se.getOldIter()+index));
        //Word not yet indexed
        if(nppostingsmap.find(decodedword) == nppostingsmap.end()) {
            ExternNPposting newposting(decodedword, doc_id, se.getNewCount(decodedword));
            nppostingsmap.emplace(make_pair(decodedword, ExternNPposting{decodedword, doc_id, se.getNewCount(decodedword)}));
        }

        index++;
        //Condition prevents attempting to access an empty vector
        while(blockindex < commonblocks.size() && 
            skipBlock(commonblocks[blockindex]->oldloc, commonblocks[blockindex]->len, index, blockindex))
            ;
    }

    sort(commonblocks.begin(), commonblocks.end(), compareNew);

    index = 0;
    blockindex = 0;
    if(commonblocks.size() > 0 && index == commonblocks[0]->newloc) {
        index = commonblocks[blockindex]->len;
        blockindex++;
    }
    
    while(index < se.getNewSize()) {
        //Edited sections in new file are considered "inserted"
        string decodedword = se.decodeNum(*(se.getNewIter()+index));
        //Word not yet indexed in nonpositional map
        if(nppostingsmap.find(decodedword) == nppostingsmap.end()) {
            nppostingsmap.emplace(make_pair(decodedword, ExternNPposting{decodedword, doc_id, se.getNewCount(decodedword)}));
        }
        //Always insert positional posting for a word
        ppostingslist.emplace_back(decodedword, doc_id, fragID, index);

        index++;
        if(blockindex < commonblocks.size()) {
            bool skip = skipBlock(commonblocks[blockindex]->newloc, commonblocks[blockindex]->len, index, blockindex);
            //When we skip a block of common text, we need a new fragID. Only need a new fragID once though, not per skip
            if(skip)
                ++fragID;
            while(blockindex < commonblocks.size() &&
                skipBlock(commonblocks[blockindex]->newloc, commonblocks[blockindex]->len, index, blockindex))
                ;
        }
    }

    return make_pair(nppostingsmap, ppostingslist);
}

//Helper functions to make block traversal more clean

//Given an index and a block, skip the common block or do nothing
//Returns whether a skip was performed or not
//To make the code reusable, pass only the block beginning and its length
bool skipBlock(int beginloc, size_t blocklength, int& index, size_t& blockindex) {
    if(index >= beginloc) {
        index += blocklength;
        ++blockindex;
        return true;
    }
    else return false;
}