#include "container/sv_db/query_objects/readInserter.h"

using namespace libMA;

#ifdef WITH_PYTHON

#include "container/sv_db/py_db_conf.h"


void exportReadInserter( py::module& rxPyModuleId )
{
    exportInserterContainer<GetReadInserterContainerModule<DBCon, DBConSingle>>( rxPyModuleId, "ReadInserter" );
    exportInserterContainer<GetPairedReadInserterContainerModule<DBCon, DBConSingle>>( rxPyModuleId,
                                                                                       "PairedReadInserter" );

    exportModule<ReadInserterModule>( rxPyModuleId, "ReadInserterModule" );
    exportModule<PairedReadInserterModule>( rxPyModuleId, "PairedReadInserterModule" );
} // function

#endif