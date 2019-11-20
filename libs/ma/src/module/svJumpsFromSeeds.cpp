/**
 * @file svJumsFromSeeds.cpp
 * @author Markus Schmidt
 */
#include "module/svJumpsFromSeeds.h"
#include <cmath>

using namespace libMA;
void SvJumpsFromSeeds::computeSeeds( Rectangle<nucSeqIndex> xArea, std::shared_ptr<NucSeq> pQuery,
                                     std::shared_ptr<Pack> pRefSeq, std::shared_ptr<Seeds> rvRet )
{
    auto pQuery2 = std::make_shared<NucSeq>( pQuery->fromTo( xArea.xYAxis.start( ), xArea.xYAxis.end( ) ) );
    auto pRef = pRefSeq->vExtract( xArea.xYAxis.start( ), xArea.xYAxis.end( ) );
    auto pSeeds = xHashMapSeeder.execute( pQuery, pRef );

    pRef->vReverseAll( );
    pRef->vSwitchAllBasePairsToComplement( );
    auto pSeeds2 = xHashMapSeeder.execute( pQuery2, pRef );
    // fix seed positions
    for( Seed& rSeed : *pSeeds )
    {
        rSeed.uiPosOnReference += xArea.xXAxis.start( );
        rSeed.iStart += xArea.xYAxis.start( );
        assert( rSeed.end( ) < pQuery->length( ) );
    } // for
    for( Seed& rSeed : *pSeeds2 )
    {
        rSeed.bOnForwStrand = false;
        // undo reversion of reference
        rSeed.uiPosOnReference = xArea.xXAxis.size( ) - rSeed.uiPosOnReference - 1;
        rSeed.uiPosOnReference += xArea.xXAxis.start( );
        rSeed.iStart += xArea.xYAxis.start( );
        assert( rSeed.end( ) < pQuery->length( ) );
    } // for

    rvRet->append( pSeeds );
    rvRet->append( pSeeds2 );
} // method

std::shared_ptr<Seeds> SvJumpsFromSeeds::computeSeeds( Rectangle<nucSeqIndex>& xArea, std::shared_ptr<NucSeq> pQuery,
                                                       std::shared_ptr<Pack> pRefSeq )
{
    auto pSeeds = std::make_shared<Seeds>( );
    if( xArea.xXAxis.start( ) < (nucSeqIndex)pSelectedSetting->xMaxSizeReseed->get( ) )
        computeSeeds( xArea, pQuery, pRefSeq, pSeeds );
    else
    {
        // use slightly more than the query size as reference section length
        auto uiRefSecLen =
            std::min( xArea.xYAxis.start( ) * 4 / 3, (nucSeqIndex)pSelectedSetting->xMaxSizeReseed->get( ) / 2 );
        computeSeeds(
            Rectangle<nucSeqIndex>( xArea.xXAxis.start( ), xArea.xYAxis.start( ), uiRefSecLen, xArea.xYAxis.size( ) ),
            pQuery, pRefSeq, pSeeds );
        computeSeeds( Rectangle<nucSeqIndex>( xArea.xXAxis.end( ) - uiRefSecLen, xArea.xYAxis.start( ), uiRefSecLen,
                                              xArea.xYAxis.size( ) ),
                      pQuery, pRefSeq, pSeeds );
    } // else

    if( pSeeds->size( ) == 0 )
        return pSeeds;

    // turn k-mers into maximally extended seeds
    return xSeedLumper.execute( pSeeds );
} // method

void SvJumpsFromSeeds::makeJumpsByReseedingRecursive( Seed& rLast, Seed& rNext, std::shared_ptr<NucSeq> pQuery,
                                                      std::shared_ptr<Pack> pRefSeq,
                                                      std::shared_ptr<ContainerVector<SvJump>>& pRet )
{
    // returns a (reference pos, query pos, width [on reference], height [on query])
    auto xRectangle = getPositionsForSeeds( rLast, rNext, pQuery->length( ) );

    // check if there is enough space to reseed
    if( xHashMapSeeder.minSeedSize( ) > xRectangle.xXAxis.size( ) ||
        xHashMapSeeder.minSeedSize( ) > xRectangle.xYAxis.size( ) )
        return;

    // reseed and recursiveley call this function again
    std::shared_ptr<Seeds> vSeeds = computeSeeds( xRectangle, pQuery, pRefSeq );
    std::sort( vSeeds->begin( ), vSeeds->end( ),
                []( const Seed& rA, const Seed& rB ) { return rA.start( ) < rB.start( ); } );

    // this auto locks using the DB; seeds need to be sorted by query position
    xCoverageInserter.insert( *vSeeds, pQuery->length( ) );

    if( vSeeds->size( ) > 0 )
    {
        Seed& rCurr = rLast;
        for( auto& rSeed : *vSeeds )
        {
            makeJumpsByReseedingRecursive( rCurr, rSeed, pQuery, pRefSeq, pRet );
            rCurr = rSeed;
        } // for
        makeJumpsByReseedingRecursive( rCurr, rNext, pQuery, pRefSeq, pRet );

        // we found at least one seed so we do not need to create a jump between rLast and rNext
        return;
    } // if
    // compute a SvJump and terminate the recursion
    if( &rLast == &xDummySeed || &rNext == &xDummySeed )
    {
        // we have to insert a dummy jump if the seed is far enough from the end/start of the query
        if( &rLast != &xDummySeed && rLast.start( ) > uiMinDistDummy )
            pRet->emplace_back( pSelectedSetting, rLast, pQuery->length( ), true, pQuery->iId );
        if( &rNext != &xDummySeed && vSeeds->back( ).end( ) + uiMinDistDummy < pQuery->length( ) )
            pRet->emplace_back( pSelectedSetting, rNext, pQuery->length( ), false, pQuery->iId );
    } // if
    else
    {
        // we have to insert a jump between two seeds
        if( SvJump::validJump( rLast, rNext, true ) )
            pRet->emplace_back( pSelectedSetting, rLast, rNext, true, pQuery->iId );
        if( SvJump::validJump( rLast, rNext, false ) )
            pRet->emplace_back( pSelectedSetting, rLast, rNext, false, pQuery->iId );
    } // else
} // function

std::shared_ptr<ContainerVector<SvJump>> SvJumpsFromSeeds::execute( std::shared_ptr<SegmentVector> pSegments,
                                                                    std::shared_ptr<Pack>
                                                                        pRefSeq,
                                                                    std::shared_ptr<FMIndex>
                                                                        pFM_index,
                                                                    std::shared_ptr<NucSeq>
                                                                        pQuery )
{
    AlignedMemoryManager xMemoryManager;
    auto pRet = std::make_shared<ContainerVector<SvJump>>( );

    // sort seeds by query position:
    std::sort( pSegments->begin( ), pSegments->end( ),
               []( const Segment& rA, const Segment& rB ) { return rA.start( ) < rB.start( ); } );
    Seeds vSeeds;
    // avoid multiple allocations (we can only guess the actual number of seeds here)
    vSeeds.reserve( pSegments->size( ) * 2 );

    // filter ambiguous segments -> 1; don't -> 0
#if 1
    /**
     * This filters segments that occur multiple times on the reference:
     * For each of those segments extract all seeds.
     * Then pick the one seed that is closest, on the reference, to:
     *  - either the last (on query) unique seed
     *  - or the next (on query) unique seed
     * This drastically reduces the number of seeds.
     * Further, it shall help dealing with ambiguous regions by eliminating all but the seeds most likely to fit
     * into a chain of seeds
     */
    std::vector<Segment*> vTemp;
    size_t uiNumSeedsTotal = 0;
    int64_t iLastUniqueRefPos = -1;
    for( Segment& rSegment : *pSegments )
    {
        if( rSegment.size( ) < uiMinSeedSizeSV )
            continue;
        uiNumSeedsTotal += rSegment.saInterval( ).size( );

        if( rSegment.saInterval( ).size( ) == 1 )
            rSegment.forEachSeed( *pFM_index, [&]( Seed& rS ) {
                // deal with vTemp
                int64_t iCurrUniqueRefPos = (int64_t)rS.start_ref( );
                for( Segment* pSeg : vTemp )
                {
                    Seed xBest;
                    int64_t iMinDist = -1;
                    pSeg->forEachSeed( *pFM_index, [&]( Seed& rS ) {
                        int64_t iStart = (int64_t)rS.start_ref( );
                        int64_t iDist = std::abs( iStart - iCurrUniqueRefPos );
                        // if the segment is not the first segment this triggers
                        if( iLastUniqueRefPos != -1 )
                            iDist = std::min( iDist, std::abs( iStart - iLastUniqueRefPos ) );
                        if( iMinDist == -1 || iDist < iMinDist )
                        {
                            xBest = rS;
                            iMinDist = iDist;
                        } // if
                        return true;
                    } ); // forEachSeed function call
                    assert( iMinDist != -1 );
                    vSeeds.push_back( xBest );
                } // for
                vTemp.clear( );

                iLastUniqueRefPos = (int64_t)rS.end_ref( );
                vSeeds.push_back( rS );
                return true;
            } ); // forEachSeed function call \ if
        else
            vTemp.push_back( &rSegment );
    } // for
    // seeds at the end
    for( Segment* pSeg : vTemp )
    {
        Seed xBest;
        int64_t iMinDist = -1;
        pSeg->forEachSeed( *pFM_index, [&]( Seed& rS ) {
            int64_t iStart = (int64_t)rS.start_ref( );
            int64_t iDist = std::abs( iStart - iLastUniqueRefPos );
            if( iMinDist == -1 || iDist < iMinDist )
            {
                xBest = rS;
                iMinDist = iDist;
            } // if
            return true;
        } ); // forEachSeed function call
        assert( iMinDist != -1 );
        vSeeds.push_back( xBest );
    } // for

    {
        std::lock_guard<std::mutex> xGuard( xLock );
        uiNumSeedsEliminatedAmbiguityFilter += uiNumSeedsTotal - vSeeds.size( );
        uiNumSeedsKeptAmbiguityFilter += vSeeds.size( );
    } // scope xGuard

#else
    pSegments->emplaceAllEachSeeds( *pFM_index, pQuery->length( ), uiMaxAmbiguitySv, uiMinSeedSizeSV, vSeeds,
                                    [&]( ) { return true; } );
#endif

    // insert coverage
    xCoverageInserter.insert( vSeeds, pQuery->length( ) );

    // actually compute the jumps
    Seed& rCurr = xDummySeed;
    for( auto& rSeed : vSeeds )
    {
        makeJumpsByReseedingRecursive( rCurr, rSeed, pQuery, pRefSeq, pRet );
        rCurr = rSeed;
    } // for
    makeJumpsByReseedingRecursive( rCurr, xDummySeed, pQuery, pRefSeq, pRet );

    return pRet;
} // method


#ifdef WITH_PYTHON
void exportSvJumpsFromSeeds( py::module& rxPyModuleId )
{
    exportModule<SvJumpsFromSeeds, int64_t, std::shared_ptr<SV_DB>, std::shared_ptr<Pack>>(
        rxPyModuleId, "SvJumpsFromSeeds", []( auto&& x ) { x.def( "commit", &SvJumpsFromSeeds::commit ); } );
} // function
#endif