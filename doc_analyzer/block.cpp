#include "block.h"

#include <vector>
#include <algorithm>
#include <map>
#include <iostream>

using namespace std;

ostream& operator<<(ostream& os, const Block& bl) {
    os << bl.oldloc << "-" << bl.newloc << "-" << bl.run.size();
    return os;
}

bool compareOld(const Block* lhs, const Block* rhs) {
    return lhs->oldloc < rhs->oldloc;
}
bool compareNew(const Block* lhs, const Block* rhs) {
    return lhs->newloc < rhs->newloc;
}

//Gets all common blocks between the two files of length minsize
vector<Block*> getCommonBlocks(int minsize, const vector<int>& oldfile, const vector<int>& newfile) {
    vector<Block*> commonblocks;
    
    //Represents all possible blocks from the old file
    //Last element of vector is oldloc
    //<hash, block>
    //Hash is computed without oldloc in vector
    multimap<unsigned int, vector<int>> potentialblocks;
    
    //Get all possible blocks in the old file
    for(size_t i = 0; i <= oldfile.size() - minsize; i++) {
        vector<int> newblock(&oldfile[i], &oldfile[i+minsize]);
        //hash the block before inserting oldloc
        unsigned int blockhash = hashVector(newblock);
        newblock.push_back(i);
        potentialblocks.insert(make_pair(blockhash, newblock));
    }
    
    //Check each block in the new file for a match with a block in the old file
    for(size_t i = 0; i <= newfile.size() - minsize; i++) {
        vector<int> blockcheck(&newfile[i], &newfile[i+minsize]);
        unsigned int blockhash = hashVector(blockcheck);
        
        //Get all blocks that match the current block's hash
        auto blockmatchrange = potentialblocks.equal_range(blockhash);
        for(auto iter = blockmatchrange.first; iter != blockmatchrange.second; ++iter) {
            //Remove oldloc before confirming equality
            int oldloc = (*iter).second.back();
            (*iter).second.pop_back();
            if(blockcheck == (*iter).second) {
                Block* newblock = new Block;
                newblock->run = blockcheck;
                newblock->oldloc = oldloc;
                newblock->newloc = i;
                
                commonblocks.push_back(newblock);
            }
            //Put oldloc back for later use
            (*iter).second.push_back(oldloc);
        }
    }
    
    return commonblocks;
}

//Attempt to extend common blocks
void extendBlocks(vector<Block*>& allblocks, const vector<int>& oldfile, const vector<int>& newfile) {
    //Sort based on old locations
    sort(allblocks.begin(), allblocks.end(), compareOld);
    
    //Index of block to be extended
    size_t index = 0;
    while(index < allblocks.size()) {
        size_t blocklength = allblocks[index]->run.size();
        //indexes are now pointing beyond the block
        int oldfileindex = allblocks[index]->oldloc + blocklength;
        int newfileindex = allblocks[index]->newloc + blocklength;
        
        //Attempt to extend the block
        while(oldfileindex < oldfile.size() && newfileindex < newfile.size() && oldfile[oldfileindex] == newfile[newfileindex]) {
            //Append the new word
            allblocks[index]->run.push_back(oldfile[oldfileindex]);
            oldfileindex++;
            newfileindex++;
        }
        
        //if we successfully extended the block, check for possible overlaps
        if(allblocks[index]->run.size() > blocklength) {
            auto overlapchecker = allblocks.begin() + index + 1;
            //Potential overlap as long as the other block's begin is before this block's end
            while(overlapchecker != allblocks.end() && (*overlapchecker)->oldloc < oldfileindex) {
                int overlaplength = (*overlapchecker)->run.size()-1;
                
                if((*overlapchecker)->oldloc >= allblocks[index]->oldloc &&
                    (*overlapchecker)->oldloc+overlaplength <= oldfileindex &&
                    (*overlapchecker)->newloc >= allblocks[index]->newloc &&
                    (*overlapchecker)->newloc+overlaplength <= newfileindex)
                {
                    delete *overlapchecker;
                    overlapchecker = allblocks.erase(overlapchecker);
                }
                else
                    overlapchecker++;
            }
        }
        
        index++;
    }
}

//Resolve blocks that are intersecting
//Should be run after extendBlocks
void resolveIntersections(vector<Block*>& allblocks) {
    //List of blocks to add to the main list later
    vector<Block*> addedblocks;
    //First, sort based on old locations
    sort(allblocks.begin(), allblocks.end(), compareOld);
    
    for(size_t i = 0; i < allblocks.size(); i++) {
        for(size_t j = i+1; j < allblocks.size(); j++) {
            //break if intersections are not possible anymore
            if(allblocks[j]->oldloc > allblocks[i]->oldloc + allblocks[i]->run.size()-1)
                break;
            
            //Intersection occurs if B.end > A.end > B.begin > A.begin
            //We know that B.begin > A.begin (iterating in sorted order)
            //We know that A.end > B.begin (is our breaking condition)
            //Thus we only need to check B.end > A.end
            if(allblocks[j]->oldloc + allblocks[j]->run.size()-1 > allblocks[i]->oldloc + allblocks[i]->run.size()-1) {
                cout << "intersect";
                //calculate how much the current block needs to shrink by
                int shrunksize = allblocks[j]->oldloc - allblocks[i]->oldloc;
                Block* shrunkblock = new Block;
                shrunkblock->run = vector<int>(allblocks[i]->run.begin(), allblocks[i]->run.begin()+shrunksize);
                shrunkblock->oldloc = allblocks[i]->oldloc;
                shrunkblock->newloc = allblocks[i]->newloc;
                addedblocks.push_back(shrunkblock);
            }
        }
    }
    
    //Now sort based on new locations
    sort(allblocks.begin(), allblocks.end(), compareNew);
    
    for(size_t i = 0; i < allblocks.size(); i++) {
        for(size_t j = i+1; j < allblocks.size(); j++) {
            if(allblocks[j]->newloc > allblocks[i]->newloc + allblocks[i]->run.size()-1)
                break;
            
            if(allblocks[j]->newloc + allblocks[j]->run.size()-1 > allblocks[i]->newloc + allblocks[i]->run.size()-1) {
                cout << "intersect";
                int shrunksize = allblocks[j]->newloc - allblocks[i]->newloc;
                Block* shrunkblock = new Block;
                shrunkblock->run = vector<int>(allblocks[i]->run.begin(), allblocks[i]->run.begin()+shrunksize);
                shrunkblock->oldloc = allblocks[i]->oldloc;
                shrunkblock->newloc = allblocks[i]->newloc;
                addedblocks.push_back(shrunkblock);
            }
        }
    }
    
    //append the list of added blocks
    allblocks.insert(allblocks.end(), addedblocks.begin(), addedblocks.end());
}

//Credit to https://stackoverflow.com/a/3404820
//Does not need to be optimal; we will confirm equality later anyways
//Must be unsigned since overflow is undefined for signed ints
unsigned int hashVector(vector<int>& v) {
    unsigned int hc = v.size();
    for(size_t i = 0; i < v.size(); i++)
    {
        hc = hc*314159 + v[i];
    }
    return hc;
}