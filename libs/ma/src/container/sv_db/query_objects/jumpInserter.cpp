#include "container/sv_db/query_objects/jumpInserter.h"

using namespace libMA;

#ifdef WITH_PYTHON

#include "container/sv_db/py_db_conf.h"

void exportSvJumpInserter( py::module& rxPyModuleId )
{
    exportInserterContainer<GetJumpInserterContainerModule<DBCon, DBConSingle>>(rxPyModuleId, "JumpInserter");

    exportModule<JumpInserterModule>(rxPyModuleId, "JumpInserterModule");
} // function

#endif
