#ifndef FILE_READER_H
#define FILE_READER_H

#include "module/module.h"
#include "container/nucSeq.h"
#include "container/alignment.h"
#include "container/pack.h"
#include <fstream>

namespace libMABS
{

    class FileWriter: public Module
    {
    public:
        std::shared_ptr<std::ostream> pFile;

        FileWriter(std::string sFileName)
        {
            if(sFileName == "stdout")
                pFile = std::shared_ptr<std::ostream>(&std::cout);
            else
                pFile = std::shared_ptr<std::ostream>(
                new std::ofstream(sFileName, std::ofstream::out | std::ofstream::trunc));
            if (!pFile->good())
            {
                std::cout << "Unable to open file" << std::endl;
                //@todo exception
            }//if
            *pFile << "@HD VN:1.5 SO:unknown" << std::endl;
        }//constructor

        ~FileWriter()
        {
            if(std::static_pointer_cast<std::ofstream>(pFile) != nullptr)
                std::static_pointer_cast<std::ofstream>(pFile)->close();
        }//deconstructor

        std::shared_ptr<Container> execute(std::shared_ptr<ContainerVector> vpInput);

        /**
         * @brief Used to check the input of execute.
         * @details
         * Returns:
         * - Nil
         */
        ContainerVector getInputType() const;

        /**
         * @brief Used to check the output of execute.
         * @details
         * Returns:
         * - ContainerVector(NucSeq)
         */
        std::shared_ptr<Container> getOutputType() const;

        std::string getName() const
        {
            return "FileReader";
        }//function

    };//class

}//namespace

void exportFileReader();

#endif