#ifndef __GAPS_GAPS_STATISTICS_H__
#define __GAPS_GAPS_STATISTICS_H__

#include "GibbsSampler.h"
#include "Matrix.h"

class GapsStatistics
{
private:

    ColMatrix mAMeanMatrix;
    ColMatrix mAStdMatrix;
    RowMatrix mPMeanMatrix;
    RowMatrix mPStdMatrix;
    
    unsigned mStatUpdates;
    unsigned mNumPatterns;

public:

    GapsStatistics(unsigned nRow, unsigned nCol, unsigned nFactor);

    Rcpp::NumericMatrix AMean() const;
    Rcpp::NumericMatrix AStd() const;
    Rcpp::NumericMatrix PMean() const;
    Rcpp::NumericMatrix PStd() const;

    float meanChiSq() const;

    void update(const AmplitudeGibbsSampler &ASampler,
        const PatternGibbsSampler &PSampler);
};

#endif