/**
 * @file seed.h
 * @brief Implements Seed.
 * @author Markus Schmidt
 */
#ifndef SEED_H
#define SEED_H

#include "ms/container/container.h"
#include "util/geom.h"

/// @cond DOXYGEN_SHOW_SYSTEM_INCLUDES
#include <algorithm>
#include <csignal>
#include <list>
/// @endcond

namespace libMA
{
///@brief any index on the query or reference nucleotide sequence is given in this datatype
typedef uint64_t nucSeqIndex;

class Pack;
class NucSeq;

/**
 * @brief A seed.
 * @details
 * A extracted seed, that comprises two intervals, one on the query one on the reference.
 * Both intervals are equal in size.
 * @note the overloaded functions of Interval refer to the Interval on the query.
 * @ingroup container
 */
class Seed : public libMS::Container, public geom::Interval<nucSeqIndex>
{
  public:
    ///@brief the beginning of the match on the reference
    nucSeqIndex uiPosOnReference = 0;
    unsigned int uiAmbiguity;
    size_t uiSoCNt = 0;
    bool bOnForwStrand;
    nucSeqIndex uiDelta = 0;

#if DEBUG_LEVEL > 0
    size_t uiId = 0;
#endif

    /**
     * @brief Creates a new Seed.
     */
    Seed( const nucSeqIndex uiPosOnQuery, const nucSeqIndex uiLength, const nucSeqIndex uiPosOnReference,
          const bool bOnForwStrand )
        : Interval( uiPosOnQuery, uiLength ),
          uiPosOnReference( uiPosOnReference ),
          uiAmbiguity( 0 ),
          uiSoCNt( 0 ),
          bOnForwStrand( bOnForwStrand )
    {} // constructor

    /**
     * @brief Creates a new Seed.
     */
    Seed( const nucSeqIndex uiPosOnQuery, const nucSeqIndex uiLength, const nucSeqIndex uiPosOnReference,
          const unsigned int uiAmbiguity, const bool bOnForwStrand )
        : Interval( uiPosOnQuery, uiLength ),
          uiPosOnReference( uiPosOnReference ),
          uiAmbiguity( uiAmbiguity ),
          uiSoCNt( 0 ),
          bOnForwStrand( bOnForwStrand )
    {} // constructor

    /**
     * @brief Copys from a Seed.
     */
    Seed( const Seed& rOther )
        : Interval( rOther ),
          uiPosOnReference( rOther.uiPosOnReference ),
          uiAmbiguity( rOther.uiAmbiguity ),
          uiSoCNt( rOther.uiSoCNt ),
          bOnForwStrand( rOther.bOnForwStrand ),
          uiDelta( rOther.uiDelta )
    {} // copy constructor

    /**
     * @brief Default Constructor.
     */
    Seed( ) : Interval( )
    {} // default constructor

    /**
     * @brief Returns the beginning of the seed on the reference.
     */
    inline nucSeqIndex start_ref( ) const
    {
        return uiPosOnReference;
    } // function

    /**
     * @brief Returns the end of the seed on the reference.
     */
    inline nucSeqIndex end_ref( ) const
    {
        return uiPosOnReference + size( );
    } // function

    /**
     * @brief Returns the beginning of the seed on the reference.
     */
    inline nucSeqIndex start_ref_cons_rev( ) const
    {
        if( !bOnForwStrand )
            return uiPosOnReference - iSize - 1;
        return uiPosOnReference;
    } // function

    /**
     * @brief Returns the end of the seed on the reference.
     */
    inline nucSeqIndex end_ref_cons_rev( ) const
    {
        if( !bOnForwStrand )
            return uiPosOnReference - 1;
        return uiPosOnReference + size( );
    } // function

    /**
     * @brief Returns the value of the seed.
     * @details
     * A seeds value corresponds to its size.
     */
    inline nucSeqIndex getValue( ) const
    {
        return size( );
    } // function

    /**
     * @brief Copys from another Seed.
     */
    inline Seed& operator=( const Seed& rxOther )
    {
        Interval::operator=( rxOther );
        uiPosOnReference = rxOther.uiPosOnReference;
        uiSoCNt = rxOther.uiSoCNt;
        uiAmbiguity = rxOther.uiAmbiguity;
        bOnForwStrand = rxOther.bOnForwStrand;
        uiDelta = rxOther.uiDelta;
        return *this;
    } // operator

    /*
     * @brief compares two Seeds.
     * @returns true if start and size are equal, false otherwise.
     */
    inline bool operator==( const Seed& rxOther )
    {
        return Interval::operator==( rxOther ) && uiPosOnReference == rxOther.uiPosOnReference &&
               bOnForwStrand == rxOther.bOnForwStrand;
    } // operator

    /*
     * @brief compares two Seeds.
     * Seed ordering is defined by the ordering of their tuples (q,r,l,f).
     * where q is the query position, r is the reference position, l is the length and f is wether the seed is on the
     * forward strand.
     */
    inline bool operator<( const Seed& rxOther )
    {
        if( start( ) != rxOther.start( ) )
            return start( ) < rxOther.start( );
        if( start_ref( ) != rxOther.start_ref( ) )
            return start_ref( ) < rxOther.start_ref( );
        if( size( ) != rxOther.size( ) )
            return size( ) < rxOther.size( );
        if( bOnForwStrand != rxOther.bOnForwStrand )
            return bOnForwStrand;
        if( uiAmbiguity != rxOther.uiAmbiguity )
            return uiAmbiguity < rxOther.uiAmbiguity;
        if( uiSoCNt != rxOther.uiSoCNt )
            return uiSoCNt < rxOther.uiSoCNt;
        if( uiDelta != rxOther.uiDelta )
            return uiDelta < rxOther.uiDelta;
        return false;
    } // operator

    // overload
    inline bool canCast( const std::shared_ptr<libMS::Container>& c ) const
    {
        return std::dynamic_pointer_cast<Seed>( c ) != nullptr;
    } // function

    // overload
    inline std::string getTypeName( ) const
    {
        return "Seed";
    } // function

    // overload
    inline std::shared_ptr<libMS::Container> getType( ) const
    {
        return std::shared_ptr<libMS::Container>( new Seed( ) );
    } // function

}; // class


inline std::ostream& operator<<( std::ostream& os, const Seed& rS )
{
    os << "iStart= " << rS.iStart << " iSize= " << rS.iSize << " uiPosOnReference= " << rS.uiPosOnReference
       << " uiAmbiguity= " << rS.uiAmbiguity << " bOnForwStrand= " << rS.bOnForwStrand << " uiDelta= " << rS.uiDelta;
    return os;
}

class Alignment;
/**
 * @brief Used to store some statistics to each alignment
 * @details
 * Intended for figuring out optimal thresholds.
 */
class AlignmentStatistics
{
  public:
    unsigned int index_of_strip;
    unsigned int num_seeds_in_strip;
    unsigned int anchor_size;
    unsigned int anchor_ambiguity;
    std::weak_ptr<Alignment> pOther;
    bool bFirst; // for paired alignments: is the a alignment for the first mate read of the second one
    bool bSetMappingQualityToZero = false;
    std::string sName;
    nucSeqIndex uiInitialQueryBegin;
    nucSeqIndex uiInitialRefBegin;
    nucSeqIndex uiInitialQueryEnd;
    nucSeqIndex uiInitialRefEnd;

    AlignmentStatistics( )
        : index_of_strip( 0 ),
          num_seeds_in_strip( 0 ),
          anchor_size( 0 ),
          anchor_ambiguity( 0 ),
          pOther( ),
          bFirst( false ),
          sName( "unknown" ),
          uiInitialQueryBegin( 0 ),
          uiInitialRefBegin( 0 ),
          uiInitialQueryEnd( 0 ),
          uiInitialRefEnd( 0 )
    {} // constructor
}; // class

class SVInfo
{
  public:
    /**
     * @details
     * Contains indices of seeds, where a structural variant (deletion or insertion)
     * follows the seed.
     */
    std::vector<size_t> vSeedIndicesOfSVIndels;
}; // class

#if DEBUG_LEVEL > 0
class SoCPriorityQueue; // DEBUG
#endif

/**
 * @brief A set with Seed elements.
 * @details
 * Also holds the summed up score of the seeds within the list.
 * @ingroup Container
 */
class Seeds : public libMS::Container
{
  private:
    typedef std::vector<Seed> TP_VEC;
    ///@brief the content of the seed set
    TP_VEC vContent;

  public:
#if DEBUG_LEVEL > 0
    std::shared_ptr<SoCPriorityQueue> pSoCIn;
#endif

    typedef typename TP_VEC::value_type value_type;
    typedef typename TP_VEC::size_type size_type;
    typedef typename TP_VEC::difference_type difference_type;
    typedef typename TP_VEC::iterator iterator;
    typedef typename TP_VEC::reverse_iterator reverse_iterator;

    nucSeqIndex mem_score = 0;
    // some statistics
    AlignmentStatistics xStats;
    /// @brief is the seed set consistent (set to true after harmonization)
    bool bConsistent = false;

    Seeds( std::initializer_list<value_type> init ) : vContent( init )
    {} // initializer list constructor

    template <class InputIt> Seeds( InputIt xBegin, InputIt xEnd ) : vContent( xBegin, xEnd )
    {} // iterator constructor

    Seeds( ) : vContent( )
    {} // default constructor

    Seeds( const std::shared_ptr<Seeds> pOther ) : vContent( pOther->vContent )
    {} // constructor

    Seeds( size_t numElements ) : vContent( numElements )
    {} // constructor

    /// @brief returns the sum off all scores within the list
    inline nucSeqIndex getScore( ) const
    {
        nucSeqIndex iRet = 0;
        for( const Seed& rS : *this )
            iRet += rS.getValue( );
        return iRet;
    } // function

    /// @brief append another seed set
    inline void append( const std::shared_ptr<Seeds> pOther )
    {
        for( Seed& rS : pOther->vContent )
            push_back( rS );
    } // method

    /// @brief return wether this seed set is larger according to getScore()
    inline bool larger( const std::shared_ptr<libMS::Container> pOther ) const
    {
        const std::shared_ptr<Seeds> pSeeds = std::dynamic_pointer_cast<Seeds>( pOther );
        if( pSeeds == nullptr )
            return true;
        return getScore( ) > pSeeds->getScore( );
    } // method

    // setter
    inline value_type& operator[]( size_type uiI )
    {
        return vContent[ uiI ];
    } // operator

    // getter
    inline const value_type& operator[]( size_type uiI ) const
    {
        return vContent[ uiI ];
    } // operator

    inline void push_back( const value_type& value )
    {
        vContent.push_back( value );
    } // method

    inline size_type size( void ) const
    {
        return vContent.size( );
    } // method

    inline void resize( size_type uiCount )
    {
        vContent.resize( uiCount );
    } // method

    inline bool mainStrandIsForward( ) const
    {
        size_type uiForw = 0;
        size_type uiRev = 0;
        for( const Seed& rS : vContent )
        {
            if( rS.bOnForwStrand )
                uiForw++;
            else
                uiRev++;
            if( uiForw * 2 >= size( ) )
                return true;
            if( uiRev * 2 >= size( ) )
                return false;
        } // for
        return uiForw >= uiRev;
    } // method

    /// @deprecated
    inline void mirror( nucSeqIndex uiReferenceLength, nucSeqIndex uiQueryLength )
    {
        for( Seed& rS : vContent )
        {
            rS.uiPosOnReference = uiReferenceLength * 2 - rS.end_ref( ); // check if this is still correct...
            rS.iStart = uiQueryLength - rS.end( ); // check if this is still correct...
        } // for
    } // method

    inline void flipOnQuery( nucSeqIndex uiQueryLength )
    {
        nucSeqIndex uiTop = 0;
        nucSeqIndex uiBottom = uiQueryLength - vContent.front( ).start( );
        for( Seed& rS : vContent )
        {
            rS.iStart = uiQueryLength - rS.start( );
            // get top and bottom
            uiTop = std::max( uiTop, rS.end( ) );
            uiBottom = std::min( uiBottom, rS.start( ) );
        } // for
        nucSeqIndex uiCenter = ( uiTop + uiBottom ) / 2;
        for( Seed& rS : vContent )
        {
            int64_t uiMovDist = ( int64_t )( rS.start( ) + rS.end( ) ) / 2 - (int64_t)uiCenter;
            int64_t uiNewPos = rS.iStart - uiMovDist * 2;
            assert( uiNewPos >= 0 );
            rS.iStart = ( nucSeqIndex )( uiNewPos );
        } // for
    } // method

    inline std::shared_ptr<Seeds> extractStrand( bool bStrand )
    {
        auto pRet = std::make_shared<Seeds>( );
        // move all seeds on bStrand to pRet and erase them from here
        vContent.erase( std::remove_if( vContent.begin( ), vContent.end( ),
                                        [&]( const Seed& rSeed ) {
                                            if( rSeed.bOnForwStrand == bStrand )
                                            {
                                                pRet->push_back( rSeed );
                                                return true;
                                            } // if
                                            return false;
                                        } ),
                        vContent.end( ) );
        return pRet;
    } // method

    inline void sortByRefPos( )
    {
        std::sort( vContent.begin( ),
                   vContent.end( ),
                   []( const Seed& rA, const Seed& rB ) {
                       if( rA.start_ref( ) != rB.start_ref( ) )
                           return rA.start_ref( ) < rB.start_ref( );
                       return rA.size( ) > rB.size( );
                   } // lambda
        );
    } // method

    inline void sortByQPos( )
    {
        for( size_t uiI = 1; uiI < vContent.size( ); uiI++ )
            if( vContent[ uiI - 1 ].start( ) > vContent[ uiI ].start( ) ) // check if not sorted
            {
                // if not sorted then sort...
                std::sort( vContent.begin( ),
                           vContent.end( ),
                           []( const Seed& rA, const Seed& rB ) { return rA.start( ) < rB.start( ); } // lambda
                ); // std::sort call
                return;
            } // if
    } // method

    inline std::shared_ptr<Seeds> splitOnStrands( nucSeqIndex uiReferenceLength, nucSeqIndex uiQueryLength )
    {
        std::cout << "Spliting " << vContent.size( ) << " seeds." << std::endl;
        auto pRet = extractStrand( true );
        pRet->mirror( uiReferenceLength, uiQueryLength );
        std::cout << "Forward: " << vContent.size( ) << std::endl;
        std::cout << "Reverse: " << pRet->size( ) << std::endl;
        return pRet;
    } // method

    inline void pop_back( void )
    {
        vContent.pop_back( );
    } // method

    template <class... Args> inline void emplace_back( Args&&... args )
    {
        vContent.emplace_back( args... );
    } // method

    inline bool empty( void ) const
    {
        return vContent.empty( );
    } // method

    inline value_type& front( void )
    {
        return vContent.front( );
    } // method

    inline value_type& back( void )
    {
        return vContent.back( );
    } // method

    inline const value_type& front( void ) const
    {
        return vContent.front( );
    } // method

    inline const value_type& back( void ) const
    {
        return vContent.back( );
    } // method

    inline TP_VEC::iterator begin( void ) noexcept
    {
        return vContent.begin( );
    } // method

    inline TP_VEC::iterator end( void ) noexcept
    {
        return vContent.end( );
    } // method

    inline TP_VEC::reverse_iterator rbegin( void ) noexcept
    {
        return vContent.rbegin( );
    } // method

    inline TP_VEC::reverse_iterator rend( void ) noexcept
    {
        return vContent.rend( );
    } // method

    inline TP_VEC::const_iterator begin( void ) const noexcept
    {
        return vContent.begin( );
    } // method

    inline TP_VEC::const_iterator end( void ) const noexcept
    {
        return vContent.end( );
    } // method

    inline void erase( TP_VEC::iterator pos )
    {
        vContent.erase( pos );
    } // method

    inline void erase( TP_VEC::iterator first, TP_VEC::iterator last )
    {
        vContent.erase( first, last );
    } // method

    inline TP_VEC::iterator insert( TP_VEC::const_iterator pos, const value_type& value )
    {
        return vContent.insert( pos, value );
    } // method

    template <class InputIt> inline TP_VEC::iterator insert( TP_VEC::const_iterator pos, InputIt first, InputIt last )
    {
        return vContent.insert( pos, first, last );
    } // method

    void reserve( size_type uiN )
    {
        vContent.reserve( uiN );
    } // method

    void clear( ) noexcept
    {
        vContent.clear( );
    } // method

    /// @brief returns unique seeds in this; shared seeds; unique seeds in pOther
    std::tuple<std::shared_ptr<Seeds>, std::shared_ptr<Seeds>, std::shared_ptr<Seeds>>
    splitSeedSets( std::shared_ptr<Seeds> pOther )
    {
        auto xRet =
            std::make_tuple( std::make_shared<Seeds>( ), std::make_shared<Seeds>( ), std::make_shared<Seeds>( ) );

        std::sort( vContent.begin( ), vContent.end( ), []( const Seed& rA, const Seed& rB ) {
            if( rA.start( ) != rB.start( ) )
                return rA.start( ) < rB.start( );
            if( rA.size( ) != rB.size( ) )
                return rA.size( ) < rB.size( );
            return rA.start_ref( ) < rB.start_ref( );
        } );

        std::sort( pOther->vContent.begin( ), pOther->vContent.end( ), []( const Seed& rA, const Seed& rB ) {
            if( rA.start( ) != rB.start( ) )
                return rA.start( ) < rB.start( );
            if( rA.size( ) != rB.size( ) )
                return rA.size( ) < rB.size( );
            return rA.start_ref( ) < rB.start_ref( );
        } );

        size_t uiI = 0;
        size_t uiJ = 0;
        while( uiI < vContent.size( ) && uiJ < pOther->size( ) )
        {
            if( vContent[ uiI ].start( ) == pOther->vContent[ uiJ ].start( ) &&
                vContent[ uiI ].size( ) == pOther->vContent[ uiJ ].size( ) &&
                vContent[ uiI ].start_ref( ) == pOther->vContent[ uiJ ].start_ref( ) )
            {
                std::get<1>( xRet )->push_back( vContent[ uiI ] );
                uiI++;
                uiJ++;
            } // if
            else if( vContent[ uiI ].start( ) < pOther->vContent[ uiJ ].start( ) ||
                     ( vContent[ uiI ].start( ) == pOther->vContent[ uiJ ].start( ) &&
                       ( vContent[ uiI ].size( ) < pOther->vContent[ uiJ ].size( ) ||
                         ( vContent[ uiI ].size( ) == pOther->vContent[ uiJ ].size( ) &&
                           vContent[ uiI ].start_ref( ) < pOther->vContent[ uiJ ].start_ref( ) ) ) ) )
            {
                // std::raise(SIGINT);
                std::get<0>( xRet )->push_back( vContent[ uiI ] );
                uiI++;
            } // if
            else
            {
                std::get<2>( xRet )->push_back( pOther->vContent[ uiJ ] );
                uiJ++;
            } // if
        } // while

        while( uiI < vContent.size( ) )
            std::get<0>( xRet )->push_back( vContent[ uiI++ ] );
        while( uiJ < pOther->size( ) )
            std::get<2>( xRet )->push_back( pOther->vContent[ uiJ++ ] );

        return xRet;
    } // method

    double getAverageSeedSize( ) const
    {
        nucSeqIndex uiSeedSizeSum = 0;
        for( auto& rSeed : vContent )
            uiSeedSizeSum += rSeed.size( );
        return uiSeedSizeSum / (double)vContent.size( );
    } // method

    /// @brief returns unique seeds in this; shared seeds; unique seeds in pOther
    std::tuple<size_t, size_t, size_t> compareSeedSets( std::shared_ptr<Seeds> pOther )
    {
        auto xTemp = splitSeedSets( pOther );

        return std::make_tuple( std::get<0>( xTemp )->size( ), std::get<1>( xTemp )->size( ),
                                std::get<2>( xTemp )->size( ) );
    } // method

    void DLL_PORT( MA )
        confirmSeedPositions( std::shared_ptr<NucSeq> pQuery, std::shared_ptr<Pack> pRef, bool bIsMaxExtended );
}; // class


class SeedsSet : public libMS::Container
{
  public:
    std::vector<std::shared_ptr<Seeds>> xContent;
}; // class

} // namespace libMA

#ifdef WITH_PYTHON
/**
 * @brief exports the Seed and Seedlist classes to python.
 * @ingroup export
 */
#ifdef WITH_BOOST
void exportSeed( );
#else
void exportSeed( libMS::SubmoduleOrganizer& xOrganizer );
#endif
#endif

#endif