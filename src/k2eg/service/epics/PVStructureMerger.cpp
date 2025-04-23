#include <k2eg/service/epics/PVStructureMerger.h>

namespace pvd = epics::pvData;
using namespace k2eg::service::epics_impl;

PVStructureMerger::PVStructureMerger() : root_builder(pvd::getFieldCreate()->createFieldBuilder()) {}

epics::pvData::PVStructure::const_shared_pointer PVStructureMerger::mergeStructureAndValue(std::vector<epics::pvData::PVStructure::const_shared_pointer> struct_ptr_vec)
{
    appendFieldFromStruct(struct_ptr_vec, true);
    return copyValue(struct_ptr_vec);
}

void PVStructureMerger::appendFieldFromStruct(std::vector<pvd::PVStructure::const_shared_pointer> struct_ptr_vec, bool reset)
{
    if (!struct_ptr_vec.size())
        return;
    if (reset)
    {
        root_builder = pvd::getFieldCreate()->createFieldBuilder();
    }
    for (auto& s : struct_ptr_vec)
    {
        if (!s)
            continue;
        copyStructure(root_builder, s.get());
    }
}

epics::pvData::PVStructure::const_shared_pointer PVStructureMerger::copyValue(std::vector<epics::pvData::PVStructure::const_shared_pointer> struct_ptr_vec)
{
    auto structure = root_builder->createStructure()->build();
    for (auto& s : struct_ptr_vec)
    {
        if (!s)
            continue;
        copyValue(structure.get(), s.get());
    }
    structure->setImmutable();
    return structure;
}

void PVStructureMerger::copyStructure(epics::pvData::FieldBuilderPtr builder, const epics::pvData::PVStructure* struct_ptr)
{
    const pvd::StructureConstPtr& type = struct_ptr->getStructure();
    const pvd::PVFieldPtrArray&   children = struct_ptr->getPVFields();
    const pvd::StringArray&       names = type->getFieldNames();
    for (size_t i = 0, N = names.size(); i < N; i++)
    {
        auto const& fld = children[i].get();
        auto        type = fld->getField()->getType();
        switch (type)
        {
        case epics::pvData::scalar:
            builder->add(names[i], static_cast<const pvd::PVScalar*>(fld)->getScalar()->getScalarType());
            break;
        case epics::pvData::scalarArray:
            builder->addArray(names[i], static_cast<const pvd::PVScalarArray*>(fld)->getScalarArray()->getElementType());
            break;
        case epics::pvData::structure:
            {
                auto nested_buider = builder->addNestedStructure(names[i]);
                copyStructure(nested_buider, static_cast<const pvd::PVStructure*>(fld));
                nested_buider->endNested();
                break;
            }
        case epics::pvData::structureArray:
            {
                int a = 0;
                break;
            }
        case epics::pvData::union_:
            {
                int a = 0;
                break;
            }
        case epics::pvData::unionArray:
            {
                int a = 0;
                break;
            }
        }
    }
}

void PVStructureMerger::copyValue(epics::pvData::PVStructure* dest_structure, const pvd::PVStructure* src_structure)
{
    const pvd::StructureConstPtr& type = src_structure->getStructure();
    const pvd::PVFieldPtrArray&   children = src_structure->getPVFields();
    const pvd::StringArray&       names = type->getFieldNames();
    for (size_t i = 0, N = names.size(); i < N; i++)
    {
        auto const& src_fld = children[i].get();
        auto        src_type = src_fld->getField()->getType();
        auto        dest_field = dest_structure->getSubField(names[i]).get();
        auto        dest_type = dest_field->getField()->getType();
        if (src_type != dest_type)
            throw std::invalid_argument("filed of different type");
        switch (src_type)
        {
        case pvd::scalar:
            {
                dest_field->copyUnchecked(*src_fld);
                break;
            }
        case pvd::scalarArray:
            {
                auto dest_scalar_array = static_cast<pvd::PVScalarArray*>(dest_field);
                dest_scalar_array->copyUnchecked(*static_cast<const pvd::PVScalarArray*>(src_fld));
                break;
            }
        case pvd::structure:
            {
                auto dest_structure = static_cast<pvd::PVStructure*>(dest_field);
                copyValue(dest_structure, static_cast<const pvd::PVStructure*>(src_fld));
                break;
            }
        case pvd::structureArray:
            {
                break;
            }
        case pvd::union_:
            {
                break;
            }
        case pvd::unionArray:
            {
                break;
            }
        default:
            {
                throw std::logic_error("PVField::copy unknown type");
            }
        }
    }
}