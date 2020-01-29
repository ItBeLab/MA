/**
 * @file geom.h
 * @brief Implements a generic Interval class.
 * @author Markus Schmidt
 */
#pragma once

#include "exception.h"
#include "support.h"

/// @cond DOXYGEN_SHOW_SYSTEM_INCLUDES
#include <functional>
#include <iostream>
#include <algorithm>
/// @endcond DOXYGEN_SHOW_SYSTEM_INCLUDES

namespace geomUtil
{
/**
 * @brief A generic multipurpose Interval.
 */
template <typename T> class Interval
{
  public:
    /// @brief Start position of interval.
    T iStart = 0;
    /// @brief Size of interval.
    T iSize = 0;
    /**
     * @brief Creates a new Interval.
     */
    Interval( T iStart, T iSize ) : iStart( iStart ), iSize( iSize )
    {} // constructor

    /**
     * @brief Copys from another Interval.
     */
    Interval( const Interval& c ) : iStart( c.iStart ), iSize( c.iSize )
    {} // copy constructor

    /**
     * @brief Default empty constructor.
     */
    Interval( )
    {} // default constructor

    inline static Interval start_end( T start, T end )
    {
        Interval xRet( start, 0 );
        xRet.end( end );
        return xRet;
    } // function

    /**
     * @returns the end of the interval.
     * @brief Interval end.
     * @note end = start + size
     */
    inline const T end( ) const
    {
        return iStart + iSize;
    } // function

    ///@brief Wrapper for boost-python.
    inline const T end_boost1( ) const
    {
        return iStart + iSize;
    } // function

    /**
     * @brief Allows chaning the end of the interval.
     * @note end = start + size
     */
    inline void end( const T iVal )
    {
        iSize = iVal - iStart;
    } // function

    ///@brief Wrapper for boost-python.
    inline void end_boost2( const T iVal )
    {
        end( iVal );
    } // function

    /**
     * @returns the start of the interval.
     * @brief Interval start.
     */
    inline const T start( ) const
    {
        return iStart;
    } // function

    ///@brief Wrapper for boost-python.
    inline const T start_boost1( ) const
    {
        return iStart;
    } // function

    /**
     * @brief allows chaning the beginning of the interval.
     */
    inline void start( const T iVal )
    {
        T iEnd = end( );
        iStart = iVal;
        end( iEnd );
    } // function

    ///@brief Wrapper for boost-python.
    inline void start_boost2( const T iVal )
    {
        start( iVal );
    } // function

    /**
     * @brief allows chaning the size of the interval.
     */
    inline void size( const T iVal )
    {
        iSize = iVal;
    } // function

    ///@brief wrapper for boost-python
    inline void size_boost2( const T iVal )
    {
        iSize = iVal;
    } // function

    /**
     * @returns the center of the interval.
     * @brief The center of the interval.
     */
    inline const T center( ) const
    {
        return start( ) + size( ) / 2;
    } // function

    /**
     * @returns the size of the interval.
     * @brief The size of the interval.
     */
    inline const T size( ) const
    {
        return iSize;
    } // function

    ///@brief wrapper for boost-python
    inline const T size_boost1( ) const
    {
        return iSize;
    } // function

    /**
     * @brief allows chaning the start and size of the interval.
     */
    inline void set( T iStart, T iSize )
    {
        start( iStart );
        size( iSize );
    } // function

    /*
     * @returns the interval start and end for i = 0 and 1 respectively.
     * @brief Interval start for 0 and end for 1.
     * @details
     * Throws a nullpointer exception for any other i.
     */
    inline const T operator[]( const std::size_t i ) const
    {
        if( i == 0 )
            return start( );
        if( i == 1 )
            return end( );
        throw AnnotatedException( "can only access index 0 and 1 of interval" );
    } // operator

    /*
     * @brief copys from another Interval.
     */
    inline Interval& operator=( const Interval& rxOther )
    {
        iStart = rxOther.iStart;
        iSize = rxOther.iSize;
        return *this;
    } // operator

    /*
     * @brief compares two Intervals.
     * @returns true if start and size are equal, false otherwise.
     */
    inline bool operator==( const Interval& rxOther )
    {
        return iStart == rxOther.iStart && iSize == rxOther.iSize;
    } // operator
}; // class

template <size_t SIZE> class WKBPolygon
{
    std::array<uint8_t, SIZE> aData;

  public:
    WKBPolygon( )
    {} // constructor

    inline void set( size_t uiPos, uint8_t uiData )
    {
        aData[ uiPos ] = uiData;
    } // method

    inline void set_double( size_t uiPos, double fData )
    {
        assert( sizeof( double ) == 8 ); // assert that float has the correct size

        uint8_t* puiData = (uint8_t*)&fData;
        for( size_t uiI = 0; uiI < 8; uiI++ )
            set( uiPos + uiI, puiData[ uiI ] );
    } // method

    inline size_t get( size_t uiI ) const
    {
        return aData[ uiI ];
    } // method

    inline double getDouble( size_t uiI ) const
    {
        assert( sizeof( double ) == 8 ); // assert that float has the correct size
        assert( SIZE >= uiI + 8 );

        double* pfData = (double*)&aData[ uiI ];
        return pfData[ 0 ];
    } // method

    inline void* getData( )
    {
        return (void*)&aData[ 0 ];
    } // method

    inline void setData( void* auiData )
    {
        std::copy_n( reinterpret_cast<uint8_t*>( auiData ), SIZE, aData.begin( ) );
    } // method
}; // class

template <typename T> class Rectangle
{
    const static size_t uiSizeWKBHeader = 1 + // big/little endian a 1 byte
                                          4; // geom type a 4 bytes
    const static size_t uiSizeWKB = uiSizeWKBHeader + 5 * 2 * sizeof( double );

    static size_t posOfPointX( size_t uiIdx )
    {
        assert( sizeof( double ) == 8 ); // assert that float has the correct size
        return uiSizeWKBHeader + uiIdx * 2 * sizeof( double ); // each point is expressed by two doubles (x and y);
    } // method

    static size_t posOfPointY( size_t uiIdx )
    {
        return posOfPointX( uiIdx ) + sizeof( double );
    } // method

  public:
    Interval<T> xXAxis, xYAxis;
    /**
     * @brief Creates a new Rectangle.
     */
    Rectangle( T iStartX, T iStartY, T iSizeX, T iSizeY ) : xXAxis( iStartX, iSizeX ), xYAxis( iStartY, iSizeY )
    {} // constructor

    inline WKBPolygon<uiSizeWKB> getWKB( )
    {
        WKBPolygon<uiSizeWKB> xData;
        xData.set( 0, is_big_endian( ) ? 0x00 : 0x01 ); // big/little endian a 1 byte

        xData.set( 1, 0 ); // geom type a 4 bytes
        xData.set( 2, 0 ); // geom type a 4 bytes
        xData.set( 3, 0 ); // geom type a 4 bytes
        xData.set( 4, 0x03 ); // geom type a 4 bytes

        // counterclockwise:
        // bottom left
        xData.set_double( posOfPointX( 0 ), xXAxis.start( ) );
        xData.set_double( posOfPointY( 0 ), xYAxis.start( ) );

        // bottom right
        xData.set_double( posOfPointX( 1 ), xXAxis.end( ) );
        xData.set_double( posOfPointY( 1 ), xYAxis.start( ) );

        // top right
        xData.set_double( posOfPointX( 2 ), xXAxis.end( ) );
        xData.set_double( posOfPointY( 2 ), xYAxis.end( ) );

        // top left
        xData.set_double( posOfPointX( 3 ), xXAxis.start( ) );
        xData.set_double( posOfPointY( 3 ), xYAxis.end( ) );

        // bottom left (again)
        xData.set_double( posOfPointX( 4 ), xXAxis.start( ) );
        xData.set_double( posOfPointY( 4 ), xYAxis.start( ) );

        return xData;
    } // method

    inline void fromWKB( WKBPolygon<uiSizeWKB>& xData )
    {
        if( xData.get( 0 ) != ( is_big_endian( ) ? 0x00 : 0x01 ) ) // check endian
            throw std::runtime_error( "WKB endian of DB does not match endian of system" );
        if( xData.get( 4 ) != 0x03 ) // check
            throw std::runtime_error( "WKB is no polygon" );

        if( xData.getDouble( posOfPointX( 0 ) ) != xData.getDouble( posOfPointX( 4 ) ) )
            throw std::runtime_error(
                "WKB polygon is no (closed) rectangle (i.e. first points x does not match last points x)" );

        if( xData.getDouble( posOfPointY( 0 ) ) != xData.getDouble( posOfPointY( 5 ) ) )
            throw std::runtime_error(
                "WKB polygon is no (closed) rectangle (i.e. first points y does not match last points y)" );

        T uiXStart = (T)xData.getDouble( posOfPointX( 0 ) );
        if( (T)xData.getDouble( posOfPointX( 3 ) ) != uiXStart )
            throw std::runtime_error( "WKB polygon is no rectangle (i.e. angles are not rectangular uiXStart)" );

        T uiXEnd = (T)xData.getDouble( posOfPointX( 1 ) );
        if( (T)xData.getDouble( posOfPointX( 2 ) ) != uiXEnd )
            throw std::runtime_error( "WKB polygon is no rectangle (i.e. angles are not rectangular uiXEnd)" );
        if( uiXStart >= uiXEnd )
            throw std::runtime_error( "WKB rectangle is in wrong order (i.e. uiXStart >= uiXEnd)" );

        T uiYStart = (T)xData.getDouble( posOfPointY( 0 ) );
        if( (T)xData.getDouble( posOfPointY( 1 ) ) != uiYStart )
            throw std::runtime_error( "WKB polygon is no rectangle (i.e. angles are not rectangular uiYStart)" );

        T uiYEnd = (T)xData.getDouble( posOfPointY( 2 ) );
        if( (T)xData.getDouble( posOfPointY( 3 ) ) != uiYEnd )
            throw std::runtime_error( "WKB polygon is no rectangle (i.e. angles are not rectangular uiYEnd)" );
        if( uiYStart >= uiYEnd )
            throw std::runtime_error( "WKB rectangle is in wrong order (i.e. uiXStart >= uiXEnd)" );

        xXAxis.iStart = uiXStart;
        xYAxis.iStart = uiYStart;
        xXAxis.iSize = uiXEnd - uiXStart;
        xYAxis.iSize = uiYEnd - uiYStart;
    } // method
}; // class

} // namespace geomUtil