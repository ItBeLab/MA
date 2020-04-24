/**
 * @file svJumpsFromSeeds.h
 * @brief Implements a way to compute SV-jumps from seeds.
 * @author Markus Schmidt
 */
#pragma once

#include "ma/container/segment.h"
#include "ma/module/binarySeeding.h"
#include "ma/module/harmonization.h"
#include "ma/module/hashMapSeeding.h"
#include "ma/module/needlemanWunsch.h"
#include "ms/module/module.h"
#include "msv/container/svJump.h"
#include "msv/container/sv_db/tables/svJump.h"
#include "msv/util/statisticSequenceAnalysis.h"

namespace libMSV
{
class PerfectMatch;

#define complement( x ) ( uint8_t ) NucSeq::nucleotideComplement( x )


/**
 * @brief Computes Sv-Jumps from a given seed set
 * @note WARNING: DO USE EACH INSTANCE OF THIS MODULE ONLY ONCE IN THE COMPUTATIONAL GRAPH
 */
class SvJumpsFromSeeds
    : public libMS::Module<libMS::ContainerVector<SvJump>, false, SegmentVector, Pack, FMIndex, NucSeq>
{
  public:
    const size_t uiMinSeedSizeSV;
    const size_t uiMaxAmbiguitySv;
    const int64_t iMaxSizeReseed;
    const bool bDoDummyJumps;
    const size_t uiMinDistDummy;
    const size_t uiMaxDistDummy;
    SeedLumping xSeedLumper;
    NeedlemanWunsch xNW;
    ParlindromeFilter xParlindromeFilter;
    double dMaxSequenceSimilarity = 0.2;

    std::mutex xLock;
    size_t uiNumSeedsEliminatedAmbiguityFilter = 0;
    size_t uiNumSeedsKeptAmbiguityFilter = 0;

    /// used to indicate that there is no seed for on of the parameters in the recursive call.
    Seed xDummySeed;

    double dExtraSeedingAreaFactor = 1.5;
    // depends on sequencer technique
    // lower -> less seed noise (via larger min size for seeds); worse breakpoint recognition
    // higher -> more seed noise; better breakpoint recognition (via smaller seeds)
    double dProbabilityForRandomMatch = 0.01;

    /**
     * @brief Initialize a SvJumpsFromSeeds Module
     */
    SvJumpsFromSeeds( const ParameterSetManager& rParameters, std::shared_ptr<Pack> pRefSeq )
        : uiMinSeedSizeSV( rParameters.getSelected( )->xMinSeedSizeSV->get( ) ),
          uiMaxAmbiguitySv( rParameters.getSelected( )->xMaxAmbiguitySv->get( ) ),
          iMaxSizeReseed( (int64_t)rParameters.getSelected( )->xMaxSizeReseed->get( ) ),
          bDoDummyJumps( rParameters.getSelected( )->xDoDummyJumps->get( ) ),
          uiMinDistDummy( rParameters.getSelected( )->xMinDistDummy->get( ) ),
          uiMaxDistDummy( rParameters.getSelected( )->xMaxDistDummy->get( ) ),
          xSeedLumper( rParameters ),
          xNW( rParameters ),
          xParlindromeFilter( rParameters )
    {} // constructor

    ~SvJumpsFromSeeds( )
    {
        size_t uiTotal = uiNumSeedsKeptAmbiguityFilter + uiNumSeedsEliminatedAmbiguityFilter;
        if( uiTotal > 0 )
            std::cout << "~SvJumpsFromSeeds: ambiguity filter kept and eliminated " << uiNumSeedsKeptAmbiguityFilter
                      << " and " << uiNumSeedsEliminatedAmbiguityFilter << " seeds respectively. " << std::endl
                      << "\tThats " << ( 1000 * uiNumSeedsKeptAmbiguityFilter / uiTotal ) / 10.0 << "% and "
                      << ( 1000 * uiNumSeedsEliminatedAmbiguityFilter / uiTotal ) / 10.0 << "% respectively."
                      << std::endl;
    } // destructor


    /**
     * @brief computes area between two seeds
     * @details
     * 1) For reseeding between two seeds we need the appropriate interval on query and reference.
     * 2) For reseeding before/after a seed we need the appropiate rectangle as well.
     *      The width of this rectangle will be its height * dExtraSeedingAreaFactor (@todo move to settings).
     *      This case is triggered by passing xDummySeed as rLast or rNext.
     *
     * If the rectangle's width is more than xMaxSizeReseed (see settings) this will return
     * two rectangles using case 2; one for each seed.
     */
    std::pair<geom::Rectangle<nucSeqIndex>, geom::Rectangle<nucSeqIndex>> DLL_PORT( MSV )
        getPositionsForSeeds( Seed& rLast, Seed& rNext, nucSeqIndex uiQStart, nucSeqIndex uiQEnd,
                              std::shared_ptr<Pack> pRefSeq );

    /**
     * @brief computes how much percent of the rectangles xRects is filled by seeds in pvSeeds.
     * @details
     * Assumes that the seeds are completeley within the rectangles.
     */
    float rectFillPercentage( std::shared_ptr<Seeds> pvSeeds,
                              std::pair<geom::Rectangle<nucSeqIndex>, geom::Rectangle<nucSeqIndex>> xRects )
    {
        nucSeqIndex uiSeedSize = 0;
        for( auto& rSeed : *pvSeeds )
            uiSeedSize += rSeed.size( );
        if( xRects.first.xXAxis.size( ) * xRects.first.xYAxis.size( ) +
                xRects.second.xXAxis.size( ) * xRects.second.xYAxis.size( ) ==
            0 )
            return 0;
        return uiSeedSize / (float)( xRects.first.xXAxis.size( ) * xRects.first.xYAxis.size( ) +
                                     xRects.second.xXAxis.size( ) * xRects.second.xYAxis.size( ) );
    } // method

    /// @brief this class exists mereley to expose the return value of execute_helper_py to python
    class HelperRetVal
    {
      public:
        std::shared_ptr<Seeds> pSeeds;
        std::vector<size_t> vLayerOfSeeds;
        std::vector<bool> vParlindromeSeed;
        std::vector<geom::Rectangle<nucSeqIndex>> vRectangles;
        std::vector<double> vRectangleFillPercentage;
        std::vector<size_t> vRectangleReferenceAmbiguity;
        std::vector<bool> vRectangleUsedDp;

        HelperRetVal( ) : pSeeds( std::make_shared<Seeds>( ) ){};
    }; // class

    /**
     * @brief computes all seeds within xArea.
     * @details
     * computes all SvJumpsFromSeeds::getKMerSizeForRectangle( xArea ) sized seeds within xArea and appends
     * them to rvRet.
     * @note This is a helper function. Use the other computeSeeds.
     */
    void DLL_PORT( MSV )
        computeSeeds( geom::Rectangle<nucSeqIndex>& xArea, std::shared_ptr<NucSeq> pQuery,
                      std::shared_ptr<Pack> pRefSeq, std::shared_ptr<Seeds> rvRet, HelperRetVal* pOutExtra );
    /**
     * @brief computes all seeds within the given areas.
     * @details
     * computes all seeds larger equal to SvJumpsFromSeeds::getKMerSizeForRectangle( xArea ) within xAreas.first and
     * xAreas.second seperately.
     */
    std::shared_ptr<Seeds> DLL_PORT( MSV )
        computeSeeds( std::pair<geom::Rectangle<nucSeqIndex>, geom::Rectangle<nucSeqIndex>>& xAreas,
                      std::shared_ptr<NucSeq> pQuery, std::shared_ptr<Pack> pRefSeq, HelperRetVal* pOutExtra );

    /**
     * @brief computes the SV jumps between the two given seeds.
     * @details
     * Recursiveley computes additional seeds, of statistically relevant sizes, between rLast and rNext.
     *
     * pSeeds, pvLayerOfSeeds and pvRectanglesOut are ignored if they are nullptrs,
     * otherwise the computed seeds, their layers and all reseeding rectangles are appended, respectiveley.
     */
    void DLL_PORT( MSV ) makeJumpsByReseedingRecursive( Seed& rLast, Seed& rNext, std::shared_ptr<NucSeq> pQuery,
                                                        std::shared_ptr<Pack> pRefSeq,
                                                        std::shared_ptr<libMS::ContainerVector<SvJump>>& pRet,
                                                        size_t uiLayer, HelperRetVal* pOutExtra );

    /**
     * @brief computes all SV jumps between the given seeds.
     * @details
     * Filters the initial seeds by their distance to the next unique seed on query:
     *      Keeps only the seeds thats closest (on the reference) to the next/last unique seed on the query.
     *
     * Uses makeJumpsByReseedingRecursive().
     */
    std::shared_ptr<libMS::ContainerVector<SvJump>> DLL_PORT( MSV )
        execute_helper( std::shared_ptr<SegmentVector> pSegments,
                        std::shared_ptr<Pack>
                            pRefSeq,
                        std::shared_ptr<FMIndex>
                            pFM_index,
                        std::shared_ptr<NucSeq>
                            pQuery,
                        HelperRetVal* pOutExtra );

    inline HelperRetVal execute_helper_py( std::shared_ptr<SegmentVector> pSegments,
                                           std::shared_ptr<Pack>
                                               pRefSeq,
                                           std::shared_ptr<FMIndex>
                                               pFM_index,
                                           std::shared_ptr<NucSeq>
                                               pQuery )
    {
        HelperRetVal xRet;
        execute_helper( pSegments, pRefSeq, pFM_index, pQuery, &xRet );
        return xRet;
    } // method

    virtual std::shared_ptr<libMS::ContainerVector<SvJump>> DLL_PORT( MSV )
        execute( std::shared_ptr<SegmentVector> pSegments,
                 std::shared_ptr<Pack>
                     pRefSeq,
                 std::shared_ptr<FMIndex>
                     pFM_index,
                 std::shared_ptr<NucSeq>
                     pQuery );
}; // class

class RecursiveReseeding : public libMS::Module<Seeds, false, SegmentVector, Pack, FMIndex, NucSeq>
{
    SvJumpsFromSeeds xJumpsFromSeeds;

  public:
    RecursiveReseeding( const ParameterSetManager& rParameters, std::shared_ptr<Pack> pRefSeq )
        : xJumpsFromSeeds( rParameters, pRefSeq )
    {}

    virtual std::shared_ptr<Seeds> execute( std::shared_ptr<SegmentVector> pSegments,
                                            std::shared_ptr<Pack>
                                                pRefSeq,
                                            std::shared_ptr<FMIndex>
                                                pFM_index,
                                            std::shared_ptr<NucSeq>
                                                pQuery )
    {
        return xJumpsFromSeeds.execute_helper_py( pSegments, pRefSeq, pFM_index, pQuery ).pSeeds;
    }
}; // class

}; // namespace libMSV

#ifdef WITH_PYTHON
/**
 * @brief exports the SvJumpsFromSeeds @ref libMSV::Module "module" to python.
 * @ingroup export
 */
void exportSvJumpsFromSeeds( libMS::SubmoduleOrganizer& xOrganizer );
#endif