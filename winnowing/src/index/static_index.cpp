#include "static_index.hpp"

#include <iostream>

#include "global_constants.hpp"
#include "static_functions/postingIO.hpp"
#include "static_functions/bytesIO.hpp"
#include "utility/fs_util.hpp"

//Copies n bytes from the ifstream to the ofstream
//Returns 0 on success
int copyBytes(std::ifstream& ifile, std::ofstream& ofile, int n) {
    char* buffer = new char[n];
    ifile.read(buffer, n);
    ofile.write(buffer, ifile.gcount());
    delete[] buffer;
    //Copied less than n bytes
    if(ifile.gcount() != n) {
        return 1;
    }
    return 0;
}

//Copies the static posting list block from ifile to ofile
//Returns the number of postings in the static block
//Assumes termID has already been read and that ifile/ofile are pointing to the correct positions
unsigned int copyPostingList(unsigned int termID, std::ifstream& ifile, std::ofstream& ofile) {
    //Read length of block
    unsigned int blocklen;
    readFromBytes(blocklen, ifile);
    if(blocklen < 32)
        throw std::runtime_error("Error, invalid blocklen in copyPostingList: " + std::to_string(blocklen));
    //Read number of postings
    unsigned int postinglistcount;
    readFromBytes(postinglistcount, ifile);

    //Copy the vars
    writeAsBytes(termID, ofile);
    writeAsBytes(blocklen, ofile);
    writeAsBytes(postinglistcount, ofile);

    //Don't copy over termid, size, postingcount
    blocklen -= 12;

    //Copy the rest of the block
    copyBytes(ifile, ofile, blocklen);

    return postinglistcount;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

StaticIndex::StaticIndex(const std::string& working_dir) : INDEXDIR("./" + working_dir + IndexPath),
    PDIR("./" + working_dir + PosPath),
    NPDIR("./" + working_dir + NonPosPath)
{
    std::cout << PDIR << '\n' << NPDIR << '\n';

}

SparseExtendedLexicon* StaticIndex::getExLexPointer() {
    return &spexlex;
}

//Writes the positional index to disk, which means it is saved either in file Z0 or I0.
void StaticIndex::write_p_disk(PosMapIter indexbegin, PosMapIter indexend) {
    std::string filename = PDIR;
    std::string indexname;
    //Z0 exists
    if(std::ifstream(filename + "Z0"))
        indexname = "I0";
    else
        indexname = "Z0";

    std::ofstream ofile(filename + indexname);

    if (ofile.is_open()){
        write_index<PosMapIter>(indexname, ofile, true, indexbegin, indexend);

        ofile.close();
    }else{
        std::cerr << "File cannot be opened." << std::endl;
    }

    merge_test(true);
}

//Writes the non-positional index to disk, which is saved in either file Z0 or I0
void StaticIndex::write_np_disk(NonPosMapIter indexbegin, NonPosMapIter indexend) {
    std::string filename = NPDIR;
    std::string indexname;
    //Z0 exists
    if(std::ifstream(filename + "Z0"))
        indexname = "I0";
    else
        indexname = "Z0";

    std::ofstream ofile(filename + indexname);

    if (ofile.is_open()){
        write_index<NonPosMapIter>(indexname, ofile, false, indexbegin, indexend);

        ofile.close();
    }else{
        std::cerr << "File cannot be opened." << std::endl;
    }

    merge_test(false);
}

//Writes an inverted index to disk using compressed postings
//indexname: The name of the index being written to. Always follows the format (Z|I)(number)
template <typename T>
void StaticIndex::write_index(std::string& indexname, std::ofstream& ofile, bool positional, T indexbegin, T indexend) {
    if(indexname.empty())
        throw std::invalid_argument("Error, index name is empty");

    bool isZindex;
    if(indexname[0] == 'Z')
        isZindex = true;
    else if(indexname[0] == 'I')
        isZindex = false;
    else
        throw std::invalid_argument("Error, index name " + indexname + " is invalid");

    unsigned int indexnum = std::stoul(indexname.substr(1));

    //Counts how many postings have accumulated since the last pointer inserted in the extended lexicon
    size_t postingcount = 0;
    //Indicates whether the last list had a pointer due to size
    bool lastlisthadpointer = false;

    //for each posting list in the index
    for(auto postinglistiter = indexbegin; postinglistiter != indexend; postinglistiter++) {
        shouldGetLexEntry(postinglistiter->second.size(), postinglistiter->first, indexnum, isZindex, ofile.tellp(),
            positional, postingcount, lastlisthadpointer);

        //Write out the posting list to disk
        write_postinglist(ofile, postinglistiter->first, postinglistiter->second, positional);
    }
}

/**
 * Test if there are two files of same index number on disk.
 * If there is, merge them and then call merge_test again until
 * all index numbers have only one file each.
 * Assumes that only one index is ever written to disk
 */
void StaticIndex::merge_test(bool isPositional) {
    //Assign directory the correct string based on the parameter
    std::string directory = isPositional ? PDIR : NPDIR;

    std::vector<std::string> files = FSUtil::readDirectory(directory);
    auto dir_iter = files.begin();

    while(dir_iter != files.end()) {
        //If any index file starts with an 'I', then we need to merge it
        if(dir_iter->size() > 1 && (*dir_iter)[0] == 'I') {
            //Get the number of the index
            int indexnum = std::stoi(dir_iter->substr(1));
            std::cerr << "Merging positional: " << isPositional << ", index number " << indexnum << std::endl;
            merge(indexnum, isPositional);

            files.clear();
            files = FSUtil::readDirectory(directory);
            dir_iter = files.begin();
        }
        else {
            dir_iter++;
        }
    }
}

/**
 * Merges the indexes of the given order. Both the Z-index and I-index must already exist before
 * this method is called.
 */
void StaticIndex::merge(int indexnum, bool positional) {
    std::ifstream zfilestream;
    std::ifstream ifilestream;
    std::ofstream ofile;
    std::string dir = positional ? PDIR : NPDIR;

    //determine the name of the output file, if "Z" file exists, than compressed to "I" file.
    zfilestream.open(dir + "Z" + std::to_string(indexnum));
    ifilestream.open(dir + "I" + std::to_string(indexnum));

    char flag = 'Z';
    //If z-index exists
    if(std::ifstream(dir + "Z" + std::to_string(indexnum+1)))
        flag = 'I';

    bool isZindex = (flag == 'Z');

    ofile.open(dir + flag + std::to_string(indexnum + 1));

    //Counts how many postings have accumulated since the last pointer inserted in the extended lexicon
    size_t postingcount = 0;
    //Indicates whether the last list had a pointer due to size
    bool lastlisthadpointer = false;

    unsigned int ZtermID, ItermID;
    readFromBytes(ZtermID, zfilestream);
    readFromBytes(ItermID, ifilestream);
    while(true) {
        if(ItermID < ZtermID) {
            //Store the position of the written block
            unsigned long pos = ofile.tellp();
            unsigned int postingsize = copyPostingList(ItermID, ifilestream, ofile);

            shouldGetLexEntry(postingsize, ItermID, indexnum+1, isZindex, pos, positional, postingcount, lastlisthadpointer);

            readFromBytes(ItermID, ifilestream);
            if(!ifilestream) break;
        }
        else if(ZtermID < ItermID) {
            //Store the position of the written block
            unsigned long pos = ofile.tellp();
            unsigned int postingsize = copyPostingList(ZtermID, zfilestream, ofile);

            shouldGetLexEntry(postingsize, ZtermID, indexnum+1, isZindex, pos, positional, postingcount, lastlisthadpointer);

            readFromBytes(ZtermID, zfilestream);
            if(!zfilestream) break;
        }
        else {
            //Store the position of the written block
            unsigned long pos = ofile.tellp();

            if(positional) {
                //read both posting lists from both files
                std::vector<Posting> zpostinglist = read_pos_postinglist(zfilestream, ZtermID);
                std::vector<Posting> ipostinglist = read_pos_postinglist(ifilestream, ItermID);

                //merge the posting lists
                std::vector<Posting> merged = merge_pos_postinglist(zpostinglist, ipostinglist);

                //write the final posting list to disk, creating a new metadata entry
                write_postinglist<Posting>(ofile, ZtermID, merged, true);

                shouldGetLexEntry(merged.size(), ZtermID, indexnum+1, isZindex, pos, positional, postingcount, lastlisthadpointer);
            }
            else {
                //read both posting lists from both files
                std::vector<nPosting> zpostinglist = read_nonpos_postinglist(zfilestream, ZtermID);
                std::vector<nPosting> ipostinglist = read_nonpos_postinglist(ifilestream, ItermID);

                //merge the posting lists
                std::vector<nPosting> merged = merge_nonpos_postinglist(zpostinglist, ipostinglist);

                //write the final posting list to disk, creating a new metadata entry
                write_postinglist<nPosting>(ofile, ZtermID, merged, false);

                shouldGetLexEntry(merged.size(), ZtermID, indexnum+1, isZindex, pos, positional, postingcount, lastlisthadpointer);
            }

            readFromBytes(ZtermID, zfilestream);
            readFromBytes(ItermID, ifilestream);
            if(!zfilestream || !ifilestream) break;
        }
    }
    while(zfilestream) {
        //Store the position of the written block
        unsigned long pos = ofile.tellp();
        unsigned int postingsize = copyPostingList(ZtermID, zfilestream, ofile);

        shouldGetLexEntry(postingsize, ZtermID, indexnum+1, isZindex, pos, positional, postingcount, lastlisthadpointer);
        readFromBytes(ZtermID, zfilestream);
    }
    while(ifilestream) {
        //Store the position of the written block
        unsigned long pos = ofile.tellp();
        unsigned int postingsize = copyPostingList(ItermID, ifilestream, ofile);

        shouldGetLexEntry(postingsize, ItermID, indexnum+1, isZindex, pos, positional, postingcount, lastlisthadpointer);
        readFromBytes(ItermID, ifilestream);
    }

    zfilestream.close();
    ifilestream.close();
    ofile.close();

    spexlex.clearIndex(indexnum, positional);

    std::string filename1 = dir + "Z" + std::to_string(indexnum);
    std::string filename2 = dir + "I" + std::to_string(indexnum);
    //deleting two files
    if( remove( filename1.c_str() ) != 0 ) std::cout << "Error deleting file" << std::endl;
    if( remove( filename2.c_str() ) != 0 ) std::cout << "Error deleting file" << std::endl;
}

//TODO: Refactor into class
//Determines whether an extended lexicon entry should be made for the given posting list
void StaticIndex::shouldGetLexEntry(unsigned int postinglistsize, unsigned int termID, unsigned int indexnum, bool isZindex,
    size_t offset, bool positional, size_t& postingcount, bool& lastlisthadpointer)
{
    //Posting list is large enough to get an entry in the sparse lex
    if(postinglistsize > SPARSE_SIZE) {
        spexlex.insertEntry(termID, indexnum, isZindex, offset, positional);

        postingcount = 0;
        lastlisthadpointer = true;
    }
    //Last posting list had an entry in the sparse lex
    else if(lastlisthadpointer) {
        spexlex.insertEntry(termID, indexnum, isZindex, offset, positional);

        postingcount += postinglistsize;
        lastlisthadpointer = false;
    }
    //Enough postings accumulated to insert a pointer
    else if(postingcount > SPARSE_BETWEEN_SIZE) {
        spexlex.insertEntry(termID, indexnum, isZindex, offset, positional);

        postingcount = 0;
    }
    else {
        postingcount += postinglistsize;
    }
}

std::vector<Posting> StaticIndex::merge_pos_postinglist(std::vector<Posting>& listz, std::vector<Posting>& listi) {
    std::vector<Posting> finallist;
    auto ziter = listz.begin();
    auto iiter = listi.begin();

    while(ziter != listz.end() && iiter != listi.end()) {
        if(ziter->docID < iiter->docID) {
            finallist.push_back(*ziter);
            ziter++;
        }
        else if(iiter->docID < ziter->docID) {
            finallist.push_back(*iiter);
            iiter++;
        }
        else {
            //Newer posting takes priority
            finallist.push_back(*iiter);
            iiter++;
        }
    }
    while(ziter != listz.end()) {
        finallist.push_back(*ziter);
        ziter++;
    }
    while(iiter != listi.end()) {
        finallist.push_back(*iiter);
        iiter++;
    }
    return finallist;
}

std::vector<nPosting> StaticIndex::merge_nonpos_postinglist(std::vector<nPosting>& listz, std::vector<nPosting>& listi) {
    std::vector<nPosting> finallist;
    auto ziter = listz.begin();
    auto iiter = listi.begin();

    while(ziter != listz.end() && iiter != listi.end()) {
        if(ziter->docID < iiter->docID) {
            finallist.push_back(*ziter);
            ziter++;
        }
        else if(iiter->docID < ziter->docID) {
            finallist.push_back(*iiter);
            iiter++;
        }
        else {
            //Newer posting takes priority
            nPosting tempposting = *ziter;
            finallist.push_back(*iiter);
            iiter++;
        }
    }
    while(ziter != listz.end()) {
        finallist.push_back(*ziter);
        ziter++;
    }
    while(iiter != listi.end()) {
        finallist.push_back(*iiter);
        iiter++;
    }
    return finallist;
}