#include "matcher.h"

#include <algorithm>
#include <iostream>

#include "distancetable.h"
#include "blockmatching.hpp"

using namespace std;

static double avgcoverage = 0;
static int coveragecount = 0;

//Using pointers for now to avoid having to write a hash function for a block (used to be due to memory constraints)
vector<std::shared_ptr<Block>> getOptimalBlocks(StringEncoder& se, int minblocksize, int maxblockcount, int selectionparameter) {
    //Find common blocks between the two files
    vector<shared_ptr<Block>> commonblocks = getCommonBlocks(minblocksize, se);
    extendBlocks(commonblocks, se);
    resolveIntersections(commonblocks);

    cout << "Got " << commonblocks.size() << " blocks" << endl;
    if(commonblocks.size() > 100000) {
        for(size_t i = 0; i < commonblocks.size(); ) {
            if(commonblocks[i]->len < 8) {
                commonblocks[i] = commonblocks.back();
                commonblocks.pop_back();
            }
            else {
                i++;
            }
        }
        // sort(commonblocks.begin(), commonblocks.end(), compareSizeGreater);
        // commonblocks.resize(2000);
    }

    //Create a graph of the common blocks
    // BlockGraph G(commonblocks);
    // vector<shared_ptr<Block>> topsort = topologicalSort(G);

    // size_t sum = 0;
    // for(shared_ptr<Block>& b : commonblocks) {
    //     sum += G.getAdjacencyList(b).size();
    // }

    // cout << "Generated " << sum << " edges in the block graph" << endl;

    //Get the optimal set of blocks to select
    // DistanceTable disttable(maxblockcount, G, topsort);
    DistanceTable disttable(maxblockcount, commonblocks);
    vector<shared_ptr<Block>> finalpath = disttable.findOptimalPath(selectionparameter);

    cout << "Selected " << finalpath.size() << " blocks" << endl;

    int newcommon = 0;
    for(shared_ptr<Block> b : finalpath)
        newcommon += b->len;
     if(se.getNewSize() > 0) {
        avgcoverage += (((double)newcommon/se.getNewSize() * 100) - avgcoverage) / (coveragecount+1);
        coveragecount++;
        cout << "Average coverage of new document is " << avgcoverage << "%" << endl;
    }

    return finalpath;
}

//Helper functions to make block traversal more clean
bool skipBlock(int beginloc, size_t blocklength, int& index, size_t& blockindex);

pair<unordered_map<string, ExternNPposting>, vector<ExternPposting>>
getPostings(vector<std::shared_ptr<Block>>& commonblocks, unsigned int doc_id, unsigned int &fragID, StringEncoder& se) {
    // Tells whether a fragID has been applied. Used to determine if fragID needs to be incremented
    // to match the "next ID to use" postcondition
    bool fragIDapplied = false;
    //Which block to skip next
    size_t blockindex = 0;
    unordered_map<string, ExternNPposting> nppostingsmap;
    vector<ExternPposting> ppostingslist;

    //Sort blocks based on oldindex first
    sort(commonblocks.begin(), commonblocks.end(), compareOld);

    //Skip common blocks if they begin at the start of the document
    int index = 0;
    while(blockindex < commonblocks.size() && 
            skipBlock(commonblocks[blockindex]->oldloc, commonblocks[blockindex]->len, index, blockindex))
            ;
    
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
    while(blockindex < commonblocks.size() && 
            skipBlock(commonblocks[blockindex]->newloc, commonblocks[blockindex]->len, index, blockindex))
            ;
    
    while(index < se.getNewSize()) {
        //Edited sections in new file are considered "inserted"
        string decodedword = se.decodeNum(*(se.getNewIter()+index));
        //Word not yet indexed in nonpositional map
        if(nppostingsmap.find(decodedword) == nppostingsmap.end()) {
            nppostingsmap.emplace(make_pair(decodedword, ExternNPposting{decodedword, doc_id, se.getNewCount(decodedword)}));
        }
        //Always insert positional posting for a word
        ppostingslist.emplace_back(decodedword, doc_id, fragID, index);
        fragIDapplied = true;

        index++;
        if(blockindex < commonblocks.size()) {
            bool skip = skipBlock(commonblocks[blockindex]->newloc, commonblocks[blockindex]->len, index, blockindex);
            //When we skip a block of common text, we need a new fragID. Only need a new fragID once though, not per skip
            if(skip) {
                ++fragID;
                fragIDapplied = false;
            }
            while(blockindex < commonblocks.size() &&
                skipBlock(commonblocks[blockindex]->newloc, commonblocks[blockindex]->len, index, blockindex))
                ;
        }
    }

    if(fragIDapplied)
        fragID++;

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