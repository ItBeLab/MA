/**
 * @file geom.h
 * @brief Support of geometric entities
 * @author Markus Schmidt
 */
#pragma once

#include "util/support.h"
#include <stdexcept>

/// @cond DOXYGEN_SHOW_SYSTEM_INCLUDES
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
/// @endcond DOXYGEN_SHOW_SYSTEM_INCLUDES

namespace geom
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
        throw std::runtime_error( "can only access index 0 and 1 of interval" );
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

    inline size_t distance( const Interval& rxOther ) const
    {
        if( iStart + iSize >= rxOther.iStart && rxOther.iStart + rxOther.iSize >= iStart ) // overlapping case
            return 0;

        if( iStart + iSize < rxOther.iStart )
            return rxOther.iStart - ( iStart + iSize );

        return iStart - ( rxOther.iStart + rxOther.iSize );
    } // method
}; // class

inline std::ostream& operator<<( std::ostream& xOS, const Interval<uint64_t>& xInterval )
{
    xOS << "(" << xInterval.start( ) << ", " << xInterval.end( ) << "] ";
    return xOS;
} // operator

/** @brief Very basic representation of a rectangle.
 * FIXME: Rename to Rectangle.
 */
template <typename T> class Rectangle
{
  public:
    Interval<T> xXAxis, xYAxis;
    /**
     * @brief Creates a new Rectangle.
     */
    Rectangle( T iStartX, T iStartY, T iSizeX, T iSizeY ) : xXAxis( iStartX, iSizeX ), xYAxis( iStartY, iSizeY )
    {} // constructor

    Rectangle( ) : Rectangle( 0, 0, 0, 0 )
    {} // default constructor

    /*
     * @brief compares two Rectangles.
     * @returns true if origin and size are equal, false otherwise.
     */
    inline bool operator==( const Rectangle<T>& rxOther )
    {
        return xXAxis == rxOther.xXAxis && xYAxis == rxOther.xYAxis;
    } // operator

    inline void resize( T iBy )
    {
        if( xXAxis.iStart >= iBy )
        {
            xXAxis.iStart -= iBy;
            xXAxis.iSize += iBy * 2;
        } // if
        else
        {
            xXAxis.iSize += xXAxis.iStart;
            xXAxis.iStart = 0;
        } // else

        if( xYAxis.iStart >= iBy )
        {
            xYAxis.iStart -= iBy;
            xYAxis.iSize += iBy * 2;
        } // if
        else
        {
            xYAxis.iSize += xYAxis.iStart;
            xYAxis.iStart = 0;
        } // else
    } // method

    inline size_t manhattonDistance( const Rectangle<T>& rxOther ) const
    {
        return xXAxis.distance( rxOther.xXAxis ) + xYAxis.distance( rxOther.xYAxis );
    } // method

}; // class

inline std::ostream& operator<<( std::ostream& xOS, const Rectangle<uint64_t>& xRect )
{
    xOS << std::dec << "Rectangle: x: " << xRect.xXAxis << " y: " << xRect.xYAxis << std::endl;
    return xOS;
} // operator

} // namespace geom
