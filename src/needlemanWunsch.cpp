#include "needlemanWunsch.h"




std::vector<ContainerType> NeedlemanWunsch::getInputType()
{
    return std::vector<ContainerType>{
        //the sound strip of consideration
        ContainerType::stripOfConsideration,
        //the query sequence
        ContainerType:nucSeq,
        //the reference sequence
        ContainerType:packedNucSeq,
    };
}//function

std::vector<ContainerType> NeedlemanWunsch::getOutputType()
{
    return std::vector<ContainerType>{ContainerType::alignment};
}//function

int iDeletion = -50;
int iInsertion = -50;
int iDeletionContinued = -1;
int iInsertionContinued = -1;
int iMatch = 20;
int iMissMatch = -5;

void needlemanWunsch(
        std::shared_ptr<NucleotideSequence> pQuery, 
        std::shared_ptr<NucleotideSequence> pRef,
        nucSeqIndex fromQuery,
        nucSeqIndex toQuery,
        nucSeqIndex fromRef,
        nucSeqIndex toRef,
        std::shared_ptr<Alignment> pAlignment
    )
{
    if(toRef <= fromRef)
        if(toQuery <= fromQuery)
            return;
    DEBUG_2(
        std::cout << toQuery-fromQuery << std::endl;
        for(nucSeqIndex i = fromQuery; i < toQuery; i++)
            std::cout << pQuery->charAt(i);
        std::cout << std::endl;
        std::cout << toRef-fromRef << std::endl;
        for(nucSeqIndex i = fromRef; i < toRef; i++)
            std::cout << pRef->charAt(i);
        std::cout << std::endl;
    )//DEBUG
    if(toQuery <= fromQuery)
    {
        int iY = toRef-fromRef;
        while(iY > 0)
        {
            pAlignment->append(Alignment::MatchType::deletion);
            DEBUG_2(
                std::cout << "D";
            )//DEBUG
            iY--;
        }//while
        return;
    }//if
    if(toRef <= fromRef)
    {
        int iX = toQuery-fromQuery;
        while(iX > 0)
        {
            pAlignment->append(Alignment::MatchType::insertion);
            DEBUG_2(  
                std::cout << "I";
            )//DEBUG
            iX--;
        }//while
        return;
    }//if
    int s[toQuery-fromQuery+1][toRef-fromRef+1];
    char dir[toQuery-fromQuery+1][toRef-fromRef+1];//1=match; 2=ins; 3=del
    s[0][0] = 0;
    dir[0][0] = 1;
    s[1][0] = iInsertion;
    dir[1][0] = 2;
    s[0][1] = iDeletion;
    dir[0][1] = 3;
    for(nucSeqIndex uiI = 2; uiI < toQuery-fromQuery+1; uiI++)
    {
        s[uiI][0] = s[uiI - 1][0] + iInsertionContinued;
        dir[uiI][0] = 2;
    }//for
    for(nucSeqIndex uiI = 2; uiI < toRef-fromRef+1; uiI++)
    {
        s[0][uiI] = s[0][uiI - 1] + iDeletionContinued;
        dir[0][uiI] = 3;
    }//for
    for(nucSeqIndex uiI = 1; uiI < toQuery-fromQuery+1; uiI++)
    {
        for(nucSeqIndex uiJ = 1; uiJ < toRef-fromRef+1; uiJ++)
        {
            int newScore;
            //insertion
            if(dir[uiI - 1][uiJ] == 2)
                newScore = s[uiI - 1][uiJ] + iInsertionContinued;
            else
                newScore = s[uiI - 1][uiJ] + iInsertion;
            s[uiI][uiJ] = newScore;
            dir[uiI][uiJ] = 2;

            //deletion
            if(dir[uiI][uiJ - 1] == 3)
                newScore = s[uiI][uiJ - 1] + iDeletionContinued;
            else
                newScore = s[uiI][uiJ - 1] + iDeletion;
            // for the first alignment we dont want to have a malus for an
            // deletion of the reference at the beginning
            if(fromQuery == 0 && uiI == toQuery-fromQuery)
                newScore = s[uiI][uiJ - 1];
            if(newScore > s[uiI][uiJ])
            {
                s[uiI][uiJ] = newScore;
                dir[uiI][uiJ] = 3;
            }//if
            //match / missmatch
            newScore = s[uiI - 1][uiJ - 1];
            if( (*pQuery)[toQuery - uiI] == (*pRef)[toRef - uiJ] )
                newScore += iMatch;
            else
                newScore += iMissMatch;
            if(newScore >= s[uiI][uiJ])
            {
                s[uiI][uiJ] = newScore;
                dir[uiI][uiJ] = 1;
            }//if
        }//for
    }//for

    DEBUG_3(
        for(nucSeqIndex uiI = 0; uiI < toRef-fromRef+1; uiI++)
        {
            if(uiI == 0)
                std::cout << " \t \t";
            else
                std::cout << pRef->charAt(toRef - uiI) << "\t";
        }//for
        std::cout << std::endl;
        for(nucSeqIndex uiI = 0; uiI < toQuery-fromQuery+1; uiI++)
        {
            if(uiI == 0)
                std::cout << " \t";
            else
                std::cout << pQuery->charAt(toQuery - uiI) << "\t";
            for(nucSeqIndex uiJ = 0; uiJ < toRef-fromRef+1; uiJ++)
                std::cout << s[uiI][uiJ] << "\t";
            std::cout << std::endl;
        }//for
    )//DEBUG

    nucSeqIndex iX = toQuery-fromQuery;
    nucSeqIndex iY = toRef-fromRef;
    while(iX > 0 || iY > 0)
    {
        if(dir[iX][iY] == 1)
        {
            if( (*pQuery)[toQuery - iX] == (*pRef)[toRef - iY] )
            {
                pAlignment->append(Alignment::MatchType::match);
                DEBUG_2(
                    std::cout << "M";
                )//DEBUG
            }
            else
            {
                pAlignment->append(Alignment::MatchType::missmatch);
                DEBUG_2(
                    std::cout << "W";
                )//DEBUG
            }
            iX--;
            iY--;
        }//if
        else if(dir[iX][iY] == 3)
        {
            pAlignment->append(Alignment::MatchType::deletion);
            iY--;
            DEBUG_2(
                std::cout << "D";
            )//DEBUG        
        }//if
        else
        {
            pAlignment->append(Alignment::MatchType::insertion);
            iX--;
            DEBUG_2(
                std::cout << "I";
            )//DEBUG
        }//if
    }//while
    DEBUG_2(
        std::cout << std::endl;
    )//DEBUG
}//function

std::shared_ptr<Container> NeedlemanWunsch::execute(
        std::vector<std::shared_ptr<Container>> vpInput
    )
{
    std::shared_ptr<StripOfConsideration> pStrip = std::static_pointer_cast<StripOfConsideration>(vpInput[0]);
    std::shared_ptr<NucleotideSequence> pQuery 
        = std::static_pointer_cast<NucleotideSequence>(vpInput[1]);
    std::shared_ptr<BWACompatiblePackedNucleotideSequencesCollection> pRefPack = 
        std::static_pointer_cast<BWACompatiblePackedNucleotideSequencesCollection>(vpInput[2]);

    std::list<Seed>* pSeeds = &pStrip->seeds();

    //sort shadows (increasingly) by start coordinate of the match
    pSeeds->sort(
            [](Seed xA, Seed xB)
            {
                if(xA.start() == xB.start())
                    return xA.start_ref() < xB.start_ref();
                return xA.start() < xB.start();
            }//lambda
        );//sort function call

    nucSeqIndex beginQuery = pSeeds->front().start();
    nucSeqIndex beginRef = 0;
    if( pSeeds->front().start_ref() >  beginQuery*2)
        beginRef = pSeeds->front().start_ref() - beginQuery*2;
    nucSeqIndex endQuery = pSeeds->back().end();
    //TODO: can only do forward hits so far...
    nucSeqIndex endRef = pRefPack->uiUnpackedSizeForwardPlusReverse()/2;
    if( 
            pSeeds->back().end_ref() + (pQuery->length()-endQuery)*2 <
            pRefPack->uiUnpackedSizeForwardPlusReverse()/2
        )
        endRef = pSeeds->back().end_ref() + (pQuery->length()-endQuery)*2;

    std::shared_ptr<Alignment> pRet(new Alignment(beginRef, endRef));

    std::shared_ptr<NucleotideSequence> pRef = pRefPack->vExtract(beginRef, endRef);

    DEBUG_2(
        std::cout << "seedlist: (start_ref, end_ref; start_query, end_query)" << std::endl;
        for(Seed& rSeed : *pSeeds)
        {
            std::cout << rSeed.start_ref() << ", " << rSeed.end_ref() << "; "
                << rSeed.start() << ", " << rSeed.end() << std::endl;
        }//for
    )

    nucSeqIndex endOfLastSeedQuery = 0;
    nucSeqIndex endOfLastSeedReference = 0;

    for(Seed& rSeed : *pSeeds)
    {
        needlemanWunsch(
                pQuery,
                pRef,
                endOfLastSeedQuery,
                rSeed.start(),
                endOfLastSeedReference,
                rSeed.start_ref() - beginRef,
                pRet
            );
        nucSeqIndex ovQ = endOfLastSeedQuery - rSeed.start();
        if(rSeed.start() > endOfLastSeedQuery)
            ovQ = 0;
        nucSeqIndex ovR = endOfLastSeedReference - (rSeed.start_ref() - beginRef);
        if(rSeed.start_ref() > endOfLastSeedReference + beginRef)
            ovR = 0;
        nucSeqIndex len = rSeed.size();
        nucSeqIndex overlap = std::max(ovQ, ovR);
        DEBUG(
            std::cout << "overlap: " << overlap << std::endl;
        )//DEBUG
        if(len > overlap)
        {
            pRet->append(Alignment::MatchType::match, len - overlap);
            DEBUG_2(
                std::cout << len - overlap << std::endl;
            )//DEBUG_2
            DEBUG(
                for(nucSeqIndex i = overlap; i < len; i++)
                    std::cout << pQuery->charAt(i + rSeed.start());
                std::cout << std::endl;
                for(nucSeqIndex i = overlap; i < len; i++)
                    std::cout << pRef->charAt(i + rSeed.start_ref() - beginRef);
                std::cout << std::endl;
            )//DEBUG
            DEBUG_2(
                for(nucSeqIndex i = 0; i < len - overlap; i++)
                    std::cout << "m";
            )//DEBUG_2
        }//if
        if(ovQ > ovR)
            pRet->append(Alignment::MatchType::deletion, ovQ - ovR);
        DEBUG_2(
            for(nucSeqIndex i = ovR; i < ovQ; i++)
                std::cout << "d";
        )
        if(ovR > ovQ)
            pRet->append(Alignment::MatchType::insertion, ovR - ovQ);
        DEBUG_2(
            for(nucSeqIndex i = ovQ; i < ovR; i++)
                std::cout << "i";
            std::cout << std::endl;
        )//DEBUG
        if(rSeed.end() > endOfLastSeedQuery)
            endOfLastSeedQuery = rSeed.end();
        if(rSeed.end_ref() > endOfLastSeedReference + beginRef)
            endOfLastSeedReference = rSeed.end_ref() - beginRef;
    }//for

    needlemanWunsch(
        pQuery,
        pRef,
        endOfLastSeedQuery,
        pQuery->length(),
        endOfLastSeedReference,
        endRef - beginRef,
        pRet
    );

    DEBUG_2(
        std::cout << std::endl;
    )
    return pRet;

}//function

void exportNeedlemanWunsch()
{
     //export the segmentation class
    boost::python::class_<
        NeedlemanWunsch, 
        boost::python::bases<Module>
    >(
        "NeedlemanWunsch", 
        "Picks a set of anchors for the strips of consideration.\n"
        "\n"
        "Execution:\n"
        "   Expects seg_list, query, ref\n"
        "       seg_list: the list of segments to pick the anchors from\n"
        "       query: the query as NucSeq\n"
        "       ref: the reference as Pack\n"
        "   returns alignment.\n"
        "       alignment: the final alignment\n"
    );

}//function