#ifndef SOC_H
#define SOC_H

#include "container/seed.h"
#include <algorithm>

namespace libMA
{
    /**
     * @brief used to determine more complex orders of SoCs 
     */
    class SoCOrder
    {
    public:
        nucSeqIndex uiAccumulativeLength = 0;
        unsigned int uiSeedAmbiguity = 0;
        unsigned int uiSeedAmount = 0;

        inline void operator+=(const Seed& rS)
        {
            uiSeedAmbiguity += rS.uiAmbiguity;
            uiSeedAmount++;
            uiAccumulativeLength += rS.getValue();
        }//operator

        inline void operator-=(const Seed& rS)
        {
            assert(uiSeedAmbiguity >= rS.uiAmbiguity);
            uiSeedAmbiguity -= rS.uiAmbiguity;
            assert(uiAccumulativeLength >= rS.getValue());
            uiAccumulativeLength -= rS.getValue();
            uiSeedAmount--;
        }//operator

        inline bool operator<(const SoCOrder& rOther) const
        {
            if(uiAccumulativeLength == rOther.uiAccumulativeLength)
                return uiSeedAmbiguity > rOther.uiSeedAmbiguity;
            return uiAccumulativeLength < rOther.uiAccumulativeLength;
        }//operator

        inline void operator=(const SoCOrder& rOther)
        {
            uiAccumulativeLength = rOther.uiAccumulativeLength;
            uiSeedAmbiguity = rOther.uiSeedAmbiguity;
        }//operator
    }; //class


    class SoCPriorityQueue: public Container
    {
    public:
#if DEBUG_LEVEL >= 1
            // confirms the respective functions are always called in the correct mode
            bool bInPriorityMode = false;
            std::vector<std::pair<nucSeqIndex, nucSeqIndex>> vScores;
            class blub{
            public:
                nucSeqIndex first = 0, second = 0, qCoverage = 0, rStart = 0, rEnd = 0, rStartSoC = 0, rEndSoC = 0;
                inline void operator=(const blub& rOther)
                {
                    first = rOther.first;
                    second = rOther.second;
                    qCoverage = rOther.qCoverage;
                    rStart = rOther.rStart;
                    rEnd = rOther.rEnd;
                    rStartSoC = rOther.rStartSoC;
                    rEndSoC = rOther.rEndSoC;
                }//operator
                inline bool operator==(const blub& rOther) const
                {
                    return
                        first == rOther.first &&
                        second == rOther.second &&
                        qCoverage == rOther.qCoverage &&
                        rStart == rOther.rStart &&
                        rEnd == rOther.rEnd &&
                        rStartSoC == rOther.rStartSoC &&
                        rEndSoC == rOther.rEndSoC;
                }//operator
            };
            std::vector<blub> vExtractOrder;
            std::vector<std::shared_ptr<Seeds>> vSoCs;
            std::vector<std::shared_ptr<Seeds>> vHarmSoCs;
            std::vector<double> vSlopes;
            std::vector<double> vIntercepts;
            std::vector<std::shared_ptr<Seeds>> vIngroup;
#endif
        unsigned int uiSoCIndex = 0;
        const nucSeqIndex uiStripSize;
        std::shared_ptr<std::vector<Seed>> pSeeds;
        // positions to remember the maxima
        std::vector<std::tuple<SoCOrder, std::vector<Seed>::iterator, std::vector<Seed>::iterator>> vMaxima;
        // last SoC end
        nucSeqIndex uiLastEnd = 0;
        static bool heapOrder(
           const std::tuple<SoCOrder, std::vector<Seed>::iterator, std::vector<Seed>::iterator>& rA,
           const std::tuple<SoCOrder, std::vector<Seed>::iterator, std::vector<Seed>::iterator>& rB
        )
        {
            return std::get<0>(rA) < std::get<0>(rB);
        }// function

        SoCPriorityQueue(nucSeqIndex uiStripSize, std::shared_ptr<std::vector<Seed>> pSeeds)
                :
            uiStripSize(uiStripSize),
            pSeeds(pSeeds),
            vMaxima()
        {}//constructor

        SoCPriorityQueue()
                :
            uiStripSize(0),
            pSeeds(nullptr),
            vMaxima()
        {}//constructor


        //overload
        inline bool canCast(const std::shared_ptr<Container>& c) const
        {
            return std::dynamic_pointer_cast<SoCPriorityQueue>(c) != nullptr;
        }//function

        //overload
        inline std::string getTypeName() const
        {
            return "SoCPriorityQueue";
        }//function

        //overload
        inline std::shared_ptr<Container> getType() const
        {
            return std::shared_ptr<Container>(new SoCPriorityQueue());
        }//function

        inline bool empty() const
        {
            return vMaxima.empty();
        }// function

        inline std::shared_ptr<Seeds> pop()
        {
            DEBUG(
                assert(!empty());
                assert(bInPriorityMode);
            )//DEBUG
            //the strip that shall be collected
            std::shared_ptr<Seeds> pRet(new Seeds());
            // get the expected amount of seeds in the SoC from the order class and reserve memory
            pRet->reserve(std::get<0>(vMaxima.front()).uiSeedAmount);
            //iterator walking till the end of the strip that shall be collected
            auto xCollect2 = std::get<1>(vMaxima.front());
            assert(xCollect2 != pSeeds->end());
            //save SoC index
            pRet->xStats.index_of_strip = uiSoCIndex++;
            // all these things are not used at the moment...
            pRet->xStats.uiInitialQueryBegin = xCollect2->start();
            pRet->xStats.uiInitialRefBegin = xCollect2->start_ref();
            pRet->xStats.uiInitialQueryEnd = xCollect2->end();
            pRet->xStats.uiInitialRefEnd = xCollect2->end_ref();
            auto xCollectEnd = std::get<2>(vMaxima.front());
            while(
                xCollect2 != pSeeds->end() &&
                xCollect2 != xCollectEnd
            )
            {
                // save the beginning and end of the SoC
                // all these things are not used at the moment...
                if(xCollect2->start() < pRet->xStats.uiInitialQueryBegin)
                    pRet->xStats.uiInitialQueryBegin = xCollect2->start();
                if(xCollect2->start_ref() < pRet->xStats.uiInitialRefBegin)
                    pRet->xStats.uiInitialRefBegin = xCollect2->start_ref();
                if(xCollect2->end() > pRet->xStats.uiInitialQueryEnd)
                    pRet->xStats.uiInitialQueryEnd = xCollect2->end();
                if(xCollect2->end_ref() > pRet->xStats.uiInitialRefEnd)
                    pRet->xStats.uiInitialRefEnd = xCollect2->end_ref();
                pRet->xStats.num_seeds_in_strip++;
                assert(xCollect2->start() <= xCollect2->end());
                //if the iterator is still within the strip add the seed and increment the iterator
                pRet->push_back(*(xCollect2++));
            }//while

            DEBUG(vSoCs.push_back(pRet);)


            //move to the next strip
            std::pop_heap (vMaxima.begin(), vMaxima.end(), heapOrder); vMaxima.pop_back();

            return pRet;
        }// function

        inline void push_back_no_overlap(
                const SoCOrder& rCurrScore, 
                const std::vector<Seed>::iterator itStrip,
                const std::vector<Seed>::iterator itStripEnd, // points to one element past end
                const nucSeqIndex uiCurrStart,
                const nucSeqIndex uiCurrEnd
            )
        {
            DEBUG(assert(!bInPriorityMode);)
            if(
                vMaxima.empty() || 
                uiLastEnd < uiCurrStart ||
                std::get<0>(vMaxima.back()) < rCurrScore)
            {
                // if we reach this point we want to save the current SoC
                if( !vMaxima.empty() && uiLastEnd >= uiCurrStart)
                    // the new and the last SoC overlap and the new one has a higher score
                    // so we want to replace the last SoC
                    vMaxima.pop_back();
                // else the new and the last SoC do not overlapp
                // so we want to save the new SOC
                vMaxima.push_back(std::make_tuple(rCurrScore, itStrip, itStripEnd));
                // we need to remember the end position of the new SoC
                uiLastEnd = uiCurrEnd;
            }// if
            // else new and last SoC overlap and the new one has a lower score => ignore the new one
        }// function

        inline void make_heap()
        {
            DEBUG(assert(!bInPriorityMode);bInPriorityMode = true;)
            // make a max heap from the SOC starting points according to the scores, 
            // so that we can extract the best SOC first
            std::make_heap(vMaxima.begin(), vMaxima.end(), heapOrder);
        }// function
    }; //class
}// namspace libMA


void exportSoC();

#endif