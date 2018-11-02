/**
 * @file harmonization.h
 * @brief Implements the harmonization @ref Module "module"
 * @author Markus Schmidt
 */
#ifndef LINESWEEP_H
#define LINESWEEP_H

#include "container/segment.h"
#include "container/soc.h"
#include "module/module.h"

//#define PRINT_BREAK_CRITERIA(x) x
#define PRINT_BREAK_CRITERIA( x )

#define USE_RANSAC ( 1 )
#define PI 3.14159265

namespace libMA
{
/**
 * @brief Implements the Harmonization algorithm.
 * @ingroup module
 * @details
 * Removes all contradicting seeds.
 * This should only be used in combination with the StripOfConsideration module.
 */
class HarmonizationSingle : public Module<Seeds, false, Seeds, NucSeq, FMIndex>
{
  private:
    /**
     * @brief The shadow of a Seed.
     * @details
     * Each perfect match "casts a shadow" at the left and right border of the strip.
     * Each shadow is stored in one of these data structures.
     */
    class ShadowInterval : public Interval<int64_t>
    {
      public:
        Seeds::iterator pSeed;

        /**
         * @brief Creates a new shadow.
         * @details
         * The linesweep algorithm disables seeds.
         * Therefore the iterator is required in order to delete the respective seed from its list.
         */
        ShadowInterval( int64_t iBegin, int64_t iSize, Seeds::iterator pSeed )
            : Interval( iBegin, iSize ), pSeed( pSeed )
        {} // constructor

        /**
         * @brief Copy constructor
         */
        ShadowInterval( const ShadowInterval& rOther ) : Interval( rOther ), pSeed( rOther.pSeed )
        {} // copy constructor

        bool within( const ShadowInterval& rOther )
        {
            return start( ) >= rOther.start( ) && end( ) <= rOther.end( );
        } // function
    }; // class


    /**
     * @brief Implements the linesweep algorithm.
     * @details
     * The algorithm has to be run on left and right shadows,
     * therefore it is provided as individual function.
     */
    std::shared_ptr<std::vector<std::tuple<Seeds::iterator, nucSeqIndex, nucSeqIndex>>> EXPORTED
    linesweep( std::shared_ptr<std::vector<std::tuple<Seeds::iterator, nucSeqIndex, nucSeqIndex>>> pShadows,
               const int64_t uiRStart, const double fAngle );

    inline double deltaDistance( const Seed& rSeed, const double fAngle, const int64_t uiRStart )
    {
        double y = rSeed.start_ref( ) + rSeed.start( ) / std::tan( PI / 2 - fAngle );
        double x = ( y - uiRStart ) * std::sin( fAngle );
        double x_1 = rSeed.start( ) / std::sin( PI / 2 - fAngle );

        return std::abs( x - x_1 );
    } // method

    std::pair<double, double> ransac( std::shared_ptr<libMA::Seeds> pSeedsIn );

    std::shared_ptr<libMA::Seeds> applyLinesweeps( std::shared_ptr<libMA::Seeds> pSeedsIn
#if DEBUG_LEVEL > 0
                                                   ,
                                                   bool bRecord
#endif
    );

    std::shared_ptr<Seeds> applyFilters( std::shared_ptr<Seeds>& pIn ) const;

    std::shared_ptr<ContainerVector<std::shared_ptr<Seeds>>> cluster( std::shared_ptr<Seeds> pSeedsIn,
                                                                      nucSeqIndex uiQLen ) const;

  public:
    /**
     * @brief true: estimate all possible position as matches in the gap cost filter
     * @details
     * After the linesweep picks a subset of all seeds so that the overall score
     * becomes optimal.
     * This filter has a linear time complexity.
     * It estimates the penalty for gaps between seeds.
     * We have two options here:
     * True -> the gapcost is estimated optimistically (as small as possible)
     * False -> we assume that the score for matches/missmatches is roughly equal within the gap
     *
     * @note not beeing optimistic here has negative affect on the accuracy
     * but improves runtime significantly
     */
    bool optimisticGapEstimation = defaults::bOptimisticGapEstimation;

    /// @brief If the seeds cover less that x percent of the query we use SW,
    /// otherwise we fill in the gaps.
    double fMinimalQueryCoverage = defaults::fMinimalQueryCoverage;

    /// @brief Stop the SoC extraction if the harmonized score drops more than x.
    double fScoreTolerace = defaults::fScoreTolerace;

    /// @brief Extract at least x SoCs.
    size_t uiMinTries = defaults::uiMinTries;

    /// @brief Lookahead for the equality break criteria.
    size_t uiMaxEqualScoreLookahead = defaults::uiMaxEqualScoreLookahead;

    /// @brief Consider two scores equal if they do not differ by more than x (relative to the total
    /// score).
    float fScoreDiffTolerance = defaults::fScoreDiffTolerance;

    /// @brief switch between the two break criteria based on weather the query len is larger than
    /// x.
    nucSeqIndex uiSwitchQLen = defaults::uiSwitchQLen;

    /// @brief minimum accumulated seed length after the harmonization as absolute value.
    nucSeqIndex uiCurrHarmScoreMin = defaults::uiCurrHarmScoreMin;
    /// @brief minimum accumulated seed length after the harmonization relative to query length.
    float fCurrHarmScoreMinRel = defaults::fGiveUp;

    bool bDoHeuristics = !defaults::bDisableHeuristics;

    bool bDoGapCostEstimationCutting = !defaults::bDisableGapCostEstimationCutting;

    double dMaxDeltaDist = defaults::dMaxDeltaDist;

    nucSeqIndex uiMinDeltaDist = defaults::uiMinDeltaDist;

    double dMaxSVRatio = defaults::dMaxSVRatio;

    int64_t iMinSVDistance = defaults::iMinSVDistance;

    nucSeqIndex uiMaxGapArea = defaults::uiMaxGapArea;

    size_t uiSVPenalty = defaults::uiSVPenalty;

    nucSeqIndex uiMaxDeltaDistanceInCLuster = defaults::uiMaxDeltaDistanceInCLuster;

    HarmonizationSingle( )
    {} // default constructor

    // overload
    virtual std::shared_ptr<Seeds> EXPORTED execute( std::shared_ptr<Seeds> pPrimaryStrand,
                                                     std::shared_ptr<NucSeq> pQuery,
                                                     std::shared_ptr<FMIndex> pFMIndex );

}; // class


class Harmonization : public Module<ContainerVector<std::shared_ptr<Seeds>>, false, SoCPriorityQueue, NucSeq, FMIndex>
{
  public:
    HarmonizationSingle xSingle;


    /// @brief Extract at most x SoCs.
    size_t uiMaxTries = defaults::uiMaxTries;

    Harmonization( )
    {} // default constructor

    // overload
    virtual std::shared_ptr<ContainerVector<std::shared_ptr<Seeds>>>
        EXPORTED execute( std::shared_ptr<SoCPriorityQueue> pSoCSsIn, std::shared_ptr<NucSeq> pQuery,
                          std::shared_ptr<FMIndex> pFMIndex )
    {
        unsigned int uiNumTries = 0;

        auto pSoCs = std::make_shared<ContainerVector<std::shared_ptr<Seeds>>>( );

        for( ; uiNumTries < uiMaxTries && !pSoCSsIn->empty( ); uiNumTries++ )
        {
            auto pSoC = pSoCSsIn->pop( );
            DEBUG( pSoC->pSoCIn = pSoCSsIn; ) // DEBUG
            // @todo changed behaviour here -> harmonization single now returns no more than one seed set
            pSoCs->push_back( xSingle.execute( pSoC, pQuery, pFMIndex ) );
        } // for

        PRINT_BREAK_CRITERIA( if( pSoCSsIn->empty( ) ) std::cout << "exhausted all SoCs" << std::endl;
                              else std::cout << "break after " << uiNumTries << " tries." << std::endl;
                              std::cout << "computed " << pSoCs->size( ) << " SoCs." << std::endl; )

        return pSoCs;
    } // method
}; // class


class SeedLumping : public Module<Seeds, false, Seeds>
{
  public:
    SeedLumping( )
    {} // default constructor

    // overload
    virtual std::shared_ptr<Seeds> EXPORTED execute( std::shared_ptr<Seeds> pIn )
    {
        auto pRet = std::make_shared<Seeds>( );

        int64_t iDelta = pIn->front( ).start_ref( ) - (int64_t)pIn->front( ).start( );
        pRet->push_back( pIn->front( ) );

        auto xIt = pIn->begin( ) + 1;
        while( xIt != pIn->end( ) )
        {
            int64_t iNewDelta = xIt->start_ref( ) - (int64_t)xIt->start( );
            if( iDelta == iNewDelta && xIt->start( ) <= pRet->back( ).end( ) )
            {
                if( xIt->end( ) > pRet->back( ).end( ) )
                    pRet->back( ).iSize += xIt->end( ) - pRet->back( ).end( );
                assert( pRet->back( ).end( ) >= xIt->end( ) );
                assert( pRet->back( ).end_ref( ) >= xIt->end_ref( ) );
            } // if
            else
            {
                pRet->push_back( *xIt );
                iDelta = iNewDelta;
            } // else
            xIt++;
        } // while

        return pRet;
    } // method
}; // class


} // namespace libMA

#ifdef WITH_PYTHON
/**
 * @brief Exposes the Harmonization @ref Module "module" to boost python.
 * @ingroup export
 */
void exportHarmonization( );
#endif

#endif