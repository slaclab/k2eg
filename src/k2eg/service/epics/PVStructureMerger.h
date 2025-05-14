#ifndef K2EG_SERVICE_EPICS_PVSTRUCTUREMERGER_H_
#define K2EG_SERVICE_EPICS_PVSTRUCTUREMERGER_H_

#include <k2eg/common/types.h>

#include <pvData.h>
#include <pvIntrospect.h>

#include <vector>
namespace k2eg::service::epics_impl {

// Manage the merge of a structure and permit to copy value from two structure
// searching for every field that match same name and type
class PVStructureMerger
{
    epics::pvData::FieldBuilderPtr root_builder;
    void copyStructure(epics::pvData::FieldBuilderPtr builder, const epics::pvData::PVStructure* struct_ptr);
    void copyValue(epics::pvData::PVStructure* dest_structure, const epics::pvData::PVStructure* src_structure);

public:
    PVStructureMerger();
    ~PVStructureMerger() = default;
    epics::pvData::PVStructure::const_shared_pointer mergeStructureAndValue(std::vector<epics::pvData::PVStructure::const_shared_pointer> struct_ptr_vec);
    void appendFieldFromStruct(std::vector<epics::pvData::PVStructure::const_shared_pointer> struct_ptr_vec, bool reset = false);
    epics::pvData::PVStructure::const_shared_pointer copyValue(std::vector<epics::pvData::PVStructure::const_shared_pointer> struct_ptr_vec);
    epics::pvData::PVStructure::const_shared_pointer getStructure() const;
};
DEFINE_PTR_TYPES(PVStructureMerger)

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_PVSTRUCTUREMERGER_H_