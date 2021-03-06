/**
 * @file pack.h
 * @brief Implements the Pack class.
 * @author Arne Kutzner
 */

/* Revision:
 * We only store the forward strand with the pack.
 * The reverse strand is imaginary and its content developed on demand by using the forward strand.
 */

#pragma once

#define FASTA_READER

#include <chrono>
#include <type_traits>
#include <cstdint>

#include "ma/container/nucSeq.h"

#ifdef FASTA_READER
class FastaDescriptor;
#endif

namespace libMA
{

/* study http://stackoverflow.com/questions/3180268/why-are-c-stl-iostreams-not-exception-friendly
 *       study
 * http://gehrcke.de/2011/06/reading-files-in-c-using-ifstream-dealing-correctly-with-badbit-failbit-eofbit-and-perror/
 */
/**
 * @brief A packed NucSeq.
 * @details
 * Compresses the sequence.
 * @ingroup container
 */
class Pack : public libMS::Container
{
    /* Delete implicit copy constructor.
     * For performance reasons we do not want to risk unwanted copies of packs, although it is safe
     * to make such copies using the implicit constructor definitions. According the C++ standard
     * this should effect all other implicit constructors as well.
     */
    Pack( const Pack& ) = delete;

  private:
    /* In the original BWA code the reverse strand is stored as part of the pack as well.
     * However, the reverse strand is only required in the context of BWT generation.
     * This boolean variable indicates, whether the revers strand is currently appended to the
     * loaded pack.
     */
    bool bPackComprisesReverseStrand;

    /* A packed collection may consist of more than one packed sequence.
     * We call such sequences embedded sequences and keep the information for each embedded sequence
     * in a sequence descriptor.
     */
  public:
    struct SequenceInPack
    {
        std::string sName; // Name of the sequence (FASTA name or NCBI accession)
        std::string sComment; // Comments (annotations) for the sequence

        /* Pack files can contain more than one FASTA-File. This is this the offset of the current
         * sequence. For the first sequence the offset is naturally 0.
         */
        uint64_t uiStartOffsetUnpacked;

        uint64_t uiLengthUnpacked; // Length of the sequence (int32_t -> size_t)
        uint32_t gi; // NCBI gi of sequence (currently unused)

        int32_t uiNumberOfHoles; // Number of holes in the sequence.

        /* Initializing constructor.
         */
        SequenceInPack( const std::string& sName, const std::string& sComment, const uint64_t uiOffsetInPack,
                        const uint64_t length )
            : sName( sName ),
              sComment( sComment.empty( ) ? "none" : sComment ),
              uiStartOffsetUnpacked( uiOffsetInPack ),
              uiLengthUnpacked( length ),
              gi( 0 ),
              uiNumberOfHoles( 0 )
        {} // initializing constructor

        /* Default constructor
         */
        SequenceInPack( )
        {} // non initializing default constructor
    }; // inner struct
    /* Vector with descriptions for all sequences in the pack.
     */
    std::vector<SequenceInPack> xVectorOfSequenceDescriptors;

  private:
    /* Debug method that checks the vector of sequence descriptors with respect to consistency.
     * Special case: If the final sequence has size 0 this check fails as well.
     */
    bool debugCheckSequenceDescriptorVector( )
    {
        DEBUG_2( std::cout << "Check description vector for consistency." << std::endl; )
        decltype( SequenceInPack::uiStartOffsetUnpacked ) uiRunningStartOffsetUnpacked = 0;

        for( size_t uiIndex = 0; uiIndex < xVectorOfSequenceDescriptors.size( ); uiIndex++ )
        {
            assert( uiRunningStartOffsetUnpacked < this->uiUnpackedSizeForwardStrand );
            assert( xVectorOfSequenceDescriptors[ uiIndex ].uiStartOffsetUnpacked == uiRunningStartOffsetUnpacked );

            uiRunningStartOffsetUnpacked += xVectorOfSequenceDescriptors[ uiIndex ].uiLengthUnpacked;
        } // for

        assert( uiRunningStartOffsetUnpacked == this->uiUnpackedSizeForwardStrand );

        /* If we come here without any problem, anything is fine.
         */
        return true;
    } // debug method

    /* A 'hole' is an area of an packed sequence, where we have 'N' symbols or similar (not 'A',
     * 'C', 'G', 'T'). Holes require some special memorizing, because we can not directly represent
     * them in the 2-bit format.
     */
    struct HoleDescriptor
    {
        uint64_t offset; // Offset of the hole (with respect to the complete collection)
        int32_t length; // The size of the hole
        char xHoleCharacter; // The character

        /* Initializing constructor.
         */
        HoleDescriptor( uint64_t offset, char xHoleCharacter )
            : offset( offset ), length( 1 ), xHoleCharacter( xHoleCharacter )
        {} // initializing constructor

        /* Default constructor.
         */
        HoleDescriptor( )
        {} // non initializing default constructor
    }; // inner struct

    /* Vector with information about holes of all sequences in the pack.
     */
    std::vector<HoleDescriptor> xVectorOfHoleDescriptors;

    /* All sequences in the collection stored in one long vector of bytes.
     * Compressed in 2 bit format. 0 -> A, 1 -> C, 2 -> G, 3 -> T
     * Each byte contains 4 Nucleotides.
     */
    std::vector<uint8_t> xPackedNucSeqs;

    /* The random seed value used for starting the generation of random numbers.
     */
    uint32_t seed;

    /* The 6 leading bits of uiValue should be zero.
     * Keep this function inline, so that the compiler can optimize.
     * Works only for the virtual forward strand.
     * @note this only works if the respective bit is zero initialized!
     */
    inline void vSetNucleotideOnPos( const uint64_t uiPosition, const uint8_t uiValue )
    { /* We expect a correct position, when we come here.
       */
        // xPackedNucSeqs[ ( size_t )( uiPosition >> 2 ) ] &= ~( 3 << ( ( ~uiPosition & 3UL ) << 1 ) );
        xPackedNucSeqs[ ( size_t )( uiPosition >> 2 ) ] |= uiValue << ( ( ~uiPosition & 3UL ) << 1 );
    } // inline method
  public:
    /* Get the value at position uiPosition in the unpacked sequence.
     * Works only for the virtual forward strand.
     */
    inline uint8_t getNucleotideOnPos( const uint64_t uiPosition ) const
    { /* We expect a correct position, when we come here.
       */
        return xPackedNucSeqs[ ( size_t )( uiPosition >> 2 ) ] >> ( ( ~uiPosition & 3UL ) << 1 ) & 3;
    } // inline method
  private:
    /* Method can throw exception if I/O operation fails.
     * Writes the isolated content of the pack-vector to the file-system.
     */
    void vStorePack( const std::string& rsFileNamePrefix,
                     const decltype( xPackedNucSeqs )& rxPackedSequences,
                     const uint64_t uiUnpackedSize ) const
    {
        auto rsFileName( rsFileNamePrefix + ".pac" ); // final filename for writing the packed sequence
        DEBUG_2( std::cout << "Storing pack with filename " << rsFileName << std::endl; )
        std::ofstream xFileOutputStream( rsFileName, std::ios::out | std::ios::binary );

        /* Write the content of the pack to the file.
         * According to the C++ standard the data are contiguous in the vector. So, this hack works.
         */
        if( rxPackedSequences.size( ) > 0 )
        { /* For size 0 the address operation is "out of range".
           */
            xFileOutputStream.write( (const char*)&rxPackedSequences[ 0 ], rxPackedSequences.size( ) );
        } // if

        /* Original comment: the following codes make the pac file size always (i64PacSize/4+1+1)
         * If the pac-size is a multiple of 4 we write some extra zero at then end.
         * We need this extra byte as adaption in the context of the later reading with fixed size
         * calculation.
         */
        if( uiUnpackedSize % 4 == 0 )
        {
            uint8_t uiZero = 0;
            xFileOutputStream.write( (const char*)&uiZero, 1 );
        } // if

        /* We write the remainder as a single byte at the end of the file.
         * So we can later reconstruct the size of the total size of the unpacked sequence.
         */
        uint8_t uiChecksum = uiUnpackedSize % 4;
        xFileOutputStream.write( (const char*)&uiChecksum, 1 );

        /* Close the output stream and check whether everything is fine.
         */
        xFileOutputStream.close( );
        if( xFileOutputStream.fail( ) )
        {
            throw( std::runtime_error( std::string( "could not store pack " ).append( rsFileName ) ) );
        } // if
    } // method

    /* Writes the vector of sequence entries to a .ann- file
     * Writes the vector of holes to a .amb-file
     * Both files stay plain readable.
     * prefix is the prefix of the filenames.
     * Improvement: The pack should comprise forward as well as reverse strand.
     */
    void vStoreCollectionDescripton( const std::string& rsFileNamePrefix )
    {
        /* Write records for all sequence entries.
         */
        {
            std::string sFileName( rsFileNamePrefix + ".ann" );
            std::ofstream xFileOutputStream( sFileName, std::ios::out ); // 0x0D -> CRLF

            /* Write the head line, followed by writing the descriptors.
             */
            xFileOutputStream << uiUnpackedSizeForwardStrand << " " // Store the size of the forward strand only.
                              << xVectorOfSequenceDescriptors.size( ) << " " // Number of sequences is the pack.
                              << seed << "\n";

            for( const auto& rxEntry : xVectorOfSequenceDescriptors )
            {
                xFileOutputStream << rxEntry.gi << " " << rxEntry.sName << " " << rxEntry.sComment << "\n"
                                  << rxEntry.uiStartOffsetUnpacked << " " << rxEntry.uiLengthUnpacked << " "
                                  << rxEntry.uiNumberOfHoles << "\n";
            } // for
        } // end of scope -> xFileOutputStream closed

        /* Write records for all hole entries.
         */
        {
            std::string sFileName( rsFileNamePrefix + ".amb" );
            std::ofstream xFileOutputStream( sFileName, std::ios::out ); // 0x0D -> CRLF

            /* Write Head line.
             */
            xFileOutputStream << uiUnpackedSizeForwardStrand << " " // Store the size of the forward strand only.
                              << xVectorOfSequenceDescriptors.size( ) << " " // Number of sequences is the pack.
                              << xVectorOfHoleDescriptors.size( ) << "\n"; // number of holes in the pack.

            for( const auto& rxEntry : xVectorOfHoleDescriptors )
            {
                xFileOutputStream << rxEntry.offset << " " << rxEntry.length << " " << rxEntry.xHoleCharacter << "\n";
            } // for
        } // end of scope -> xFileOutputStream closed
    } // method

    /* Method can throw exception if I/O operation fails.
     * Restores a pack from the file-system.
     * When we come here, we expect, that uiUnpackedSizeForwardStrand has already been set.
     */
    void vLoadPackedSequence( std::string sFileNamePrefix,
                              uint64_t uiUnpackedSize // this size should be known in the moment of pack reconstruction
    )
    {
        /* Create the full file name and open the stream for input
         */
        sFileNamePrefix.append( ".pac" );
        std::ifstream xFileInputStream( sFileNamePrefix, std::ios::in | std::ios::binary );

        if( xFileInputStream.fail( ) )
        {
            throw std::runtime_error( "Reading pack-file failed, because file opening failed." );
        } // if

        /* We get the size of the pac file. And move the read position back to the beginning of the
         * file.
         */
        xFileInputStream.seekg( 0, std::ifstream::end );
        std::streamoff uiFileSize = xFileInputStream.tellg( );
        xFileInputStream.seekg( 0, std::ifstream::beg );

        /* In the context of the construction depending on the checksum we injected a zero-byte
         */
        bool bZeroByteInjection = uiUnpackedSize % 4 == 0;

        /* Read the pack back from the file.
         * TO DO: On 32 Bit systems check for some overflow.
         */
        xPackedNucSeqs.resize( ( size_t )( uiFileSize - ( 1 + bZeroByteInjection ) ) );
        xFileInputStream.read( (char*)&xPackedNucSeqs[ 0 ], uiFileSize - ( 1 + bZeroByteInjection ) );

        /* Check existence of injected zero-byte.
         */
        if( bZeroByteInjection )
        {
            uint8_t uiZeroByte;
            xFileInputStream.read( (char*)&uiZeroByte, 1 );
            if( uiZeroByte != 0 )
            {
                throw std::runtime_error( "Loading pack failed. Missed expected zero-byte." );
            } // if
        } // if

        /* Read and verify the checksum.
         */
        uint8_t uiChecksum;
        xFileInputStream.read( (char*)&uiChecksum, 1 );
        if( uiChecksum != uiUnpackedSize % 4 )
        {
            throw std::runtime_error( "Loading pack failed. Wrong checksum." );
        } // if

        /* Check whether the unpacked sequence size is reasonable.
         */
        if( ( uiUnpackedSize >> 2 ) + ( ( uiUnpackedSize & 3 ) == 0 ? 0 : 1 ) != xPackedNucSeqs.size( ) )
        {
            //// std::cout << uiUnpackedSizeForwardStrand << "\n";
            //// std::cout << ( uiUnpackedSizeForwardStrand >> 2) + (( uiUnpackedSizeForwardStrand &
            /// 3 ) == 0 ? 0 : 1 ) << "\n"; / std::cout << ( xPackedNucSeqs.size() - 1 ) << "\n";
            throw std::runtime_error( "Loading pack failed. Inconsistent pack size recognized." );
        } // if

        /* Close the file input stream
         */
        xFileInputStream.close( );
    } // method

    /* WARNING! Never do this for some already used (filled) sequence collection.
     * The descriptors must be stored in ascending with respect to uiStartOffsetUnpacked.
     * Throws an exception, if something goes wrong.
     * Deserialization of the vector for sequence descriptors.
     */
    void vLoadSequenceDescriptorVector( const char* pcFileNamePrefix )
    {
        size_t uiExpectedVectorSize = 0;

        /* TO DO insert a file open check over here!!
         */
        std::ifstream xFileInputStream( fullFileName( pcFileNamePrefix, "ann" ).c_str( ),
                                        std::ios::in | std::ios::binary );

        /* Read the headline
         */
        xFileInputStream >> uiUnpackedSizeForwardStrand // Size of the forward stand.
            >> uiExpectedVectorSize // number of sequences
            >> seed;

        /* Read all descriptors until you get some error.
         */
        while( true )
        {
            SequenceInPack rxEntry;

            /* We read two lines containing sequence descriptor data.
             * The comment(annotation) might contain white-spaces, so we read it using getline.
             */
            xFileInputStream >> rxEntry.gi >> rxEntry.sName; // read the serialized GI and sequence name
            std::getline( xFileInputStream, rxEntry.sComment ); // read the comment
            xFileInputStream >> rxEntry.uiStartOffsetUnpacked >> rxEntry.uiLengthUnpacked >>
                rxEntry.uiNumberOfHoles; // read start in pack, length and number of holes

            if( !xFileInputStream.fail( ) )
            {
                /* Looks good, we add our Sequence descriptor to the vector
                 */
                xVectorOfSequenceDescriptors.emplace_back( std::move( rxEntry ) );
            } // if
            else
            {
                break;
            } // else
        } // while

        /* Some very basic test, whether something went wrong.
         */
        if( !xFileInputStream.eof( ) || uiExpectedVectorSize != xVectorOfSequenceDescriptors.size( ) )
        {
            throw std::runtime_error( "Loading pack failed. Inconsistent or incomplete sequence descriptor data." );
        } // if
    } // method

    /* WARNING! Never do this for some already used (filled) sequence collection.







    * Throws an exception, if something goes wrong.
    * Deserialization of the vector for hole descriptors.
    */
    void vLoadHoleDescriptorVector( const char* pcFileNamePrefix )
    {
        size_t uiExpectedSequenceDescriptorVectorSize = 0;
        size_t uiExpectedHoleDescriptorVectorSize = 0;

        /* TO DO insert a file open check over here!!
         */
        std::ifstream xFileInputStream( fullFileName( pcFileNamePrefix, "amb" ).c_str( ),
                                        std::ios::in | std::ios::binary );

        /* Read the headline
         */
        uint64_t uiSizeOfSingleStrand; // we got this value already in the context of the descriptor
                                       // loading.
        xFileInputStream >> uiSizeOfSingleStrand >> uiExpectedSequenceDescriptorVectorSize >>
            uiExpectedHoleDescriptorVectorSize;

        while( true )
        {
            HoleDescriptor rxEntry;

            /* We read two lines containing sequence descriptor data.
             * The comment(annotation) might contain white-spaces, so we read it using getline.
             */
            xFileInputStream >> rxEntry.offset >> rxEntry.length >> rxEntry.xHoleCharacter;

            if( !xFileInputStream.fail( ) )
            {
                /* Looks good, we add our Sequence descriptor to the vector
                 */
                xVectorOfHoleDescriptors.emplace_back( std::move( rxEntry ) );
            } // if
            else
            {
                break;
            } // else
        } // while

        /* Some very basic test, whether something went wrong.
         */
        if( !xFileInputStream.eof( ) || uiExpectedHoleDescriptorVectorSize != xVectorOfHoleDescriptors.size( ) )
        {
            throw std::runtime_error( "Loading pack failed. Inconsistent or incomplete hole descriptor data." );
        } // if
    } // method

  public:
    inline void printHoles( )
    {
        for( auto& rHole : xVectorOfHoleDescriptors )
            std::cout << rHole.offset << " " << rHole.length << " " << rHole.xHoleCharacter << std::endl;
    } // method

    friend void vTextSequenceCollectionClass( );

    /* Check method, in oder to see whether everything is fine
     */
    bool checkForDefect( )
    {
        std::cout << "Check description vector for consistency." << std::endl;

        decltype( SequenceInPack::uiStartOffsetUnpacked ) uiRunningStartOffsetUnpacked = 0;

        for( size_t uiIndex = 0; uiIndex < xVectorOfSequenceDescriptors.size( ); uiIndex++ )
        {
            if( !( uiRunningStartOffsetUnpacked < this->uiUnpackedSizeForwardStrand ) ||
                !( xVectorOfSequenceDescriptors[ uiIndex ].uiStartOffsetUnpacked == uiRunningStartOffsetUnpacked ) )
            {
                return false;
            } // if

            uiRunningStartOffsetUnpacked += xVectorOfSequenceDescriptors[ uiIndex ].uiLengthUnpacked;
        } // for

        return uiRunningStartOffsetUnpacked == this->uiUnpackedSizeForwardStrand;
    } // verification method

    /* The number of base pairs stored in the pack.
     * (Expressed in forward strand bp)
     */
    uint64_t uiUnpackedSizeForwardStrand; // initialized in constructor

    /* The number of base pairs stored in the pack for forward and revers strand.
     */
    inline uint64_t uiUnpackedSizeForwardPlusReverse( void ) const
    {
        return uiUnpackedSizeForwardStrand * 2;
    } // method

    /* Default constructor.
     */
    Pack( )
        : bPackComprisesReverseStrand( false ),
          xVectorOfSequenceDescriptors( ),
          xVectorOfHoleDescriptors( ),
          xPackedNucSeqs( ),
          seed( uint32_t( time( NULL ) ) ), // random number generator initial value
          uiUnpackedSizeForwardStrand( 0 )
    {
        /* Initialize the random number generator
         */
        srand( seed );
    } // default constructor

    /* constructor.
     */
    Pack( std::string sFileName )
        : bPackComprisesReverseStrand( false ),
          xVectorOfSequenceDescriptors( ),
          xVectorOfHoleDescriptors( ),
          xPackedNucSeqs( ),
          seed( uint32_t( time( NULL ) ) ), // random number generator initial value
          uiUnpackedSizeForwardStrand( 0 )
    {
        /* Initialize the random number generator
         */
        srand( seed );
        this->vLoadCollection( sFileName );
    } // constructor


    // overload
    bool canCast( std::shared_ptr<libMS::Container> c ) const
    {
        return std::dynamic_pointer_cast<Pack>( c ) != nullptr;
    } // function

    // overload
    std::string getTypeName( ) const
    {
        return "Pack";
    } // function

    // overload
    std::shared_ptr<libMS::Container> getType( ) const
    {
        return std::shared_ptr<libMS::Container>( new Pack( ) );
    } // function

    /**
     * @brief compute the percentage of the interval [uiStart, uiEnd) that is covered by holes (Ns).
     * @details
     * This function assumes that no two holes overlap, which should be guaranteed by the pack.
     */
    inline double amountOfRegionCoveredByHole( uint64_t uiStart, uint64_t uiEnd ) const
    {
        assert( uiStart < uiEnd );
        uint64_t uiCovered = 0;

        // check all holes
        for( auto& rHole : xVectorOfHoleDescriptors )
            // check if hole is overlapping
            if( rHole.offset <= uiEnd && rHole.offset + rHole.length > uiStart )
                // add the overlapping interval to covered
                uiCovered += std::min( uiEnd, rHole.offset + rHole.length ) - std::max( uiStart, rHole.offset );

        assert( uiCovered <= uiEnd - uiStart );
        // divide the overlapping interval by the total interval
        return uiCovered / (double)( uiEnd - uiStart );
    } // method

    /**
     * @brief prints wether the nucleotide is in a hole
     */
    inline double isHole( uint64_t uiX ) const
    {
        // check all holes
        for( auto& rHole : xVectorOfHoleDescriptors )
            // check if hole is overlapping
            if( rHole.offset <= uiX && rHole.offset + rHole.length > uiX )
                // add the overlapping interval to covered
                return true;

        return false;
    } // method

    /* Appends a single nucleotide sequence to the collection.
     * TO DO: uiOffsetDistance seems not be required any longer.
     */
    void vAppendSequence( const std::string& rsName, // name of the sequence within the collection
                          const std::string& rsComment, // comment for the sequence
                          const NucSeq& rxSequence // sequence itself (The sequence will be copied
                                                   // and is not referred after method termination.)
    )
    {
        metaMeasureAndLogDuration<false>( "vAppendSequence", [ & ]( ) {
            /* Skip empty sequences, because they may become troublemaker, particularly if the occur on
             * tail positions.
             */
            if( rxSequence.empty( ) )
            {
                std::cerr << "Skip Sequence " << rsName << " because it is empty." << std::endl;
                return;
            } // if

            /* The initial offset of the current sequence, if we see all nucleotides as some contiguous
             * sequence.
             */
            uint64_t uiOffsetDistance = xVectorOfSequenceDescriptors.empty( )
                                            ? 0
                                            : xVectorOfSequenceDescriptors.back( ).uiStartOffsetUnpacked +
                                                  xVectorOfSequenceDescriptors.back( ).uiLengthUnpacked;

            assert( uiOffsetDistance == uiUnpackedSizeForwardStrand );
            DEBUG( auto uiInitialUnpackedSize = uiUnpackedSizeForwardStrand; ) // DEBUG

            /* TO DO: Put this into the vector of sequence descriptions.
             */
            SequenceInPack xPackedSequenceDescriptor(
                rsName, // name of the embedded sequence
                rsComment, // comments for the sequence
                uiOffsetDistance, // offset (number of nucleotides from begin) of the embedded sequence
                rxSequence.length( ) // length of the sequence
            ); // struct construction

            unsigned int uiPreviousSymbolCode = 0;

            /* We iterate over the fresh sequence.
             */
            for( size_t i = 0; i < rxSequence.length( ); i++ )
            {
                unsigned int uiSymbolCode = rxSequence[ i ];

                /* seq->seq.s[i] is our current symbol.
                 * For holes we have some special list, that has to be maintained.
                 */
                if( uiSymbolCode >= 4 )
                { /* Found something like N.
                   */
                    if( uiPreviousSymbolCode == uiSymbolCode )
                    { /* We see the same symbol once again. => Contiguous hole(N).
                       * We increase the size of the current hole in the list of holes. (*q brings us to
                       * the current hole record.)
                       */
                        xVectorOfHoleDescriptors.back( ).length += 1;
                    } // if
                    else
                    { /* First N seen. We append a fresh record to vector of holes and initialize the
                       * record. We have to double the capacity if there is no space anymore.
                       */
                        assert( uiSymbolCode == (unsigned int)'N' || uiSymbolCode == (unsigned int)'n' ||
                                uiSymbolCode == 4 );
                        xVectorOfHoleDescriptors.emplace_back( HoleDescriptor( uiOffsetDistance, // offset
                                                                               'N' // character
                                                                               ) ); // vector insertion

                        /* Our current sequence has one hole more ..
                         */
                        xPackedSequenceDescriptor.uiNumberOfHoles += 1;
                    } // else
                } // if
                uiPreviousSymbolCode = uiSymbolCode;

                /* We create random characters, for N's.
                 * Create random number and map the last 2 bits.
                 */
                if( uiSymbolCode >= 4 )
                {
                    uiSymbolCode = rand( ) & (int)3;
                } // if

                /* Fill the buffer - Put the fresh symbol c into the buffer
                 */
                /* Maps code into the pack and increments the size of the pack.
                 * ( _set_pac divides bns->i64PacSize by 4 combined with the trick of mapping the last 2
                 * bits )
                 */
                unsigned char uiShift = ( ( ~(uiUnpackedSizeForwardStrand)&3 ) << 1 );
                if( uiShift == 6 )
                { /* Fresh element required.
                   */
                    xPackedNucSeqs.emplace_back( uiSymbolCode << uiShift );
                } // if
                else
                { /* We insert our base pair by shifting into the last vector element.
                   */
                    xPackedNucSeqs.back( ) |= ( uiSymbolCode << uiShift );
                } // else

                uiUnpackedSizeForwardStrand++;
                uiOffsetDistance++;
            } // for

            /* Finally we store the descriptor in the collection vector.
             */
            xVectorOfSequenceDescriptors.emplace_back( xPackedSequenceDescriptor );

            assert( xPackedSequenceDescriptor.uiLengthUnpacked > 0 );
            assert( xPackedSequenceDescriptor.uiLengthUnpacked == rxSequence.length( ) );
            assert( uiUnpackedSizeForwardStrand == uiInitialUnpackedSize + rxSequence.length( ) );
        } );
    } // method

    /*
     *    wrapper for boost
     */
    void vAppendSequence_boost( const char* rsName, // name of the sequence within the collection
                                const char* rsComment, // comment for the sequence
                                const std::shared_ptr<NucSeq>
                                    pxSequence // sequence itself (The sequence will be copied and
                                               // is not referred after method termination.)
    )
    {
        vAppendSequence( std::string( rsName ), std::string( rsComment ), *pxSequence );
    }

#ifdef FASTA_READER
    /* Appends a single FASTA record to the collection and pack.
     */
    void DLL_PORT( MA ) vAppendFastaSequence( const FastaDescriptor& rxFastaDescriptor );
    /* Appends a single FASTA record to the collection and pack.
     */
    void DLL_PORT( MA ) vAppendFastaFile( const char* pcFileName );
#endif

    /* Creates the reverse strand a saves the collection on the disk.
     * After finishing a collection it is impossible to add further sequences.
     */
    void vStoreCollection( const std::string& rsPackPrefix )
    {
        assert( debugCheckSequenceDescriptorVector( ) );

        /* 1. Store the .pac file ( pcPackPrefix.pac )
         * 2. Store the .amb file and the .ann file
         */
        vStorePack( rsPackPrefix, xPackedNucSeqs, uiUnpackedSizeForwardStrand );
        vStoreCollectionDescripton( rsPackPrefix );
    } // method

    /* This method is only required in the context of BWT-large, an old code part from the original
     * BWA code. Here we simply store the pure pack together with its reverse strand.
     */
    void vCreateAndStorePackForBWTProcessing( const std::string& rxFilePath ) const
    {
        /* Make a copy of the packed nucleotide sequence.
         */
        decltype( xPackedNucSeqs ) xPackedSequence = xPackedNucSeqs;

        uint64_t uiRequiredSize = ( uiUnpackedSizeForwardPlusReverse( ) + 3 ) / 4; // required size of packed sequence

        /* We adapt the size of the container and set all fresh elements to zero.
         */
        xPackedSequence.resize( (size_t)uiRequiredSize, 0 );

        /* Appends the reverse sequence directly in compressed form.
         * WARNING: forward iterator becomes negative.
         */
        int64_t iReversePosition = (int64_t)uiUnpackedSizeForwardStrand;
        for( int64_t iForwardPosition = iReversePosition - 1; iForwardPosition >= 0; --iForwardPosition )
        {
            /* Create for each forward BP the corresponding reverse strand BP.
             */
            uint8_t uiValue =
                xPackedSequence[ ( size_t )( iForwardPosition >> 2 ) ] >> ( ( ~iForwardPosition & 3UL ) << 1 ) &
                3; // get BP on forward strand

            /* Fixed Bug: uiValue => (3 - uiValue)
             */
            xPackedSequence[ ( size_t )( iReversePosition >> 2 ) ] |=
                ( 3 - uiValue ) << ( ( ~iReversePosition & 3UL ) << 1 ); // set complement on reverse strand
            ++iReversePosition;
        } // for

        /* Store the pack filesystem.
         */
        vStorePack( rxFilePath, xPackedSequence, uiUnpackedSizeForwardPlusReverse( ) );
    } // method
    /* Checks whether the files required for loading a pack does exist on the file system.
     */
    static bool packExistsOnFileSystem( const std::string& rsPrefix )
    {
        return fileExists( rsPrefix + ".pac" ) && fileExists( rsPrefix + ".ann" ) && fileExists( rsPrefix + ".amb" );
    } // method

#ifdef FASTA_READER
    /* Entry point, for the construction of packs.
     * pcPackPrefix is some prefix for the pack-files.
     * Reads all sequences on the file system and creates a sequence collection out of them.
     */
    void DLL_PORT( MA ) vPackFastaFilesDeprecated( const std::vector<std::string>& rxvFileNameOfFastaFiles,
                                                   const char* pcPackPrefix, bool bMakeReverseStand = false );

    /* Entry point, for the construction of packs.
     * pcPackPrefix is some prefix for the pack-files.
     * Reads all sequences on the file system and creates a sequence collection out of them.
     */
    void DLL_PORT( MA ) vAppendFASTA( const std::string& sFastaFilePath );
#endif

    /* Restores a nucleotide sequence collection from the file system using the prefix given as
     * argument.
     */
    void vLoadCollection( const std::string& rsFileNamePrefix )
    {
        if( !packExistsOnFileSystem( rsFileNamePrefix ) ) // if the files of the pack does not
                                                          // exist, we inform the caller.
        {
            throw std::runtime_error( "Tried to load non-existing pack with prefix " + rsFileNamePrefix );
        } // if

        vLoadSequenceDescriptorVector( rsFileNamePrefix.c_str( ) ); // load the .ann file
        vLoadPackedSequence( rsFileNamePrefix, uiUnpackedSizeForwardStrand ); // load the .pac file
        vLoadHoleDescriptorVector( rsFileNamePrefix.c_str( ) ); // load the .amb file

        assert( debugCheckSequenceDescriptorVector( ) );
    } // method

    /* The index of the first element that belongs to the reverse strand.
     */
    inline uint64_t uiStartOfReverseStrand( ) const
    {
        return uiUnpackedSizeForwardStrand;
    } // method

    /* Start of in sequence with id on forward strand.
     */
    uint64_t startOfSequenceWithId( int64_t iSequenceId ) const
    {
        return xVectorOfSequenceDescriptors[ iSequenceId ].uiStartOffsetUnpacked;
    } // method

    /* Start of in sequence with name on forward strand.
     */
    uint64_t startOfSequenceWithName( std::string name ) const
    {
        for( const SequenceInPack& seq : xVectorOfSequenceDescriptors )
            if( seq.sName.compare( name ) == 0 )
                return seq.uiStartOffsetUnpacked;
        return 0;
    } // method

    /* End of in sequence with name on forward strand.
     */
    uint64_t endOfSequenceWithName( std::string name ) const
    {
        for( const SequenceInPack& seq : xVectorOfSequenceDescriptors )
            if( seq.sName.compare( name ) == 0 )
                return seq.uiStartOffsetUnpacked + seq.uiLengthUnpacked;
        return 0;
    } // method

    /* Start of sequence with id on reverse strand.
     */
    uint64_t endOfSequenceWithId( int64_t iSequenceId ) const
    {
        return startOfSequenceWithId( iSequenceId ) + xVectorOfSequenceDescriptors[ iSequenceId ].uiLengthUnpacked;
    } // method

    bool isForwPositionInSequenceWithId( size_t iSequenceId, uint64_t uiPosition ) const
    {
        assert( iSequenceId < xVectorOfSequenceDescriptors.size( ) );
        assert( uiPosition < uiStartOfReverseStrand( ) );
        return startOfSequenceWithId( iSequenceId ) <= uiPosition && uiPosition < endOfSequenceWithId( iSequenceId );
    } // method

    /* Start of in sequence with id on forward strand.
     */
    uint64_t lengthOfSequenceWithName( std::string name ) const
    {
        for( const SequenceInPack& seq : xVectorOfSequenceDescriptors )
            if( seq.sName.compare( name ) == 0 )
                return seq.uiLengthUnpacked;
        return 0;
    } // method

    /* Start of in sequence with id on forward strand.
     */
    uint64_t lengthOfSequenceWithId( int64_t iSequenceId ) const
    {
        return xVectorOfSequenceDescriptors[ iSequenceId ].uiLengthUnpacked;
    } // method

    /* For unaligned sequences we can have request the sequence id -1. (In this case we deliver
     * "*".) FIX ME: This is design flaw from the original BWA code.
     */
    inline const char* nameOfSequenceWithId( int64_t iSequenceId ) const
    {
        return ( iSequenceId >= 0 ) ? xVectorOfSequenceDescriptors[ iSequenceId ].sName.c_str( ) : "*";
    } // method

    /* For unaligned sequences we can have request the sequence id -1. (In this case we deliver
     * "*".) FIX ME: This is design flaw from the original BWA code.
     */
    inline int64_t uiSequenceIdForName( std::string sName ) const
    {
        for( int64_t iId = 0; iId < (int64_t)xVectorOfSequenceDescriptors.size( ); iId++ )
            if( xVectorOfSequenceDescriptors[ iId ].sName == sName )
                return iId;
        return -1;
    } // method

    /* Returns true if given position belongs to reverse strand
     */
    inline bool bPositionIsOnReversStrand( uint64_t uiPosition ) const
    {
        return uiPosition >= uiStartOfReverseStrand( );
    } // method

    /* If the position belongs to the reverse strand, the returned value represents the
     * corresponding position on the forward strand. WARNING: The absolute position can become
     * negative, if we try to catch element behind uiTotalUnpackedSize - 1
     */
    inline int64_t iAbsolutePosition( uint64_t uiPosition ) const
    {
        return bPositionIsOnReversStrand( uiPosition ) ? uiUnpackedSizeForwardPlusReverse( ) - ( uiPosition + 1 )
                                                       : uiPosition;
    } // method

    /* Gives the absolute start position of a sequence
     */
    inline int64_t iAbsolutePosition( uint64_t uiBegin, uint64_t uiEnd ) const
    {
        return bPositionIsOnReversStrand( uiEnd ) ? uiUnpackedSizeForwardPlusReverse( ) - ( uiEnd + 1 ) : uiBegin;
    } // method

    /* Maps a forward strand position to the reverse strand.
     */
    inline uint64_t uiPositionToReverseStrand( uint64_t uiPositionOnForwardStrand ) const
    {
        return uiUnpackedSizeForwardPlusReverse( ) - ( uiPositionOnForwardStrand + 1 );
    } // method

    /* Finds the id (absolute position of the sequence in the collection) of the sequence for some
     * position in the collection. (former bns_pos2rid) Returns -1 if uiPosition is out of range.
     * (uses a binary search inside)
     */
    int64_t uiSequenceIdForPosition( uint64_t uiPosition ) const
    {
        /* FIX ME: Add an exception here, in the case of NDEBUG.
         */
        assert( uiPosition < uiUnpackedSizeForwardPlusReverse( ) );

        /* Map the position to the forward strand, so that we can find the appropriate descriptor.
         * (See attached PPT documentation.)
         */
        const auto iAbsPosition = iAbsolutePosition( uiPosition );
        assert( iAbsPosition >= 0 );

        uint64_t uiLeft = 0;
        uint64_t uiMid = 0;
        uint64_t uiRight = xVectorOfSequenceDescriptors
                               .size( ); // WARNING: size() - 1 doesn't work, because uiMid can't become uiRight

        /* Binary search
         */
        while( uiLeft < uiRight ) // Here is the problem with respect to the above WARNING.
        { /* Get the index of the center element.
           */
            uiMid = ( uiLeft + uiRight ) / 2;

            if( iAbsPosition >= static_cast<int64_t>( xVectorOfSequenceDescriptors[ uiMid ].uiStartOffsetUnpacked ) )
            { /* Limit search space to the right side.
               */
                if( uiMid == xVectorOfSequenceDescriptors.size( ) - 1 )
                {
                    break;
                } // if
                if( iAbsPosition <
                    static_cast<int64_t>( xVectorOfSequenceDescriptors[ uiMid + 1 ].uiStartOffsetUnpacked ) )
                {
                    break; // bracketed
                } // if
                uiLeft = uiMid + 1;
            } // if
            else
            { /* Limit the search space to the left side.
               */
                uiRight = uiMid;
            } // else
        } // while

        /* Some checks whether the outcome is reasonable.
         */
        assert( uiMid < xVectorOfSequenceDescriptors.size( ) );
        assert(
            ( iAbsPosition >= static_cast<int64_t>( xVectorOfSequenceDescriptors[ uiMid ].uiStartOffsetUnpacked ) ) &&
            ( ( uiMid < xVectorOfSequenceDescriptors.size( ) - 1 )
                  ? ( iAbsPosition <
                      static_cast<int64_t>( xVectorOfSequenceDescriptors[ uiMid + 1 ].uiStartOffsetUnpacked ) )
                  : true ) // conditional operator
        ); // assert

        return uiMid;
    } // method


    /* Name of the sequence on position.
     */
    std::string nameOfSequenceForPosition( uint64_t uiPosition ) const
    {
        return std::string( nameOfSequenceWithId( uiSequenceIdForPosition( uiPosition ) ) );
    } // method

    /** Returns true if the section defined by both arguments has bridging properties.
     * Returns false for a non-bridging section, where the sequence id belonging to the section is
     * transferred via the reference variable. Returns the sequence id additionally.
     */
    bool bridgingSubsection( const uint64_t uiBegin, const uint64_t uiSize, int64_t& riSequenceId ) const
    {
        if( uiSize > 0 )
        {
            riSequenceId = uiSequenceIdForPosition( uiBegin );
            if( uiBegin + uiSize > uiUnpackedSizeForwardStrand * 2 )
                return true;
            return ( bPositionIsOnReversStrand( uiBegin ) !=
                     bPositionIsOnReversStrand( ( uiBegin + uiSize ) - 1 ) ) // bridging forward reverse border
                   || ( riSequenceId !=
                        uiSequenceIdForPosition( ( uiBegin + uiSize ) - 1 ) ); // section crosses different sequences
        } // if
        else
        {
            return false;
        } // else
    } // method

    /*
     * Return a sequence id including the reverse complement.
     * this is done by multiplying the id's by 2
     * an even number is on the forward strand
     * an odd on the reverse
     * in order to get the initial id do floor(id/2)
     */
    int64_t uiSequenceIdForPositionOrRev( uint64_t uiPosition ) const
    {
        if( bPositionIsOnReversStrand( uiPosition ) )
            return uiSequenceIdForPosition( uiPositionToReverseStrand( uiPosition ) ) * 2 + 1;
        return uiSequenceIdForPosition( uiPosition ) * 2;
    } // function

    /*
     * @see uiSequenceIdForPositionOrRev
     * only use with ids from uiSequenceIdForPositionOrRev
     */
    uint64_t endOfSequenceWithIdOrReverse( int64_t iSequenceId ) const
    {
        if( iSequenceId % 2 == 1 )
            return uiPositionToReverseStrand( startOfSequenceWithId( iSequenceId / 2 ) ) - 1;
        return endOfSequenceWithId( iSequenceId / 2 );
    } // function

    uint64_t startOfSequenceWithIdOrReverse( int64_t iSequenceId ) const
    {
        if( iSequenceId % 2 == 1 )
            return uiPositionToReverseStrand( endOfSequenceWithId( iSequenceId / 2 ) ) + 1;
        return startOfSequenceWithId( iSequenceId / 2 );
    } // function

    uint64_t lengthOfSequenceWithIdOrReverse( int64_t iSequenceId ) const
    {
        if( iSequenceId % 2 == 1 )
            return uiPositionToReverseStrand( startOfSequenceWithId( iSequenceId / 2 ) ) - 1;
        return lengthOfSequenceWithId( iSequenceId / 2 );
    } // function

    /* Gives the relative position
     */
    inline uint64_t posInSequence( uint64_t uiBegin, uint64_t uiEnd ) const
    {
        auto uiPosition = iAbsolutePosition( uiBegin, uiEnd );
        return uiPosition - startOfSequenceWithId( uiSequenceIdForPosition( uiPosition ) );
    } // method

    /** Returns true if the section defined by both arguments has bridging properties.
     * Returns false for a non-bridging section.
     */
    bool bridgingSubsection( const uint64_t uiBegin, const uint64_t uiSize ) const
    {
        assert( uiBegin + uiSize < uiUnpackedSizeForwardPlusReverse( ) );
        if( uiSize > 0 )
        {
            int64_t riSequenceId = uiSequenceIdForPositionOrRev( uiBegin );
            // bridging forward reverse border
            return ( bPositionIsOnReversStrand( uiBegin ) != bPositionIsOnReversStrand( ( uiBegin + uiSize ) - 1 ) ) ||
                   ( riSequenceId !=
                     uiSequenceIdForPositionOrRev( ( uiBegin + uiSize ) - 1 ) ); // section crosses different sequences
        } // if
        else
        {
            return false;
        } // else
    } // method

    /** Returns true if the section defined by both arguments has bridging properties.
     * Returns false for a non-bridging section.
     */
    bool bridgingPositions( const uint64_t uiA, const uint64_t uiB ) const
    {
        // bridging forward reverse border
        return bPositionIsOnReversStrand( uiA ) != bPositionIsOnReversStrand( uiB ) ||
               // section crosses different sequences
               uiSequenceIdForPositionOrRev( uiA ) != uiSequenceIdForPositionOrRev( uiB );
    } // method

    bool onContigBorder( const uint64_t uiA ) const
    {
        if( uiA == 0 )
            return true;
        return bridgingPositions( uiA - 1, uiA );
    } // method

    /*
     * boost can't handle overloads
     * thus we create a un-overloaded function for it...
     */
    bool bridgingSubsection_boost( const uint64_t uiBegin, const uint64_t uiSize ) const
    {
        return bridgingSubsection( uiBegin, uiSize );
    } // function


    /** changes uiBegin and uiSize so that they return the larges possible subinterval that is
     * non bridging
     * undefined behaviour for a non-bridging interval
     */
    void unBridgeSubsection( uint64_t& uiBegin, uint64_t& uiSize ) const
    {
        DEBUG( assert( bridgingSubsection( uiBegin, uiSize ) ); uint64_t uiSizeOriginal = uiSize; )
        assert( uiBegin + uiSize < uiUnpackedSizeForwardPlusReverse( ) );
        int64_t startId = uiSequenceIdForPositionOrRev( uiBegin );

        uint64_t uiSplit = endOfSequenceWithIdOrReverse( startId );
        assert( uiBegin <= uiSplit );
        if( uiBegin + uiSize / 2 > uiSplit )
        {
            uiSize = uiSize + uiBegin - uiSplit;
            uiBegin = uiSplit;
        } // if
        else
        {
            uiSize = uiSplit - uiBegin;
        } // else
        DEBUG( assert( uiSize <= uiSizeOriginal ); )
    } // method

    /* Extracts some subsequence from the packed sequence. (original name bns_get_seq)
     * The reverse strand will be automatically extracted on the foundation of the forward strand.
     * Bridging between forward strand and reverse strand is not allowed.
     * FIX ME: Here we have to integrate ambiguous base pairs, by restoring data form the "hole
     * section".
     */
    void vExtractSubsection( const int64_t iBegin, // begin of extraction
                             const int64_t iEnd, // end of extraction
                             NucSeq& rxSequence, // receiver of the extraction process
                             bool bAppend = false // deliver true, if you would like to append to an
                                                  // existing nucleotide sequence
    ) const
    {
        /* Do range-check for begin and end of extraction.
         */
        vRangeCheckAndThrowExclusive( "(vExtractSubsection)", static_cast<int64_t>( 0 ), iBegin,
                                      static_cast<int64_t>( uiUnpackedSizeForwardPlusReverse( ) ) );
        vRangeCheckAndThrowInclusive( "(vExtractSubsection)", static_cast<int64_t>( 0 ), iEnd,
                                      static_cast<int64_t>( uiUnpackedSizeForwardPlusReverse( ) ) );

        /* Check whether both sequence belong to the same strand.
         */
        if( bPositionIsOnReversStrand( iBegin ) != bPositionIsOnReversStrand( iEnd - 1 ) )
        {
            throw std::runtime_error( "(vExtractSubsection) Try to extract bridging sequence. This is impossible." );
        } // if

        if( !( iBegin <= iEnd ) )
        { /* In the case of begin not smaller than end, we give up and throw an exception.
           */
            throw std::runtime_error( "(vExtractSubsection) Try to extract with begin greater than end." );
        } // if

        /* Prepare sequence
         */
        if( !bAppend )
        {
            rxSequence.vClear( );
        } // if

        /* Compute absolute positions. (Forward strand positions)
         */
        const bool bExtractAsOnReverseStrand = bPositionIsOnReversStrand( iBegin );

        /* Prepare extraction process by initializing a iterator and resizing the sequence
         */
        uint64_t uiSequenceIterator = bAppend ? rxSequence.length( ) : 0;
        rxSequence.resize( bAppend ? rxSequence.length( ) + ( iEnd - iBegin ) : iEnd - iBegin );

#define CHECK_HOLE_DESC ( 0 )

        if( !bExtractAsOnReverseStrand )
        { /* Extract as forward strand.
           */
#if CHECK_HOLE_DESC == 1
            const auto itEnd = xVectorOfHoleDescriptors.end( );
            auto itHolesDesc = xVectorOfHoleDescriptors.begin( );
#endif
            for( auto iPosition = iBegin; iPosition < iEnd; ++iPosition )
            {
#if CHECK_HOLE_DESC == 1
                // move the hole iterator forwards
                while( itHolesDesc != itEnd && itHolesDesc->offset + itHolesDesc->length <= (uint64_t)iPosition )
                    itHolesDesc++;
                // append an N if we are currently within a hole
                if( itHolesDesc != itEnd && itHolesDesc->offset <= (uint64_t)iPosition )
                    rxSequence[ uiSequenceIterator++ ] = itHolesDesc->xHoleCharacter; // 4 == N
                else // otherwise get the correct nucleotide
#endif
                    rxSequence[ uiSequenceIterator++ ] = getNucleotideOnPos( iPosition );
            } // for
        } // if
        else
        { /* Extract as reverse strand. (begin is now bigger than end)
           */
#if CHECK_HOLE_DESC == 1
            const auto itEnd = xVectorOfHoleDescriptors.rend( );
            auto itHolesDesc = xVectorOfHoleDescriptors.rbegin( );
#endif
            int64_t iAbsoluteBegin = iAbsolutePosition( iBegin );
            int64_t iAbsoluteEnd = iAbsolutePosition( iEnd );
            for( int64_t iPosition = iAbsoluteBegin; iPosition > iAbsoluteEnd; --iPosition )
            {
#if CHECK_HOLE_DESC == 1
                // move the (reversed) hole iterator forwards
                while( itHolesDesc != itEnd && itHolesDesc->offset > (uint64_t)iPosition )
                    itHolesDesc++;
                // append an N if we are currently within a hole
                if( itHolesDesc != itEnd && itHolesDesc->offset + itHolesDesc->length > (uint64_t)iPosition )
                    rxSequence[ uiSequenceIterator++ ] = itHolesDesc->xHoleCharacter; // 4 == N
                else // otherwise get the correct nucleotide
#endif
                    rxSequence[ uiSequenceIterator++ ] = 3 - getNucleotideOnPos( iPosition );
            } // for
        } // else
    } // method

    void vExtractSubsectionN( const int64_t iBegin, // begin of extraction
                              const int64_t iEnd, // end of extraction
                              NucSeq& rxSequence, // receiver of the extraction process
                              bool bAppend = false // deliver true, if you would like to append to
                                                   // an existing nucleotide sequence
    ) const
    {
        metaMeasureAndLogDuration<false>( "vExtractSubsectionN", [ & ]( ) {
            /* Prepare sequence
             */
            if( !bAppend )
            {
                rxSequence.vClear( );
            } // if

            // sequence is of size 0; we don't have to do anything
            if( iBegin == iEnd )
                return;
            /* Do range-check for begin and end of extraction.
             */
            vRangeCheckAndThrowExclusive( "(vExtractSubsectionN)", static_cast<int64_t>( 0 ), iBegin,
                                          static_cast<int64_t>( uiUnpackedSizeForwardPlusReverse( ) ) );
            vRangeCheckAndThrowInclusive( "(vExtractSubsectionN)", static_cast<int64_t>( 0 ), iEnd,
                                          static_cast<int64_t>( uiUnpackedSizeForwardPlusReverse( ) ) );

            /* Check whether both sequence belong to the same strand.
             */
            if( bPositionIsOnReversStrand( iBegin ) != bPositionIsOnReversStrand( iEnd - 1 ) )
            {
                throw std::runtime_error(
                    "(vExtractSubsectionN) Try to extract bridging sequence. This is impossible." );
            } // if

            if( !( iBegin <= iEnd ) )
            { /* In the case of begin not smaller than end, we give up and throw an exception.
               */
                throw std::runtime_error( "(vExtractSubsectionN) Try to extract with begin greater than end." );
            } // if


            /* Compute absolute positions. (Forward strand positions)
             */
            const bool bExtractAsOnReverseStrand = bPositionIsOnReversStrand( iBegin );

            /* Prepare extraction process by initializing a iterator and resizing the sequence
             */
            uint64_t uiSequenceIterator = bAppend ? rxSequence.length( ) : 0;
            rxSequence.resize( bAppend ? rxSequence.length( ) + ( iEnd - iBegin ) : iEnd - iBegin );


            if( !bExtractAsOnReverseStrand )
            { /* Extract as forward strand.
               */
                const auto itEnd = xVectorOfHoleDescriptors.end( );
                auto itHolesDesc = std::lower_bound( xVectorOfHoleDescriptors.begin( ), xVectorOfHoleDescriptors.end( ),
                                                     (uint64_t)iBegin,
                                                     []( const libMA::Pack::HoleDescriptor& rX, uint64_t iPosition ) {
                                                         return rX.offset + rX.length <= iPosition;
                                                     } );
                for( auto iPosition = iBegin; iPosition < iEnd; ++iPosition )
                {
                    // move the hole iterator forwards
                    while( itHolesDesc != itEnd && itHolesDesc->offset + itHolesDesc->length <= (uint64_t)iPosition )
                        itHolesDesc++;
                    // append an N if we are currently within a hole
                    if( itHolesDesc != itEnd && itHolesDesc->offset <= (uint64_t)iPosition )
                        rxSequence[ uiSequenceIterator++ ] = 4; // itHolesDesc->xHoleCharacter; // 4 == N @fixme
                    else // otherwise get the correct nucleotide
                        rxSequence[ uiSequenceIterator++ ] = getNucleotideOnPos( iPosition );
                } // for
            } // if
            else
            { /* Extract as reverse strand. (begin is now bigger than end)
               */
                int64_t iAbsoluteBegin = iAbsolutePosition( iBegin );
                int64_t iAbsoluteEnd = iAbsolutePosition( iEnd );
                const auto itEnd = xVectorOfHoleDescriptors.rend( );
                auto itHolesDesc = std::lower_bound(
                    xVectorOfHoleDescriptors.rbegin( ), xVectorOfHoleDescriptors.rend( ), (uint64_t)iAbsoluteBegin,
                    []( const libMA::Pack::HoleDescriptor& rX, uint64_t iPosition ) { return rX.offset > iPosition; } );
                for( int64_t iPosition = iAbsoluteBegin; iPosition > iAbsoluteEnd; --iPosition )
                {
                    // move the (reversed) hole iterator forwards
                    while( itHolesDesc != itEnd && itHolesDesc->offset > (uint64_t)iPosition )
                        itHolesDesc++;
                    // append an N if we are currently within a hole
                    if( itHolesDesc != itEnd && itHolesDesc->offset + itHolesDesc->length > (uint64_t)iPosition )
                        rxSequence[ uiSequenceIterator++ ] = 4; // itHolesDesc->xHoleCharacter; // 4 == N @fixme
                    else // otherwise get the correct nucleotide
                        rxSequence[ uiSequenceIterator++ ] = 3 - getNucleotideOnPos( iPosition );
                } // for
            } // else
        } );
    } // method

    /**
     * @brief Extracts a sequence (with N's) from the beginning of the respective chromosome till iPos
     */
    uint64_t vExtractUntil( const int64_t iPos, // end of extraction
                            NucSeq& rxSequence, // receiver of the extraction process
                            bool bAppend = false // deliver true, if you would like to append to
                                                 // an existing nucleotide sequence
    ) const
    {
        vExtractSubsectionN( startOfSequenceWithIdOrReverse( uiSequenceIdForPositionOrRev( iPos ) ), iPos, rxSequence,
                             bAppend );
        return startOfSequenceWithIdOrReverse( uiSequenceIdForPositionOrRev( iPos ) );
    } // method

    /**
     * @brief Extracts a sequence (with N's) from iPos till the end of the respective chromosome
     */
    uint64_t vExtractFrom( const int64_t iPos, // start of extraction
                           NucSeq& rxSequence, // receiver of the extraction process
                           bool bAppend = false // deliver true, if you would like to append to
                                                // an existing nucleotide sequence
    ) const
    {
        vExtractSubsectionN( iPos, endOfSequenceWithIdOrReverse( uiSequenceIdForPositionOrRev( iPos ) ), rxSequence,
                             bAppend );
        return endOfSequenceWithIdOrReverse( uiSequenceIdForPositionOrRev( iPos ) );
    } // method

    /**
     * @brief Extracts a sequence (with N's) from/untill iPos depending on the context given
     */
    uint64_t vExtractContext( const int64_t iPos, // start/end of extraction
                              NucSeq& rxSequence, // receiver of the extraction process
                              bool bAppend, // deliver true, if you would like to append to
                                            // an existing nucleotide sequence
                              bool bForwardContext ) const
    {

        if( bForwardContext )
            return vExtractFrom( iPos, rxSequence, bAppend );
        else
            return vExtractUntil( iPos, rxSequence, bAppend );
    } // method

    /**
     * @brief Extracts a complete contig (with N's)
     */
    void vExtractContig( const int64_t iId, // contig id
                         NucSeq& rxSequence, // receiver of the extraction process
                         bool bAppend = false // deliver true, if you would like to append to
                                              // an existing nucleotide sequence
    ) const
    {
        vExtractSubsectionN( startOfSequenceWithIdOrReverse( iId ), endOfSequenceWithIdOrReverse( iId ), rxSequence,
                             bAppend );
    } // method

    /* Unpacks the complete collection (forward as well as revers strand) as a single sequence into
     * rxSequence.
     */
    std::shared_ptr<NucSeq> vColletionAsNucSeq( ) const
    {
        std::shared_ptr<NucSeq> pRet( new NucSeq( ) );
        vExtractSubsection( 0, uiStartOfReverseStrand( ),
                            *pRet ); // get the forward strand
        vExtractSubsection( uiStartOfReverseStrand( ), uiUnpackedSizeForwardPlusReverse( ), *pRet,
                            true ); // get the reverse strand (true triggers appending)
        return pRet;
    } // method

    /* Unpacks the forward strand sequences of the collection as a single sequence into rxSequence.
     */
    std::shared_ptr<NucSeq> vColletionWithoutReverseStrandAsNucSeq( ) const
    {
        std::shared_ptr<NucSeq> pRet( new NucSeq( ) );
        if( this->uiUnpackedSizeForwardStrand == 0 )
            return pRet;
        vExtractSubsection( 0, uiStartOfReverseStrand( ),
                            *pRet ); // get the forward strand
        return pRet;
    } // method

    /* Unpacks the forward strand sequences of the collection as a single sequence into rxSequence.
     */
    std::shared_ptr<NucSeq> vColletionWithoutReverseStrandAsNucSeqWithN( ) const
    {
        std::shared_ptr<NucSeq> pRet( new NucSeq( ) );
        vExtractSubsectionN( 0, uiStartOfReverseStrand( ),
                             *pRet ); // get the forward strand
        return pRet;
    } // method


    /* Get the value at position uiPosition in the unpacked sequence.
     * Works only for the virtual forward strand.
     */
    inline uint8_t vExtract( const int64_t uiPosition ) const
    {
        if( bPositionIsOnReversStrand( uiPosition ) )
            return NucSeq::nucleotideComplement( getNucleotideOnPos( uiPositionToReverseStrand( uiPosition ) ) );
        else
            return getNucleotideOnPos( uiPosition );
    } // inline method


    /* Unpacks the forward strand sequences of the collection as a single sequence into rxSequence.
     */
    std::shared_ptr<NucSeq> vExtract( const int64_t iBegin, // begin of extraction
                                      const int64_t iEnd // end of extraction
    ) const
    {
        std::shared_ptr<NucSeq> pRet( new NucSeq( ) );
        vExtractSubsection( iBegin, iEnd,
                            *pRet ); // get the forward strand
        return pRet;
    } // method


    /* Unpacks the forward strand sequences of the collection as a single sequence into rxSequence.
     */
    std::shared_ptr<NucSeq> vExtractPy( const int64_t iBegin, // begin of extraction
                                        const int64_t iEnd // end of extraction
    ) const
    {
        std::shared_ptr<NucSeq> pRet( new NucSeq( ) );
        vExtractSubsectionN( iBegin, iEnd,
                             *pRet ); // get the forward strand
        return pRet;
    } // method

    // markus

    std::shared_ptr<NucSeq> vColletionOnlyReverseStrandAsNucSeq( ) const
    {
        std::shared_ptr<NucSeq> pRet( new NucSeq( ) );
        vExtractSubsection( uiStartOfReverseStrand( ), uiUnpackedSizeForwardPlusReverse( ), *pRet,
                            true ); // get the reverse strand (true triggers appending)
        return pRet;
    } // method

    std::vector<std::string> contigNames( ) const
    {
        std::vector<std::string> vRet;
        for( auto xContig : xVectorOfSequenceDescriptors )
            vRet.push_back( xContig.sName );
        return vRet;
    } // method

    std::vector<nucSeqIndex> contigLengths( ) const
    {
        std::vector<nucSeqIndex> vRet;
        for( auto xContig : xVectorOfSequenceDescriptors )
            vRet.push_back( xContig.uiLengthUnpacked );
        return vRet;
    } // method

    std::vector<nucSeqIndex> contigStarts( ) const
    {
        std::vector<nucSeqIndex> vRet;
        for( auto xContig : xVectorOfSequenceDescriptors )
            vRet.push_back( xContig.uiStartOffsetUnpacked );
        return vRet;
    } // method

    size_t uiNumContigs( ) const
    {
        return xVectorOfSequenceDescriptors.size( );
    }

    std::vector<std::string> contigSeqs( ) const
    {
        std::vector<std::string> vRet;
        NucSeq s;
        for( size_t uiI = 0; uiI < uiNumContigs( ); uiI++ )
        {
            // *2 since vExtractContig uses id system with forward and reverse contig ids...
            vExtractContig( uiI * 2, s, false );
            vRet.push_back( s.toString( ) );
        } // for
        return vRet;
    } // method

    std::vector<std::shared_ptr<NucSeq>> contigNucSeqs( ) const
    {
        std::vector<std::shared_ptr<NucSeq>> vRet;
        for( size_t uiI = 0; uiI < uiNumContigs( ); uiI++ )
        {
            // *2 since vExtractContig uses id system with forward and reverse contig ids...
            vRet.push_back( std::make_shared<NucSeq>( ) );
            vExtractContig( uiI * 2, *vRet.back( ), false );
            vRet.back( )->sName = xVectorOfSequenceDescriptors[ uiI ].sName;
        } // for
        return vRet;
    } // method
    // end markus

    /* Align iBegin and iEnd, so that they span only over the sequence indicated by middle.
     * If riBegin and riEnd are already with the sequence indicated by middle we do not change their
     * values.
     */
    void vAlignPositions( int64_t& riBegin, const int64_t iMiddle, int64_t& riEnd ) const
    {
        assert( ( riBegin <= iMiddle ) && ( iMiddle < riEnd ) ); // consistency check

        int64_t iSequenceId = uiSequenceIdForPosition( iMiddle ); // delivers sequence id
        const uint64_t iSequenceBegin = startOfSequenceWithId( iSequenceId ); // on forward strand
        const uint64_t iSequenceEnd = endOfSequenceWithId( iSequenceId ); // on reverse strand

        /* Consistency check.
         */
        assert( ( (int64_t)iSequenceBegin <= iAbsolutePosition( iMiddle ) ) &&
                ( iAbsolutePosition( iMiddle ) < (int64_t)iSequenceEnd ) );
        if( !bPositionIsOnReversStrand( iMiddle ) )
        { /* On forward strand.
           */
            if( (int64_t)iSequenceBegin > riBegin )
                riBegin = iSequenceBegin;
            if( (int64_t)iSequenceEnd < riEnd )
                riEnd = iSequenceEnd;

            assert( riBegin <= riEnd ); // consistency check
        } // if
        else
        { /* On reverse strand.
           */
            if( (int64_t)uiPositionToReverseStrand( iSequenceEnd ) + 1 > riBegin )
                riBegin = uiPositionToReverseStrand( iSequenceEnd ) + 1;
            if( (int64_t)uiPositionToReverseStrand( iSequenceBegin ) + 1 < riEnd )
                riEnd = uiPositionToReverseStrand( iSequenceBegin ) + 1;

            assert( riBegin <= riEnd ); // consistency check
        } // else
    } // method
}; // class
} // namespace libMA

#ifdef WITH_PYTHON
/**
 * @brief exports the Pack class to python.
 * @ingroup export
 */
#ifdef WITH_BOOST
void exportPack( );
#else
void exportPack( libMS::SubmoduleOrganizer& xOrganizer );
#endif

#endif
