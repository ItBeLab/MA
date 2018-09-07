/** 
 * @file fileWriter.cpp
 * @author Markus Schmidt
 */
#include "module/fileWriter.h"

using namespace libMA;



ContainerVector FileWriter::getInputType() const
{
    return ContainerVector{
            std::shared_ptr<NucSeq>(new NucSeq()),
            std::shared_ptr<NucSeq>(new NucSeq()),
            std::shared_ptr<ContainerVector>(
                new ContainerVector(std::shared_ptr<Alignment>(new Alignment()))),
            std::shared_ptr<Pack>(new Pack())
        };
}//function

std::shared_ptr<Container> FileWriter::getOutputType() const
{
    return std::shared_ptr<Container>(new Nil());
}//function


ContainerVector RadableFileWriter::getInputType() const
{
    return ContainerVector{
            std::shared_ptr<NucSeq>(new NucSeq()),
            std::shared_ptr<ContainerVector>(
                new ContainerVector(std::shared_ptr<Alignment>(new Alignment()))),
            std::shared_ptr<Pack>(new Pack())
        };
}//function

std::shared_ptr<Container> RadableFileWriter::getOutputType() const
{
    return std::shared_ptr<Container>(new Nil());
}//function

std::shared_ptr<Container> FileWriter::execute(std::shared_ptr<ContainerVector> vpInput)
{
    std::shared_ptr<NucSeq> pQuery =
        std::dynamic_pointer_cast<NucSeq>((*vpInput)[0]); // dc
    std::shared_ptr<NucSeq> pQuery2 =
        std::dynamic_pointer_cast<NucSeq>((*vpInput)[1]); // dc
    std::shared_ptr<ContainerVector> pAlignments =
        std::dynamic_pointer_cast<ContainerVector>((*vpInput)[2]); // dc
    std::shared_ptr<Pack> pPack =
        std::dynamic_pointer_cast<Pack>((*vpInput)[3]); // dc

    std::string sCombined = "";
    for(std::shared_ptr<Container> pA : *pAlignments)
    {
        std::shared_ptr<Alignment> pAlignment = std::dynamic_pointer_cast<Alignment>(pA); // dc
        if(pAlignment->length() == 0)
            continue;
        std::string sCigar = pAlignment->cigarString(*pPack);

        uint32_t flag = pAlignment->getSamFlag(*pPack);

        std::string sContigOther = "*";
        std::string sPosOther = "0";
        std::string sName = pQuery->sName;
        std::string sSegment = pAlignment->getQuerySequence(*pQuery, *pPack);
        std::string sTlen = std::to_string(pAlignment->uiEndOnQuery - pAlignment->uiBeginOnQuery);
        // paired
        if( pAlignment->xStats.pOther.lock() != nullptr )
        {
            assert(pQuery2 != nullptr);
            //flag |= pAlignment->xStats.bFirst ? FIRST_IN_TEMPLATE // flag not actually required
            //                                  : LAST_IN_TEMPLATE;
            flag |= MULTIPLE_SEGMENTS_IN_TEMPLATE | SEGMENT_PROPERLY_ALIGNED;
            if(pPack->bPositionIsOnReversStrand(pAlignment->xStats.pOther.lock()->uiBeginOnRef))
                flag |= NEXT_REVERSE_COMPLEMENTED;

            sContigOther = pAlignment->xStats.pOther.lock()->getContig(*pPack);
            sPosOther = std::to_string(pAlignment->xStats.pOther.lock()->getSamPosition(*pPack));

            if( ! pAlignment->xStats.bFirst)
            {
                sSegment = pAlignment->getQuerySequence(*pQuery2, *pPack);
                sName = pQuery2->sName;
                sTlen = "-" + sTlen;
            }// if
            DEBUG(
                if( pQuery->uiFromLine != pQuery2->uiFromLine )
                {
                    std::cerr << "outputting paired alignment for reads from different lines: "
                        << pQuery->uiFromLine << " and " << pQuery2->uiFromLine
                        << "; query names are: " << pQuery->sName << " and " << pQuery2->sName
                        << std::endl;
                }// if
            )// DEBUG
        }// if

        std::string sRefName = pAlignment->getContig(*pPack);
        // sam file format has 1-based indices bam 0-based...
        auto uiRefPos = pAlignment->getSamPosition(*pPack) + 1;

        DEBUG(// check if the position that is saved to the file is correct
            bool bWrong = false;
            if(pPack->bPositionIsOnReversStrand(pAlignment->uiBeginOnRef))
            {
                //@todo frill in this self check...
            }// if
            else
            {
                if(pAlignment->uiBeginOnRef != pPack->startOfSequenceWithName(sRefName)+uiRefPos-1)
                    bWrong = true;
            }// else

            if(bWrong)
            {
                std::cerr << "Error: Tried to write wrong index to file" << std::endl;
                std::cout << "Have: " << sRefName
                          << " (= " << pPack->startOfSequenceWithName(sRefName) << ") "
                          << uiRefPos << std::endl;
                std::cout << "Wanted: " << pAlignment->uiBeginOnRef << " " << uiRefPos << std::endl;
                if(pPack->bPositionIsOnReversStrand(pAlignment->uiBeginOnRef))
                    std::cout << "Is reverse: True" << std::endl;
                else
                    std::cout << "Is reverse: False" << std::endl;
                exit(0);
            }// if
        )// DEBUG

        //std::string sQual = pQuery->fromToQual(pAlignment->uiBeginOnQuery, pAlignment->uiEndOnQuery);
        std::string sMapQual;
        if(std::isnan(pAlignment->fMappingQuality))
            sMapQual = "255";
        else
            sMapQual = std::to_string( static_cast<int>(std::ceil(pAlignment->fMappingQuality * 254)) );

        sCombined += 
            //query name
            sName + "\t" + 
            //alignment flag
            std::to_string(flag) + "\t" + 
            //reference name
            sRefName + "\t" + 
            //pos
            std::to_string(uiRefPos) + "\t" + 
            //mapping quality
            sMapQual + "\t" + 
            //cigar
            sCigar  + "\t" + 
            //Ref. name of the mate/next read
            sContigOther + "\t" + 
            //Position of the mate/next read
            sPosOther + "\t" + 
            //observed Template length
            sTlen + "\t" + 
            //segment sequence
            sSegment + "\t"
            //ASCII of Phred-scaled base Quality+33
            + "*\n";
    }//for

    if(sCombined.size() > 0)
    {// scope xGuard
        //synchronize file output
        std::lock_guard<std::mutex> xGuard(*pLock);

        //print alignment
        // flushing will be done in the deconstructor
        *pOut << sCombined;
    }// if & scope xGuard
    return std::shared_ptr<Container>(new Nil());
}//function


std::shared_ptr<Container> RadableFileWriter::execute(std::shared_ptr<ContainerVector> vpInput)
{
    std::shared_ptr<NucSeq> pQuery =
        std::dynamic_pointer_cast<NucSeq>((*vpInput)[0]); // dc
    std::shared_ptr<ContainerVector> pAlignments =
        std::dynamic_pointer_cast<ContainerVector>((*vpInput)[1]); // dc
    std::shared_ptr<Pack> pPack =
        std::dynamic_pointer_cast<Pack>((*vpInput)[2]); // dc

    for(std::shared_ptr<Container> pA : *pAlignments)
    {
        std::shared_ptr<Alignment> pAlignment = std::dynamic_pointer_cast<Alignment>(pA); // dc
        if(pAlignment->length() == 0)
            continue;
        
        std::string sPaired = "";
        //paired
        if(!pAlignment->xStats.pOther.expired())
            sPaired = pAlignment->xStats.bFirst ? 
                "first mate of read pair" : "second mate of read pair";

        std::string sRefName = pPack->nameOfSequenceWithId(pPack->uiSequenceIdForPosition(pAlignment->uiBeginOnRef));
        //1 based index... 
        std::string sRefPos = std::to_string( 1 + pAlignment->uiBeginOnRef - pPack->startOfSequenceWithId(pPack->uiSequenceIdForPosition(pAlignment->uiBeginOnRef)));
        std::string sSegmentQuery = pQuery->fromTo(pAlignment->uiBeginOnQuery, pAlignment->uiEndOnQuery);
        std::string sQueryPos = std::to_string(pAlignment->uiBeginOnQuery);
        std::string sSegmentRef = pPack->vExtract(pAlignment->uiBeginOnRef, pAlignment->uiEndOnRef)->toString();
        //std::string sQual = pQuery->fromToQual(pAlignment->uiBeginOnQuery, pAlignment->uiEndOnQuery);
        std::string sMapQual;
        if(std::isnan(pAlignment->fMappingQuality))
            sMapQual = "255";
        else
            sMapQual = std::to_string( (int)(pAlignment->fMappingQuality * 254));

        std::string sQueryLine;
        std::string sMatchLine;
        std::string sRefLine;
        unsigned int uiAlignmentCounter = 0;
        unsigned int uiQueryCounter = 0;
        unsigned int uiRefCounter = 0;

        {
            //synchronize output
            std::lock_guard<std::mutex> xGuard(*pLock);
            *pOut << "Score: " << std::to_string(pAlignment->score()) << "\nBegin on reference sequence: " << sRefName << " at position: " << sRefPos << "\nBegin on Query: " << sQueryPos << (pAlignment->bSecondary ? " Secondary\n" : "\n");
            for(std::tuple<MatchType, nucSeqIndex> section : pAlignment->data)
            {
                for(unsigned int i=0; i< std::get<1>(section);i++)
                {
                    if (uiAlignmentCounter % uiNucsPerLine == 0)
                        *pOut << std::to_string(uiAlignmentCounter)
                            << "-" << std::to_string(uiAlignmentCounter+uiNucsPerLine) << "\n";
                    switch(std::get<0>(section))
                    {
                        case MatchType::match:
                            sQueryLine += sSegmentQuery[uiQueryCounter++];
                            sRefLine += sSegmentRef[uiRefCounter++];
                            sMatchLine += "|";
                            break;
                        case MatchType::seed:
                            sQueryLine += sSegmentQuery[uiQueryCounter++];
                            sRefLine += sSegmentRef[uiRefCounter++];
                            sMatchLine += "I";
                            break;
                        case MatchType::missmatch:
                            sQueryLine += sSegmentQuery[uiQueryCounter++];
                            sRefLine += sSegmentRef[uiRefCounter++];
                            sMatchLine += " ";
                            break;
                        case MatchType::insertion:
                            sQueryLine += sSegmentQuery[uiQueryCounter++];
                            sRefLine += "-";
                            sMatchLine += " ";
                            break;
                        case MatchType::deletion:
                            sQueryLine += "-";
                            sRefLine += sSegmentRef[uiRefCounter++];
                            sMatchLine += " ";
                            break;
                    }//switch
                    if (++uiAlignmentCounter % uiNucsPerLine == 0)
                    {
                        *pOut << sQueryLine << "\tQuery\n";
                        *pOut << sMatchLine << "\n";
                        *pOut << sRefLine << "\tReference\n\n";
                        sQueryLine = "";
                        sRefLine = "";
                        sMatchLine = "";
                    }//if
                }//for
            }//for
            if(uiAlignmentCounter % uiNucsPerLine == 0)
                return std::shared_ptr<Container>(new Nil());
            while (uiAlignmentCounter++ % uiNucsPerLine != 0)
            {
                sQueryLine += "-";
                sRefLine += "-";
                sMatchLine += " ";
            }
            *pOut << sQueryLine << "\tQuery\n";
            *pOut << sMatchLine << "\n";
            *pOut << sRefLine << "\tReference\n\n";
        }// score xGuard
    }// for

    return std::shared_ptr<Container>(new Nil());
}// function

ContainerVector SeedSetFileWriter::getInputType() const
{
    return ContainerVector{
        // the harmonized strip of consideration
        std::make_shared<ContainerVector>( std::make_shared<Seeds>() ),
        std::make_shared<Pack>(),
    };
}// function

std::shared_ptr<Container> SeedSetFileWriter::getOutputType() const
{
    return std::shared_ptr<Container>(new Nil());
}// function

std::shared_ptr<Container> SeedSetFileWriter::execute(std::shared_ptr<ContainerVector> vpInput)
{
    auto pSoCs = std::dynamic_pointer_cast<ContainerVector>((*vpInput)[0]); // dc
    const auto& pPack = std::dynamic_pointer_cast<Pack>((*vpInput)[1]); // dc
    std::string sPrimary = "true";

    for(std::shared_ptr<Container> pS : *pSoCs)
    {
        const auto& pSeeds = std::dynamic_pointer_cast<Seeds>(pS); // dc
        pSeeds->mem_score = 0;
        for(const auto& rSeed : *pSeeds)
            pSeeds->mem_score += rSeed.getValue();
    }// for

    //sort the harmonized SoCs
    //sort descending
    std::sort(
        pSoCs->begin(), pSoCs->end(),
        []
        (std::shared_ptr<Container>& a_, std::shared_ptr<Container>& b_)
        {
            const auto& a = std::dynamic_pointer_cast<Seeds>(a_); // dc
            const auto& b = std::dynamic_pointer_cast<Seeds>(b_); // dc
            return a->mem_score > b->mem_score;
        }//lambda
    );//sort function call
    assert( pSoCs->size() <= 1 ||  !  pSoCs->back()->larger(pSoCs->front()) );

    for(std::shared_ptr<Container> pS : *pSoCs)
    {
        const auto& pSeeds = std::dynamic_pointer_cast<Seeds>(pS); // dc
        
        // sanity checks
        if(pSeeds == nullptr)
            continue;
        if(pSeeds->empty())
            continue;

        DEBUG_2(
            std::cout << "seedlist: (start_ref, end_ref; start_query, end_query)" << std::endl;
            for(Seed& rSeed : *pSeeds)
            {
                std::cout << rSeed.start_ref() << ", " << rSeed.end_ref() << "; "
                    << rSeed.start() << ", " << rSeed.end() << std::endl;
            }// for
        )// DEBUG

        // Determine the query and reverence coverage of the seeds
        nucSeqIndex beginRef = pSeeds->front().start_ref();
        nucSeqIndex endRef = pSeeds->back().end_ref();
        // seeds are sorted by ther startpos so we 
        // actually need to check all seeds to get the proper end
        nucSeqIndex endQuery = pSeeds->back().end();
        nucSeqIndex beginQuery = pSeeds->front().start();
        nucSeqIndex uiAccSeedLength = 0;
        nucSeqIndex uiNumSeeds = 0;
        for (auto xSeed : *pSeeds)
        {
            if(endRef < xSeed.end_ref())
                endRef = xSeed.end_ref();
            if(beginRef > xSeed.start_ref())
                beginRef = xSeed.start_ref();
            if(endQuery < xSeed.end())
                endQuery = xSeed.end();
            if(beginQuery > xSeed.end())
                beginQuery = xSeed.start();
            uiAccSeedLength += xSeed.size();
            uiNumSeeds++;
            assert(xSeed.start() <= xSeed.end());
        }// for
        DEBUG_2(
            std::cout << beginRef << ", " << endRef << "; " << beginQuery << ", " << endQuery << std::endl;
        )// DEEBUG
        
        std::string sRefName = pPack->nameOfSequenceForPosition(beginRef);
        // 0 based index... 
        std::string sRefPos = std::to_string(pPack->posInSequence(beginRef, endRef) + 1);
        std::string sRefLen = std::to_string(endRef - beginRef);
        std::string sQueryPos = std::to_string(beginQuery);
        std::string sQueryLen = std::to_string(endQuery - beginQuery);
        std::string sAccSeedLength = std::to_string(uiAccSeedLength);
        std::string sNumSeeds = std::to_string(uiNumSeeds);
        std::string sOnRevComp = (pPack->bPositionIsOnReversStrand(beginRef) ? "true" : "false");

        {// synchronize output
            std::lock_guard<std::mutex> xGuard(*pLock);
            *pOut << pSeeds->xStats.sName << "\t";
            *pOut << sQueryPos << "\t";
            *pOut << sQueryLen << "\t";

            *pOut << sRefName << "\t";
            *pOut << sRefPos << "\t";
            *pOut << sRefLen << "\t";

            *pOut << sPrimary << "\t";
            *pOut << sOnRevComp << "\t";
            *pOut << sAccSeedLength << "\t";
            *pOut << sNumSeeds << "\n";
        }// score xGuard

        sPrimary = "false";
    }// for

    return std::shared_ptr<Container>(new Nil());
}// function

#ifdef WITH_PYTHON
void exportFileWriter()
{
    //export the FileWriter class
    boost::python::class_<
            FileWriter, 
            boost::python::bases<Module>, 
            std::shared_ptr<FileWriter>
        >("FileWriter", boost::python::init<std::string, std::shared_ptr<Pack>>())
    ;

    boost::python::implicitly_convertible< 
        std::shared_ptr<FileWriter>,
        std::shared_ptr<Module> 
    >();

}//function
#endif