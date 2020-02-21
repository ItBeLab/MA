/**
 * @file fileReader.h
 * @brief Reads queries from a file.
 * @author Markus Schmidt
 */
#ifndef FILE_READER_H
#define FILE_READER_H

#include "container/nucSeq.h"
#include "module/cyclic_queue_modules.h"
#include "module/module.h"
#include "support.h"


#ifdef WITH_ZLIB
#include "zlib.h"
#endif


/// @cond DOXYGEN_SHOW_SYSTEM_INCLUDES
#include <fstream>
#include <sstream>
/// @endcond

namespace libMA
{

class FileStream : public Container
{
  public:
    std::mutex xMutex;
    DEBUG( size_t uiNumLinesWithNs = 0; ) // DEBUG
    DEBUG( size_t uiNumLinesRead = 0; ) // DEBUG

    FileStream( )
    {}

    FileStream( const FileStream& ) = delete;

    virtual bool eof( ) const
    {
        throw std::runtime_error( "This function should have been overridden" );
    }

    virtual bool is_open( ) const
    {
        throw std::runtime_error( "This function should have been overridden" );
    }
    virtual void close( )
    {
        throw std::runtime_error( "This function should have been overridden" );
    }
    virtual size_t tellg( )
    {
        throw std::runtime_error( "This function should have been overridden" );
    }
    virtual size_t fileSize( )
    {
        throw std::runtime_error( "This function should have been overridden" );
    }
    virtual char peek( )
    {
        throw std::runtime_error( "This function should have been overridden" );
    }
    virtual std::string fileName( )
    {
        throw std::runtime_error( "This function should have been overridden" );
    }
    virtual void safeGetLine( std::string& t )
    {
        throw std::runtime_error( "This function should have been overridden" );
    }
    virtual std::string status( )
    {
        return std::string( fileName( ) )
            .append( ": " )
            .append( std::to_string( 100.0 * tellg( ) / (double)fileSize( ) ) )
            .append( " %" );
    } // method
}; // class

class StdFileStream : public FileStream
{
  private:
    std::ifstream xStream;
    size_t uiFileSize = 0;
    const std::string sFileName;

  public:
    StdFileStream( fs::path sFilename ) : xStream( sFilename ), sFileName( sFilename.string( ) )
    {
        std::ifstream xFileEnd( sFilename, std::ifstream::ate | std::ifstream::binary );
        uiFileSize = xFileEnd.tellg( );
    } // constructor

    bool eof( ) const
    {
        return !xStream.good( ) || xStream.eof( );
    } // method

    ~StdFileStream( )
    {
        DEBUG( if( !eof( ) ) std::cout << "StdFileStream read " << uiNumLinesRead << " lines in total." << std::endl;
               if( !eof( ) ) std::cerr << "WARNING: Did abort before end of File." << std::endl; ) // DEBUG
    } // deconstrucotr

    bool is_open( ) const
    {
        return xStream.is_open( );
    } // method

    void close( )
    {
        xStream.close( );
    } // method

    char peek( )
    {
        return xStream.peek( );
    } // method

    virtual std::string fileName( )
    {
        return sFileName;
    } // method

    size_t tellg( )
    {
        return xStream.tellg( );
    } // method

    virtual size_t fileSize( )
    {
        return uiFileSize;
    } // method

    /**
     * code taken from
     * https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
     * suports windows linux and mac line endings
     */
    void safeGetLine( std::string& t )
    {
        t.clear( );
        DEBUG( uiNumLinesRead++; ) // DEBUG

        // The characters in the stream are read one-by-one using a std::streambuf.
        // That is faster than reading them one-by-one using the std::istream.
        // Code that uses streambuf this way must be guarded by a sentry object.
        // The sentry object performs various tasks,
        // such as thread synchronization and updating the stream state.

        std::istream::sentry se( xStream, true );
        std::streambuf* sb = xStream.rdbuf( );

        while( true )
        {
            int c = sb->sbumpc( );
            switch( c )
            {
                case '\n':
                    return;
                case '\r':
                    if( sb->sgetc( ) == '\n' )
                        sb->sbumpc( );
                    return;
                case std::streambuf::traits_type::eof( ):
                    // Also handle the case when the last line has no line ending
                    if( t.empty( ) )
                        xStream.setstate( std::ios::eofbit );
                    return;
                default:
                    t += (char)c;
            } // switch
        } // while
    } // method
}; // class

class StringStream : public FileStream
{
  private:
    std::stringstream xStream;
    size_t uiFileSize;

  public:
    StringStream( const std::string& sString ) : xStream( sString ), uiFileSize( sString.size( ) )
    {} // constructor

    bool eof( ) const
    {
        return !xStream.good( ) || xStream.eof( );
    } // method

    ~StringStream( )
    {
        DEBUG( if( !eof( ) ) std::cout << "StringStream read " << uiNumLinesRead << " lines in total." << std::endl;
               if( !eof( ) ) std::cerr << "WARNING: Did abort before end of File." << std::endl; ) // DEBUG
    } // deconstrucotr

    bool is_open( ) const
    {
        return true;
    } // method

    void close( )
    {
        // we do not need to do anything
    } // method

    char peek( )
    {
        return xStream.peek( );
    } // method

    virtual std::string fileName( )
    {
        return "StringStream";
    } // method

    size_t tellg( )
    {
        return xStream.tellg( );
    } // method

    virtual size_t fileSize( )
    {
        return uiFileSize;
    } // method

    /**
     * code taken from
     * https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
     * suports windows linux and mac line endings
     */
    void safeGetLine( std::string& t ) // @todo duplicate code
    {
        t.clear( );
        DEBUG( uiNumLinesRead++; ) // DEBUG

        // The characters in the stream are read one-by-one using a std::streambuf.
        // That is faster than reading them one-by-one using the std::istream.
        // Code that uses streambuf this way must be guarded by a sentry object.
        // The sentry object performs various tasks,
        // such as thread synchronization and updating the stream state.

        std::istream::sentry se( xStream, true );
        std::streambuf* sb = xStream.rdbuf( );

        while( true )
        {
            int c = sb->sbumpc( );
            switch( c )
            {
                case '\n':
                    return;
                case '\r':
                    if( sb->sgetc( ) == '\n' )
                        sb->sbumpc( );
                    return;
                case std::streambuf::traits_type::eof( ):
                    // Also handle the case when the last line has no line ending
                    if( t.empty( ) )
                        xStream.setstate( std::ios::eofbit );
                    return;
                default:
                    t += (char)c;
            } // switch
        } // while
    } // method
}; // class

#ifdef WITH_ZLIB
class GzFileStream : public FileStream
{
  private:
    // gzFile is a pointer type
    gzFile pFile;
    int lastReadReturn = 1; // 1 == last read was ok; 0 == eof; -1 == error
    unsigned char cBuff;
    size_t uiFileSize = 0;
    std::string sFileName;

  public:
    GzFileStream( fs::path xFilename ) : sFileName( xFilename.stem( ).string( ) )
    {
        {
            std::ifstream xFileEnd( xFilename, std::ifstream::ate | std::ifstream::binary );
            uiFileSize = xFileEnd.tellg( );
        } // scope for xFileEnd
        // open the file in reading and binary mode
        pFile = gzopen( xFilename.string( ).c_str( ), "rb" );
        if( pFile != nullptr )
            // fill internal buffer
            lastReadReturn = gzread( pFile, &cBuff, 1 );
        else
            lastReadReturn = -1; // if the file could net be opened set error immediately
    } // constructor

    bool eof( ) const
    {
        return lastReadReturn != 1;
    } // method

    ~GzFileStream( )
    {
        DEBUG( if( !eof( ) ) std::cout << "StdFileStream read " << uiNumLinesRead << " lines in total." << std::endl;
               if( !eof( ) ) std::cerr << "WARNING: Did abort before end of File." << std::endl; ) // DEBUG
    } // deconstructor

    bool is_open( ) const
    {
        return pFile != nullptr;
    } // method

    void close( )
    {
        gzclose( pFile );
    } // method

    size_t tellg( )
    {
        return gzoffset( pFile );
    } // method

    virtual size_t fileSize( )
    {
        return uiFileSize;
    } // method

    char peek( )
    {
        return cBuff;
    } // method

    virtual std::string fileName( )
    {
        return sFileName;
    } // method

    void safeGetLine( std::string& t )
    {
        t.clear( );
        DEBUG( uiNumLinesRead++; ) // DEBUG

        // read until EoF or EoL is reached
        while( true )
        {
            // gzread returns the number of bytes read; if we do not get 1 here something went wrong
            if( lastReadReturn != 1 )
                break;
            // if we reached the end of the line return
            if( cBuff == '\r' )
                // read the \n that should be the next character
                lastReadReturn = gzread( pFile, &cBuff, 1 );
            if( lastReadReturn != 1 || cBuff == '\n' )
                break;
            t += cBuff;

            // read one char from the stream
            lastReadReturn = gzread( pFile, &cBuff, 1 );
        } // while

        // read the EoL char from the stream (if the stream is ok)
        if( lastReadReturn == 1 )
            lastReadReturn = gzread( pFile, &cBuff, 1 );
    } // method
}; // class
#endif

class FileStreamFromPath : public FileStream
{
  public:
    std::unique_ptr<FileStream> pStream;

    FileStreamFromPath( fs::path sFileName )
    {
#ifdef WITH_ZLIB
        if( sFileName.extension( ).string( ) == ".gz" )
            pStream = std::make_unique<GzFileStream>( sFileName );
        else
#endif
            pStream = std::make_unique<StdFileStream>( sFileName );
        if( !pStream->is_open( ) )
            throw std::runtime_error( "Unable to open file" + sFileName.string( ) );
    } // contstructor

    FileStreamFromPath( std::string sFileName ) : FileStreamFromPath( fs::path( sFileName ) )
    {} //  // contstructor

    ~FileStreamFromPath( )
    {
        pStream->close( );
    }

    virtual bool eof( ) const
    {
        return pStream->eof( );
    }

    virtual bool is_open( ) const
    {
        return pStream->is_open( );
    }
    virtual void close( )
    {
        pStream->close( );
    }
    virtual size_t tellg( )
    {
        return pStream->tellg( );
    }
    virtual size_t fileSize( )
    {
        return pStream->fileSize( );
    }
    virtual char peek( )
    {
        return pStream->peek( );
    }
    virtual std::string fileName( )
    {
        return pStream->fileName( );
    } // method
    virtual void safeGetLine( std::string& t )
    {
        pStream->safeGetLine( t );
    }
}; // class

/**
 * @brief Reads Queries from a file.
 * @details
 * Reads (multi-)fasta or fastaq format.
 */
class FileReader : public Module<NucSeq, true, FileStream>
{
  public:
    /**
     * @brief creates a new FileReader.
     */
    FileReader( const ParameterSetManager& rParameters )
    {} // constructor

    std::shared_ptr<NucSeq> EXPORTED execute( std::shared_ptr<FileStream> pStream );

}; // class

using FileStreamQueue = CyclicQueue<FileStream>;
class PairedFileStream : public FileStream, public std::pair<std::shared_ptr<FileStream>, std::shared_ptr<FileStream>>
{
  public:
    using std::pair<std::shared_ptr<FileStream>, std::shared_ptr<FileStream>>::pair;

    virtual bool eof( ) const
    {
        return first->eof( ) || second->eof( );
    }

    virtual bool is_open( ) const
    {
        return first->is_open( ) && second->is_open( );
    }
    virtual void close( )
    {
        first->close( );
        second->close( );
    }
    virtual size_t tellg( )
    {
        return first->tellg( ) + second->tellg( );
    }
    virtual size_t fileSize( )
    {
        return first->fileSize( ) + second->fileSize( );
    }
    virtual char peek( )
    {
        throw std::runtime_error( "This function should have been overridden" );
    }
    virtual std::string fileName( )
    {
        return first->fileName( ) + std::string( "," ) + second->fileName( );
    }
    virtual void safeGetLine( std::string& t )
    {
        throw std::runtime_error( "This function should have been overridden" );
    }
}; // class

using PairedFileStreamQueue = CyclicQueue<PairedFileStream>;

inline std::shared_ptr<PairedFileStreamQueue>
combineFileStreams( std::shared_ptr<FileStreamQueue> pA, std::shared_ptr<FileStreamQueue> pB )
{
    auto pRet = std::make_shared<PairedFileStreamQueue>( );
    while( pA->numUnifinished( ) > 0 && pB->numUnifinished( ) > 0 )
    {
        pRet->add( std::make_shared<PairedFileStream>( pA->pop( ), pB->pop( ) ) );
        pA->informThatContainerIsFinished( );
        pB->informThatContainerIsFinished( );
    } // while
    if( pA->numUnifinished( ) != pB->numUnifinished( ) )
        throw std::runtime_error( "@todo" );
    return pRet;
} // function

using PairedReadsContainer = ContainerVector<std::shared_ptr<NucSeq>>;

/**
 * @brief Reads Queries from a file.
 * @details
 * Reads (multi-)fasta or fastaq format.
 */
class PairedFileReader : public Module<PairedReadsContainer, true, PairedFileStream>
{
  public:
    FileReader xFileReader;
    const bool bRevCompMate;

    /**
     * @brief creates a new FileReader that reads from a list of files.
     */
    PairedFileReader( const ParameterSetManager& rParameters )
        : xFileReader( rParameters ), bRevCompMate( rParameters.getSelected( )->xRevCompPairedReadMates->get( ) )
    {} // constructor

    std::shared_ptr<PairedReadsContainer> execute( std::shared_ptr<PairedFileStream> pFileStreamIn )
    {
        auto pRet = std::make_shared<PairedReadsContainer>( );
        pRet->push_back( xFileReader.execute( pFileStreamIn->first ) );
        pRet->push_back( xFileReader.execute( pFileStreamIn->second ) );
#if 0
#if DEBUG_LEVEL > 0
        if( rReader1.getCurrFileIndex( ) != rReader2.getCurrFileIndex( ) )
            throw std::runtime_error( "Cannot perfrom paired alignment on files with different amounts of "
                                      "reads. FileReader Status: " +
                                      this->status( ) );
#endif
        // forward the finished flags...
        if( rReader1.isFinished( ) || rReader2.isFinished( ) )
        {
            /*
             * Print a warning if the fasta files have a different number of queries.
             */
            if( !rReader1.isFinished( ) || !rReader2.isFinished( ) )
                throw std::runtime_error( "You cannot perform paired alignment with a different amount of primary "
                                          "queries and mate queries." );
            // this->setFinished( );
        } // if
#endif
        if( ( *pRet )[ 0 ] == nullptr )
            return nullptr;
        if( ( *pRet )[ 1 ] == nullptr )
            return nullptr;

        if( bRevCompMate )
        {
            pRet->back( )->vReverse( );
            pRet->back( )->vSwitchAllBasePairsToComplement( );
        } // if
        return pRet;
    } // method
}; // class

/** @brief prints the progrss of a FileStreamQueue or PairedFileStreamQueue
 * @details
 * Prints out the files that are currently open and how far they are read
 */
template <typename FileStreamQueue> class ProgressPrinter : public Module<Container, false, Container, FileStreamQueue>
{
    std::mutex xMutex;
    using duration = std::chrono::duration<double>;
    using time_point = std::chrono::time_point<std::chrono::steady_clock, duration>;
    time_point xLastTime;
    double dPrintTime = 3;

  public:
    ProgressPrinter( const ParameterSetManager& rParameters ) : xLastTime( std::chrono::steady_clock::now( ) )
    {} // constructor

    std::shared_ptr<Container> execute( std::shared_ptr<Container> pContainer, std::shared_ptr<FileStreamQueue> pQueue )
    {
        std::unique_lock<std::mutex> xLock( xMutex );
        auto xNow = std::chrono::steady_clock::now( );
        duration xElapsedTime = xNow - xLastTime;
        if( xElapsedTime.count( ) > dPrintTime )
        {
            xLastTime = xNow;
            std::cout << "Open files:" << std::endl;
            size_t uiRemaining = 0;
            size_t uiDone = 0;
            pQueue->iter( [&]( auto pStream ) {
                if( pStream->tellg( ) * 100 <= pStream->fileSize( ) )
                    uiRemaining++;
                else if( pStream->tellg( ) == pStream->fileSize( ) )
                    uiDone++;
                else
                    std::cout << pStream->status( ) << std::endl;
            } // lambda
            );
            std::cout << "Remaining files: " << uiRemaining << " Finished files: " << uiDone << std::endl;
        } // if
        return pContainer;
    } // method
}; // class

} // namespace libMA

#ifdef WITH_PYTHON
#ifdef WITH_BOOST
void exportFileReader( );
#else
void exportFileReader( py::module& rxPyModuleId );
#endif
#endif

#endif