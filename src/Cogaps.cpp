#include "GibbsSampler.h"
#include "Matrix.h"

#include <Rcpp.h>
#include <ctime>
#include <boost/date_time/posix_time/posix_time.hpp>

typedef std::vector<Rcpp::NumericMatrix> SnapshotList;

enum GapsPhase
{
    GAPS_CALIBRATION,
    GAPS_COOLING,
    GAPS_SAMPLING
};

// declaration order of member variables important!
// initialization depends on it
struct GapsInternalState
{
    Vector chi2VecEquil;
    Vector nAtomsAEquil;
    Vector nAtomsPEquil;

    Vector chi2VecSample;
    Vector nAtomsASample;
    Vector nAtomsPSample;

    unsigned nIterA;
    unsigned nIterP;
    
    unsigned nEquil;
    unsigned nEquilCool;
    unsigned nSample;

    unsigned nSnapshots;
    unsigned nOutputs;
    bool messages;

    unsigned iteration;
    GapsPhase phase;
    uint32_t seed;

    GibbsSampler sampler;

    SnapshotList snapshotsA;
    SnapshotList snapshotsP;

    GapsInternalState(Rcpp::NumericMatrix DMatrix, Rcpp::NumericMatrix SMatrix,
        unsigned nFactor, double alphaA, double alphaP, unsigned nE,
        unsigned nEC, unsigned nS, double maxGibbsMassA,
        double maxGibbsMassP, Rcpp::NumericMatrix fixedPatterns,
        char whichMatrixFixed, bool msg, bool singleCellRNASeq,
        unsigned numOutputs, unsigned numSnapshots, uint32_t in_seed)
            :
        chi2VecEquil(nE), nAtomsAEquil(nE), nAtomsPEquil(nE),
        chi2VecSample(nS), nAtomsASample(nS), nAtomsPSample(nS),
        nIterA(10), nIterP(10), nEquil(nE), nEquilCool(nEC), nSample(nS),
        nSnapshots(numSnapshots), nOutputs(numOutputs), messages(msg),
        iteration(0), phase(GAPS_CALIBRATION), seed(in_seed),
        sampler(DMatrix, SMatrix, nFactor, alphaA, alphaP,
            maxGibbsMassA, maxGibbsMassP, singleCellRNASeq, fixedPatterns,
            whichMatrixFixed)
    {}
};

static void runGibbsSampler(GapsInternalState &state, unsigned nIterTotal,
Vector &chi2Vec, Vector &aAtomVec, Vector &pAtomVec)
{
    for (unsigned i = 0; i < nIterTotal; ++i)
    {
        if (state.phase == GAPS_CALIBRATION)
        {
            state.sampler.setAnnealingTemp(std::min(1.0,
                ((double)i + 2.0) / ((double)state.nEquil / 2.0)));
        }

        for (unsigned j = 0; j < state.nIterA; ++j)
        {
            state.sampler.update('A');
        }

        for (unsigned j = 0; j < state.nIterP; ++j)
        {
            state.sampler.update('P');
        }

        if (state.phase == GAPS_SAMPLING)
        {
            state.sampler.updateStatistics();
            if (state.nSnapshots > 0 && (i + 1) % (nIterTotal / state.nSnapshots) == 0)
            {
                state.snapshotsA.push_back(state.sampler.getNormedMatrix('A'));
                state.snapshotsP.push_back(state.sampler.getNormedMatrix('P'));
            }
        }

        if (state.phase != GAPS_COOLING)
        {
            aAtomVec(i) = state.sampler.totalNumAtoms('A');
            pAtomVec(i) = state.sampler.totalNumAtoms('P');
            chi2Vec(i) = state.sampler.chi2();
            state.nIterA = gaps::random::poisson(std::max(aAtomVec(i), 10.0));
            state.nIterP = gaps::random::poisson(std::max(pAtomVec(i), 10.0));

            if ((i + 1) % state.nOutputs == 0 && state.messages)
            {
                std::string temp = state.phase == GAPS_CALIBRATION ? "Equil: "
                    : "Samp: ";
                std::cout << temp << i + 1 << " of " << nIterTotal
                    << ", Atoms:" << aAtomVec(i) << "(" << pAtomVec(i)
                    << ") Chi2 = " << state.sampler.chi2() << '\n';
            }
        }
    }
}

static Rcpp::List runCogaps(GapsInternalState &state)
{
    if (state.phase == GAPS_CALIBRATION)
    {
        runGibbsSampler(state, state.nEquil, state.chi2VecEquil,
            state.nAtomsAEquil, state.nAtomsPEquil);
        state.phase = GAPS_COOLING;
    }

    if (state.phase == GAPS_COOLING)
    {
        Vector trash(1);
        runGibbsSampler(state, state.nEquilCool, trash, trash, trash);
        state.phase = GAPS_SAMPLING;
    }

    if (state.phase == GAPS_SAMPLING)
    {
        runGibbsSampler(state, state.nSample, state.chi2VecSample,
            state.nAtomsASample, state.nAtomsPSample);
    }

    // combine chi2 vectors
    Vector chi2Vec(state.chi2VecEquil);
    chi2Vec.concat(state.chi2VecSample);

    return Rcpp::List::create(
        Rcpp::Named("Amean") = state.sampler.AMeanRMatrix(),
        Rcpp::Named("Asd") = state.sampler.AStdRMatrix(),
        Rcpp::Named("Pmean") = state.sampler.PMeanRMatrix(),
        Rcpp::Named("Psd") = state.sampler.PStdRMatrix(),
        Rcpp::Named("ASnapshots") = Rcpp::wrap(state.snapshotsA),
        Rcpp::Named("PSnapshots") = Rcpp::wrap(state.snapshotsP),
        Rcpp::Named("atomsAEquil") = state.nAtomsAEquil.rVec(),
        Rcpp::Named("atomsASamp") = state.nAtomsASample.rVec(),
        Rcpp::Named("atomsPEquil") = state.nAtomsPEquil.rVec(),
        Rcpp::Named("atomsPSamp") = state.nAtomsPSample.rVec(),
        Rcpp::Named("chiSqValues") = chi2Vec.rVec(),
        Rcpp::Named("randSeed") = state.seed
    );
}

// [[Rcpp::export]]
Rcpp::List cogaps(Rcpp::NumericMatrix DMatrix, Rcpp::NumericMatrix SMatrix,
unsigned nFactor, double alphaA, double alphaP, unsigned nEquil,
unsigned nEquilCool, unsigned nSample, double maxGibbsMassA,
double maxGibbsMassP, Rcpp::NumericMatrix fixedPatterns,
char whichMatrixFixed, int seed, bool messages, bool singleCellRNASeq,
unsigned numOutputs, unsigned numSnapshots)
{
    // set seed
    uint32_t seedUsed = static_cast<uint32_t>(seed);
    if (seed < 0)
    {
        boost::posix_time::ptime epoch(boost::gregorian::date(1970,1,1)); 
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        boost::posix_time::time_duration diff = now - epoch;
        seedUsed = static_cast<uint32_t>(diff.total_milliseconds() % 1000);
    }
    gaps::random::setSeed(seedUsed);

    // create internal state from parameters
    GapsInternalState state(DMatrix, SMatrix, nFactor, alphaA, alphaP,
        nEquil, nEquilCool, nSample, maxGibbsMassA, maxGibbsMassP,
        fixedPatterns, whichMatrixFixed, messages, singleCellRNASeq,
        numOutputs, numSnapshots, seedUsed);

    // run cogaps from this internal state
    return runCogaps(state);
}

/*
Rcpp::List cogapsFromCheckpoint(const std::string &fileName)
{   
    // open file
    std::ifstream file(fileName);

    // verify magic number
    uint32_t magicNum = 0;
    file.read(reinterpret_cast<char*>(&magicNum), sizeof(uint32_t));
    if (magicNum != 0xCE45D32A)
    {
        std::cout << "invalid checkpoint file" << std::endl;
        return Rcpp::List::create();
    }
    
    // seed random number generator and create internal state
    gaps::random::load(file);
    GapsInternalState state(file);

    // run cogaps from this internal state
    return runCogaps(state);
}
*/
