#ifndef K2EG_SERVICE_EPICS_PVSTRUCTUREMERGER_H_
#define K2EG_SERVICE_EPICS_PVSTRUCTUREMERGER_H_

#include <k2eg/common/types.h>

#include <pvData.h>
#include <pvIntrospect.h>

#include <vector>
namespace k2eg::service::epics_impl {

struct PVStructureToMerge
{
    epics::pvData::PVStructure::const_shared_pointer structure;
    const std::vector<std::string>& field_names;
};

// Manage the merge of a structure and permit to copy value from two structure
// searching for every field that match same name and type
class PVStructureMerger
{
    epics::pvData::FieldBuilderPtr root_builder;

    // Helper methods for structure definition copying
    void copyStructure(epics::pvData::FieldBuilderPtr builder, const epics::pvData::PVStructure* struct_ptr);
    void copyStructureDefinition(epics::pvData::FieldBuilderPtr builder, const epics::pvData::Structure* structDef);
    void copyUnionDefinition(epics::pvData::FieldBuilderPtr builder, const epics::pvData::Union* unionDef);
    
    // Helper methods for value copying
    void copyStructureValue(epics::pvData::PVStructure* dest_structure, const epics::pvData::PVStructure* src_structure);
    void copyFieldValue(epics::pvData::PVField* dest_field, const epics::pvData::PVField* src_field);
    
    // Builder helper
    void addFieldToBuilder(epics::pvData::FieldBuilderPtr builder, const std::string& field_name, const epics::pvData::PVField* field);

public:
    PVStructureMerger();
    ~PVStructureMerger() = default;
    
    // Main merge method that combines structures and copies values
    epics::pvData::PVStructure::const_shared_pointer mergeStructureAndValue(const std::vector<PVStructureToMerge>& struct_ptr_vec);
    
    // Legacy methods
    void appendFieldFromStruct(std::vector<epics::pvData::PVStructure::const_shared_pointer> struct_ptr_vec, bool reset = false);
    epics::pvData::PVStructure::const_shared_pointer copyValue(std::vector<epics::pvData::PVStructure::const_shared_pointer> struct_ptr_vec);
    epics::pvData::PVStructure::const_shared_pointer getStructure() const;
};
DEFINE_PTR_TYPES(PVStructureMerger)

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_PVSTRUCTUREMERGER_H_