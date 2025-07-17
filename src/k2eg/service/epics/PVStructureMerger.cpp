#include <k2eg/service/epics/PVStructureMerger.h>
#include <set>
#include <stdexcept>

namespace pvd = epics::pvData;
using namespace k2eg::service::epics_impl;

PVStructureMerger::PVStructureMerger() : root_builder(pvd::getFieldCreate()->createFieldBuilder()) {}

epics::pvData::PVStructure::const_shared_pointer PVStructureMerger::mergeStructureAndValue(const std::vector<PVStructureToMerge>& struct_ptr_vec)
{
    if (struct_ptr_vec.empty())
        return nullptr;

    // Build merged structure schema that includes all fields
    auto merged_builder = pvd::getFieldCreate()->createFieldBuilder();
    std::set<std::string> added_fields; // Track added fields to avoid duplicates

    // Add all fields from all structures to the builder
    for (const auto& item : struct_ptr_vec)
    {
        if (!item.structure)
            continue;

        for (const auto& fname : item.field_names)
        {
            // Skip if already added to avoid duplicates
            if (added_fields.find(fname) != added_fields.end())
                continue;
                
            auto src_field = item.structure->getSubField(fname);
            if (src_field)
            {
                addFieldToBuilder(merged_builder, fname, src_field.get());
                added_fields.insert(fname);
            }
        }
    }

    // Create the destination structure with the merged schema
    auto merged_structure = merged_builder->createStructure();
    auto dest = pvd::getPVDataCreate()->createPVStructure(merged_structure);

    // Copy values from all structures
    for (const auto& item : struct_ptr_vec)
    {
        if (!item.structure)
            continue;

        for (const auto& fname : item.field_names)
        {
            auto src_field = item.structure->getSubField(fname);
            auto dest_field = dest->getSubField(fname);
            if (src_field && dest_field)
            {
                copyFieldValue(dest_field.get(), src_field.get());
            }
        }
    }

    dest->setImmutable();
    return dest;
}

void PVStructureMerger::addFieldToBuilder(epics::pvData::FieldBuilderPtr builder, const std::string& field_name, const epics::pvData::PVField* field)
{
    auto fieldDef = field->getField();
    auto type = fieldDef->getType();
    
    switch (type)
    {
    case epics::pvData::scalar:
        {
            auto scalarDef = static_cast<const pvd::Scalar*>(fieldDef.get());
            builder->add(field_name, scalarDef->getScalarType());
            break;
        }
    case epics::pvData::scalarArray:
        {
            auto arrayDef = static_cast<const pvd::ScalarArray*>(fieldDef.get());
            builder->addArray(field_name, arrayDef->getElementType());
            break;
        }
    case epics::pvData::structure:
        {
            auto structDef = static_cast<const pvd::Structure*>(fieldDef.get());
            auto nested_builder = builder->addNestedStructure(field_name);
            copyStructureDefinition(nested_builder, structDef);
            nested_builder->endNested();
            break;
        }
    case epics::pvData::structureArray:
        {
            auto structArrayDef = static_cast<const pvd::StructureArray*>(fieldDef.get());
            auto elementStructDef = structArrayDef->getStructure();
            auto nested_builder = builder->addNestedStructureArray(field_name);
            copyStructureDefinition(nested_builder, elementStructDef.get());
            nested_builder->endNested();
            break;
        }
    case epics::pvData::union_:
        {
            auto unionDef = static_cast<const pvd::Union*>(fieldDef.get());
            auto nested_builder = builder->addNestedUnion(field_name);
            copyUnionDefinition(nested_builder, unionDef);
            nested_builder->endNested();
            break;
        }
    case epics::pvData::unionArray:
        {
            auto unionArrayDef = static_cast<const pvd::UnionArray*>(fieldDef.get());
            auto elementUnionDef = unionArrayDef->getUnion();
            auto nested_builder = builder->addNestedUnionArray(field_name);
            copyUnionDefinition(nested_builder, elementUnionDef.get());
            nested_builder->endNested();
            break;
        }
    default:
        throw std::runtime_error("Unsupported field type in addFieldToBuilder: " + std::to_string(type));
    }
}

void PVStructureMerger::copyStructureDefinition(epics::pvData::FieldBuilderPtr builder, const epics::pvData::Structure* structDef)
{
    const pvd::StringArray& names = structDef->getFieldNames();
    const pvd::FieldConstPtrArray& fields = structDef->getFields();
    
    for (size_t i = 0, N = names.size(); i < N; i++)
    {
        auto fieldDef = fields[i];
        auto type = fieldDef->getType();
        
        switch (type)
        {
        case epics::pvData::scalar:
            {
                auto scalarDef = static_cast<const pvd::Scalar*>(fieldDef.get());
                builder->add(names[i], scalarDef->getScalarType());
                break;
            }
        case epics::pvData::scalarArray:
            {
                auto arrayDef = static_cast<const pvd::ScalarArray*>(fieldDef.get());
                builder->addArray(names[i], arrayDef->getElementType());
                break;
            }
        case epics::pvData::structure:
            {
                auto nestedStructDef = static_cast<const pvd::Structure*>(fieldDef.get());
                auto nested_builder = builder->addNestedStructure(names[i]);
                copyStructureDefinition(nested_builder, nestedStructDef);
                nested_builder->endNested();
                break;
            }
        case epics::pvData::structureArray:
            {
                auto structArrayDef = static_cast<const pvd::StructureArray*>(fieldDef.get());
                auto elementStructDef = structArrayDef->getStructure();
                auto nested_builder = builder->addNestedStructureArray(names[i]);
                copyStructureDefinition(nested_builder, elementStructDef.get());
                nested_builder->endNested();
                break;
            }
        case epics::pvData::union_:
            {
                auto unionDef = static_cast<const pvd::Union*>(fieldDef.get());
                auto nested_builder = builder->addNestedUnion(names[i]);
                copyUnionDefinition(nested_builder, unionDef);
                nested_builder->endNested();
                break;
            }
        case epics::pvData::unionArray:
            {
                auto unionArrayDef = static_cast<const pvd::UnionArray*>(fieldDef.get());
                auto elementUnionDef = unionArrayDef->getUnion();
                auto nested_builder = builder->addNestedUnionArray(names[i]);
                copyUnionDefinition(nested_builder, elementUnionDef.get());
                nested_builder->endNested();
                break;
            }
        default:
            throw std::runtime_error("Unsupported nested field type: " + std::to_string(type));
        }
    }
}

void PVStructureMerger::copyUnionDefinition(epics::pvData::FieldBuilderPtr builder, const epics::pvData::Union* unionDef)
{
    const pvd::StringArray& names = unionDef->getFieldNames();
    const pvd::FieldConstPtrArray& fields = unionDef->getFields();
    
    for (size_t i = 0, N = names.size(); i < N; i++)
    {
        auto fieldDef = fields[i];
        auto type = fieldDef->getType();
        
        switch (type)
        {
        case epics::pvData::scalar:
            {
                auto scalarDef = static_cast<const pvd::Scalar*>(fieldDef.get());
                builder->add(names[i], scalarDef->getScalarType());
                break;
            }
        case epics::pvData::scalarArray:
            {
                auto arrayDef = static_cast<const pvd::ScalarArray*>(fieldDef.get());
                builder->addArray(names[i], arrayDef->getElementType());
                break;
            }
        case epics::pvData::structure:
            {
                auto nestedStructDef = static_cast<const pvd::Structure*>(fieldDef.get());
                auto nested_builder = builder->addNestedStructure(names[i]);
                copyStructureDefinition(nested_builder, nestedStructDef);
                nested_builder->endNested();
                break;
            }
        case epics::pvData::structureArray:
            {
                auto structArrayDef = static_cast<const pvd::StructureArray*>(fieldDef.get());
                auto elementStructDef = structArrayDef->getStructure();
                auto nested_builder = builder->addNestedStructureArray(names[i]);
                copyStructureDefinition(nested_builder, elementStructDef.get());
                nested_builder->endNested();
                break;
            }
        default:
            throw std::runtime_error("Unsupported union field type: " + std::to_string(type));
        }
    }
}

void PVStructureMerger::copyFieldValue(epics::pvData::PVField* dest_field, const epics::pvData::PVField* src_field)
{
    auto src_type = src_field->getField()->getType();
    auto dest_type = dest_field->getField()->getType();
    
    if (src_type != dest_type)
    {
        throw std::invalid_argument("Source and destination field types don't match");
    }
    
    switch (src_type)
    {
    case epics::pvData::scalar:
        {
            dest_field->copyUnchecked(*src_field);
            break;
        }
    case epics::pvData::scalarArray:
        {
            auto dest_array = static_cast<pvd::PVScalarArray*>(dest_field);
            auto src_array = static_cast<const pvd::PVScalarArray*>(src_field);
            dest_array->copyUnchecked(*src_array);
            break;
        }
    case epics::pvData::structure:
        {
            auto dest_struct = static_cast<pvd::PVStructure*>(dest_field);
            auto src_struct = static_cast<const pvd::PVStructure*>(src_field);
            copyStructureValue(dest_struct, src_struct);
            break;
        }
    case epics::pvData::structureArray:
        {
            auto dest_struct_array = static_cast<pvd::PVStructureArray*>(dest_field);
            auto src_struct_array = static_cast<const pvd::PVStructureArray*>(src_field);
            dest_struct_array->copyUnchecked(*src_struct_array);
            break;
        }
    case epics::pvData::union_:
        {
            auto dest_union = static_cast<pvd::PVUnion*>(dest_field);
            auto src_union = static_cast<const pvd::PVUnion*>(src_field);
            dest_union->copyUnchecked(*src_union);
            break;
        }
    case epics::pvData::unionArray:
        {
            auto dest_union_array = static_cast<pvd::PVUnionArray*>(dest_field);
            auto src_union_array = static_cast<const pvd::PVUnionArray*>(src_field);
            dest_union_array->copyUnchecked(*src_union_array);
            break;
        }
    default:
        throw std::runtime_error("Unsupported field type for value copy: " + std::to_string(src_type));
    }
}

void PVStructureMerger::copyStructureValue(epics::pvData::PVStructure* dest_structure, const epics::pvData::PVStructure* src_structure)
{
    const pvd::StructureConstPtr& type = src_structure->getStructure();
    const pvd::PVFieldPtrArray& children = src_structure->getPVFields();
    const pvd::StringArray& names = type->getFieldNames();
    
    for (size_t i = 0, N = names.size(); i < N; i++)
    {
        auto src_field = children[i].get();
        auto dest_field = dest_structure->getSubField(names[i]);
        
        if (dest_field)
        {
            copyFieldValue(dest_field.get(), src_field);
        }
    }
}

void PVStructureMerger::appendFieldFromStruct(std::vector<pvd::PVStructure::const_shared_pointer> struct_ptr_vec, bool reset)
{
    if (struct_ptr_vec.empty())
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
    auto structure = root_builder->createStructure();
    auto dest = pvd::getPVDataCreate()->createPVStructure(structure);
    
    for (auto& s : struct_ptr_vec)
    {
        if (!s)
            continue;
        copyStructureValue(dest.get(), s.get());
    }
    
    dest->setImmutable();
    return dest;
}

void PVStructureMerger::copyStructure(epics::pvData::FieldBuilderPtr builder, const epics::pvData::PVStructure* struct_ptr)
{
    const pvd::StructureConstPtr& type = struct_ptr->getStructure();
    copyStructureDefinition(builder, type.get());
}