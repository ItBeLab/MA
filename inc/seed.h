/** 
 * @file seed.h
 * @brief Implements Seed.
 * @author Markus Schmidt
 */
#ifndef SEED_H
#define SEED_H

#include "container.h"
#include "interval.h"
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <list>

///@brief any index on the query or reference nucleotide sequence is given in this datatype
typedef uint64_t nucSeqIndex;

/**
 * @brief A seed.
 * @details
 * A extracted seed, that comprises two intervals, one on the query one on the reference.
 * Both intervals are equal in size.
 * @note the overloaded functions of Interval refer to the Interval on the query.
 * @ingroup container
 */
class Seed: public Container, public Interval<nucSeqIndex>
{
private:
    ///@brief the beginning of the match on the reference
    nucSeqIndex uiPosOnReference;

public:

    /**
     * @brief Creates a new Seed.
     */
    Seed(
            const nucSeqIndex uiPosOnQuery, 
            const nucSeqIndex uiLenght, 
            const nucSeqIndex uiPosOnReference
        )
            :
        Interval(uiPosOnQuery, uiLenght),
        uiPosOnReference(uiPosOnReference)
    {}//constructor
    
    /**
     * @brief Returns the beginning of the seed on the reference.
     */
    nucSeqIndex start_ref() const
    {
        return uiPosOnReference;
    }//function
    
    /**
     * @brief Returns the end of the seed on the reference.
     */
    nucSeqIndex end_ref() const
    {
        return uiPosOnReference + size();
    }//function
    
    /**
     * @brief Returns the value of the seed.
     * @details
     * A seeds value corresponds to its size.
     */
    nucSeqIndex getValue() const
    {
        return size();
    }//function

    /**
     * @brief Copys from another Seed.
     */
    inline Seed& operator=(const Seed& rxOther)
    {
        Interval::operator=(rxOther);
        uiPosOnReference = rxOther.uiPosOnReference;
        return *this;
    }// operator
    
	/*
	 * @brief compares two Seeds.
	 * @returns true if start and size are equal, false otherwise.
	 */
	inline bool operator==(const Seed& rxOther)
	{
		return Interval::operator==(rxOther) && uiPosOnReference == rxOther.uiPosOnReference;
	}// operator

    /*used to identify the Seed datatype in the aligner pipeline*/
    ContainerType getType(){return ContainerType::seed;}
}; //class

/**
 * @brief A list where one element is a Seed.
 * @details
 * Also holds the summed up score of the seeds within the list.
 * @ingroup Container
 */
class Seeds:
public std::list<Seed>,
public Container
{
    public:
    //inherit the constructors from vector
    using list::list;
    //inherit the constructors from Container
    using Container::Container;

    Seeds(std::shared_ptr<Seeds> pOther)
        :
        list(*pOther),
        Container()
    {}//copy constructor

    Seeds()
        :
        list(),
        Container()
    {}//default constructor

    /*used to identify the Seeds datatype in the aligner pipeline*/
    ContainerType getType(){return ContainerType::seeds;}
    /*returns the sum off all scores within the list*/
    nucSeqIndex getScore() const
    {
        nucSeqIndex iRet = 0;
        for(const Seed& rS : *this)
            iRet += rS.getValue();
        return iRet;
    }//function
    /*append a copy of another list*/
    void append(std::shared_ptr<Seeds> pOther)
    {
        for(Seed& rS : *pOther)
            push_back(rS);
    }//function

};//class

/**
 * @brief a std::vector of seeds.
 * @details
 * This class is necessary in order to inherit from Container.
 */
class SeedsVector:
    public std::vector<std::shared_ptr<Seeds>>,
    public Container
{
public:
    //inherit the constructors from vector
    using vector::vector;
    //inherit the constructors from Container
    using Container::Container;
    /*used to identify the SeedsVector datatype in the aligner pipeline*/
    ContainerType getType(){return ContainerType::seedsVector;}
};//class


/**
 * @brief exports the Seed and Seedlist classes to python.
 * @ingroup export
 */
void exportSeed();

#endif