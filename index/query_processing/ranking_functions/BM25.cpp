#include "BM25.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

/*  Needs:
    *term frequency in document
    length of document in words
    average document length
    total number of documents in index
    number of documents containing term
*/

double IDF(double totaldocs, unsigned int docscontaining) {
    double numerator = totaldocs - docscontaining + 0.5;
    double denominator = docscontaining + 0.5;
    return log10(numerator / denominator);
}

double BM25(std::vector<unsigned int>& freq, std::vector<unsigned int>& docscontaining, double doclength, double avgdoclength, double totaldocs) {

    //free parameters
    double k = 1.2;
    double b = 0.75;

    if(freq.size() != docscontaining.size()) {
        std::string x = "Error, freq and docscontaining arrays mismatched in size: ";
        x += freq.size();
        x += ", ";
        x += docscontaining.size();
        throw std::invalid_argument(x);
    }

    double score = 0;

    for(size_t i = 0; i < freq.size(); i++) {
        double numerator = freq[i] * (k + 1);
        double denominator = freq[i] + k * (1 - b + b * (doclength / avgdoclength));
        score += IDF(totaldocs, docscontaining[i]) * (numerator/denominator);
    }

    return score;
}
