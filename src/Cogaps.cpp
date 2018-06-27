#include "GapsDispatcher.h"
#include "math/SIMD.h"

#include <Rcpp.h>

static RowMatrix convertRMatrix(const Rcpp::NumericMatrix &rmat)
{
    RowMatrix mat(rmat.nrow(), rmat.ncol());
    for (unsigned i = 0; i < mat.nRow(); ++i)
    {
        for (unsigned j = 0; j < mat.nCol(); ++j)
        {
            mat(i,j) = rmat(i,j);
        }
    }
    return mat;
}

template <class Matrix>
static Rcpp::NumericMatrix createRMatrix(const Matrix &mat)
{
    Rcpp::NumericMatrix rmat(mat.nRow(), mat.nCol());
    for (unsigned i = 0; i < mat.nRow(); ++i)
    {
        for (unsigned j = 0; j < mat.nCol(); ++j)
        {
            rmat(i,j) = mat(i,j);
        }
    }
    return rmat;
}

template <class T>
static Rcpp::List cogapsRun(T data, unsigned nPatterns,
unsigned maxIter, unsigned outputFrequency, unsigned seed, float alphaA,
float alphaP, float maxGibbsMassA, float maxGibbsMassP, bool messages,
bool singleCell, unsigned nCores)
{
    GapsDispatcher dispatcher(seed);

    dispatcher.setNumPatterns(nPatterns);
    dispatcher.setMaxIterations(maxIter);
    dispatcher.setOutputFrequency(outputFrequency);
    
    dispatcher.setAlpha(alphaA, alphaP);
    dispatcher.setMaxGibbsMass(maxGibbsMassA, maxGibbsMassP);

    dispatcher.printMessages(messages);
    dispatcher.singleCell(singleCell);
    dispatcher.setNumCoresPerSet(nCores);
    
    dispatcher.loadData(data);

    GapsResult result(dispatcher.run());
    return Rcpp::List::create(
        Rcpp::Named("Amean") = createRMatrix(result.Amean),
        Rcpp::Named("Pmean") = createRMatrix(result.Pmean),
        Rcpp::Named("Asd") = createRMatrix(result.Asd),
        Rcpp::Named("Psd") = createRMatrix(result.Psd)
    );
}

// [[Rcpp::export]]
Rcpp::List cogaps_cpp_from_file(const std::string &data, const std::string &unc,
unsigned nPatterns, unsigned maxIterations, unsigned outputFrequency,
uint32_t seed, float alphaA, float alphaP, float maxGibbsMassA,
float maxGibbsMassP, bool messages, bool singleCellRNASeq,
const std::string &checkpointOutFile)
{
    return cogapsRun(data, nPatterns, maxIter, outputFrequency, seed,
        alphaA, alphaP, maxGibbsMassA, maxGibbsMassP, messages,
        singleCellRNASeq, 1);
}

// [[Rcpp::export]]
Rcpp::List cogaps_cpp(const Rcpp::NumericMatrix &data,
const Rcpp::NumericMatrix &unc, unsigned nPatterns, unsigned maxIterations,
unsigned outputFrequency, uint32_t seed, float alphaA, float alphaP,
float maxGibbsMassA, float maxGibbsMassP, bool messages, bool singleCell,
const std::string &checkpointOutFile)
{
    return cogapsRun(convertRMatrix(data), convertRMatrix(unc), nPatterns,
        maxIterations, outputFrequency, seed, alphaA, alphaP, maxGibbsMassA,
        maxGibbsMassP, messages, singleCell, nCores);
}

// [[Rcpp::export]]
std::string getBuildReport_cpp()
{
    return buildReport();
}
