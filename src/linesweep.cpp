#include "linesweep.h"

std::vector<ContainerType> LineSweep::getInputType()
{
	return std::vector<ContainerType>{
			//the querry
			ContainerType::nucSeq,
			//the reference
			ContainerType::packedNucSeq,
			//the stips of consideration
			ContainerType::stripOfConsideration,
		};
}//function

std::vector<ContainerType> LineSweep::getOutputType()
{
	return std::vector<ContainerType>{ContainerType::stripOfConsideration};
}//function

/**
 * determine the start and end positions this match casts on the left border of the given bucket
 * pxMatch is the container this match is stored in.
 */
ShadowInterval LineSweep::getLeftShadow(
        nucSeqIndex uiBucketStart,
        std::list<Seed>::iterator pSeed,
        nucSeqIndex uiQueryLength
    ) const
{
    return ShadowInterval(
            pSeed->start() /*+ uiBucketStart*/,
            pSeed->end_ref() - pSeed->start() + uiQueryLength,
            pSeed
        );
}//function

/**
 * determine the start and end positions this match casts on the right border of the given bucket
 * pxMatch is the container this match is stored in.
 */
ShadowInterval LineSweep::getRightShadow(
        nucSeqIndex iBucketStart,
        std::list<Seed>::iterator pSeed,
        nucSeqIndex iRefSize
    ) const
{
    return ShadowInterval(
            pSeed->start_ref(),
            pSeed->end() - pSeed->start_ref() + iRefSize,
            pSeed
        );
}//function

void LineSweep::linesweep(
        std::vector<ShadowInterval>& vShadows, 
        std::list<Seed>& rSeeds, 
        std::shared_ptr<StripOfConsideration> pStrip
    )
{
    //sort shadows (increasingly) by start coordinate of the match
    std::sort(
            vShadows.begin(),
            vShadows.end(),
            [](ShadowInterval xA, ShadowInterval xB)
            {
                /*
                * sort by the interval starts
                * if two intervals start at the same point the larger one shall be treated first
                */
                if(xA.start() == xB.start())
                    return xA.end() > xB.end();
                return xA.start() < xB.start();
            }//lambda
        );//sort function call

    //records the interval ends
    SelfBalancingBinarySearchTree<ShadowInterval> xItervalEnds =
        SelfBalancingBinarySearchTree<ShadowInterval>();

    //this is the line sweeping part
    for(ShadowInterval& rInterval : vShadows)
    {
        //remove the stack markers
        while(!xItervalEnds.isEmpty())
        {

            ShadowInterval* pFirstEnding = xItervalEnds.first();
            //check if we really need to remove the first interval in the tree
            if(pFirstEnding->end() > rInterval.start())
                break;

            DEBUG(
                std::cout << "Current Sweep position: " << pFirstEnding->end() << std::endl;
                std::cout << "\tend of interval " << pFirstEnding->start() << ", "
                    << pFirstEnding->end() << std::endl;
            )

            //when reaching here we actually have to remove the intervall
            pFirstEnding->removeSeedIfNecessary(rSeeds, pStrip);
            xItervalEnds.deleteFirst();
        }//while

        DEBUG(
            std::cout << "Current Sweep position: " << rInterval.start() << std::endl;
            std::cout << "\tstart of interval " << rInterval.start() <<
                ", " << rInterval.end() << std::endl;
            std::cout << "\t(start_ref, end_ref; start_query, end_query) " 
                << rInterval->start_ref() << ", " << rInterval->end_ref() << "; "
                << rInterval->start() << ", " << rInterval->end() << std::endl;
        )

        //work on the current interval
        ShadowInterval* pNextShadow = xItervalEnds.insert(&rInterval);
        if(pNextShadow != nullptr)
            pNextShadow->addInterferingInterval(rInterval);
        DEBUG(
            else
                std::cout << "\tno interference" << std::endl;
        )
    }//for
    
    //remove the stack markers
    while(!xItervalEnds.isEmpty())
    {

        ShadowInterval* pFirstEnding = xItervalEnds.first();

        DEBUG(
            std::cout << "Current Sweep position: " << pFirstEnding->end() << std::endl;
            std::cout << "\tend of interval (cleanup) " << pFirstEnding->start() << ", " 
                << pFirstEnding->end() << std::endl;
        )

        //when reaching here we actually have to remove the intervall
        pFirstEnding->removeSeedIfNecessary(rSeeds, pStrip);
        xItervalEnds.deleteFirst();
    }//while

}//function

std::shared_ptr<Container> LineSweep::execute(
        std::vector<std::shared_ptr<Container>> vpInput
    )
{
    std::shared_ptr<NucleotideSequence> pQuerrySeq =
        std::static_pointer_cast<NucleotideSequence>(vpInput[0]);
    std::shared_ptr<BWACompatiblePackedNucleotideSequencesCollection> pRefSeq =
        std::static_pointer_cast<BWACompatiblePackedNucleotideSequencesCollection>(vpInput[1]);
    std::shared_ptr<StripOfConsideration> pStrip =
        std::static_pointer_cast<StripOfConsideration>(vpInput[2]);

    std::vector<ShadowInterval> vShadows = {};

    //get the left shadows
    pStrip->forAllSeeds(
        [&](std::list<Seed>::iterator pSeed)
        {
            vShadows.push_back(getLeftShadow(
                    pStrip->start(),
                    pSeed,
                    pQuerrySeq->length()
                ));
        }//lambda
    );

    //perform the line sweep algorithm on the left shadows
    linesweep(vShadows, pStrip->seeds(), pStrip);
    
    vShadows.clear();
    
    //get the right shadows
    pStrip->forAllSeeds(
        [&](std::list<Seed>::iterator pSeed)
        {
            vShadows.push_back(getRightShadow(
                    pStrip->start(),
                    pSeed,
                    pRefSeq->uiUnpackedSizeForwardPlusReverse()
                ));
        }//lambda
    );

    //perform the line sweep algorithm on the right shadows
    linesweep(vShadows, pStrip->seeds(), pStrip);

    return pStrip;
}//function

void exportLinesweep()
{
    //export the LineSweepContainer class
	boost::python::class_<LineSweep, boost::python::bases<Module>>(
        "LineSweep",
        "Uses linesweeping to remove contradicting "
        "matches within one strip of consideration.\n"
        "\n"
        "Execution:\n"
        "	Expects querry, ref, strip_vec as input.\n"
        "		querry: the querry as NucleotideSequence\n"
        "		ref: the reference seqeuence as Pack\n"
        "		strip_vec: the areas that shall be evaluated as StripOfConsiderationVector\n"
        "	returns strip_vec.\n"
        "		strip_vec: the evaluated areas\n"
    );
}//function