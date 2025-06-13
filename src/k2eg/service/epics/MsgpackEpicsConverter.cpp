#include <k2eg/service/epics/MsgpackEpicsConverter.h>

#include <pv/convert.h>
#include <pvType.h>

namespace pvd = epics::pvData;
using namespace k2eg::service::epics_impl;

#define PACK_TYPED_ARRAY(T, arr, packer)                \
    {                                                   \
        pvd::shared_vector<const T> data;                     \
        arr->getAs(data);                                \
        packer.pack_array(data.size());                 \
        for (auto& e : data) packer.pack(e);            \
    }

void MsgpackEpicsConverter::epicsToMsgpack(const pvd::PVStructurePtr& pvStruct, msgpack::packer<msgpack::sbuffer>& pk)
{
    const auto& names = pvStruct->getStructure()->getFieldNames();
    pk.pack_map(names.size());

    for (const auto& name : names)
    {
        pk.pack(name);
        packField(pk, pvStruct->getSubField(name));
    }
}

void MsgpackEpicsConverter::packField(msgpack::packer<msgpack::sbuffer>& pk, const pvd::PVFieldPtr& field)
{
    if (!field)
    {
        pk.pack_nil();
        return;
    }
    switch (field->getField()->getType())
    {
    case pvd::scalar:
        {
            auto s = std::dynamic_pointer_cast<pvd::PVScalar>(field);
            switch (s->getScalar()->getScalarType())
            {
                // clang-format off
                case pvd::pvBoolean: pk.pack(static_cast<bool>(std::static_pointer_cast<pvd::PVBoolean>(s)->get())); break;
                case pvd::pvByte:    pk.pack(std::static_pointer_cast<pvd::PVByte>(s)->get()); break;
                case pvd::pvShort:   pk.pack(std::static_pointer_cast<pvd::PVShort>(s)->get()); break;
                case pvd::pvInt:     pk.pack(std::static_pointer_cast<pvd::PVInt>(s)->get()); break;
                case pvd::pvLong:    pk.pack(std::static_pointer_cast<pvd::PVLong>(s)->get()); break;
                case pvd::pvUByte:   pk.pack(std::static_pointer_cast<pvd::PVUByte>(s)->get()); break;
                case pvd::pvUShort:  pk.pack(std::static_pointer_cast<pvd::PVUShort>(s)->get()); break;
                case pvd::pvUInt:    pk.pack(std::static_pointer_cast<pvd::PVUInt>(s)->get()); break;
                case pvd::pvULong:   pk.pack(std::static_pointer_cast<pvd::PVULong>(s)->get()); break;
                case pvd::pvFloat:   pk.pack(std::static_pointer_cast<pvd::PVFloat>(s)->get()); break;
                case pvd::pvDouble:  pk.pack(std::static_pointer_cast<pvd::PVDouble>(s)->get()); break;
                case pvd::pvString:  pk.pack(std::static_pointer_cast<pvd::PVString>(s)->get()); break;
                default: pk.pack_nil();
                // clang-format on
            }
            break;
        }
    case pvd::scalarArray:
        {
            auto            arr = std::dynamic_pointer_cast<pvd::PVScalarArray>(field);
            pvd::ScalarType etype = arr->getScalarArray()->getElementType();
            switch (etype)
            {
                // clang-format off
                case pvd::pvBoolean: { PACK_TYPED_ARRAY(pvd::boolean, arr, pk); } break;
                case pvd::pvByte:    { PACK_TYPED_ARRAY(pvd::int8, arr, pk); } break;
                case pvd::pvShort:   { PACK_TYPED_ARRAY(pvd::uint16, arr, pk); } break;
                case pvd::pvInt:     { PACK_TYPED_ARRAY(pvd::int32, arr, pk); } break;
                case pvd::pvLong:    { PACK_TYPED_ARRAY(pvd::int64, arr, pk); } break;
                case pvd::pvUByte:   { PACK_TYPED_ARRAY(pvd::uint8, arr, pk); } break;
                case pvd::pvUShort:  { PACK_TYPED_ARRAY(pvd::uint16, arr, pk); } break;
                case pvd::pvUInt:    { PACK_TYPED_ARRAY(pvd::uint32, arr, pk); } break;
                case pvd::pvULong:   { PACK_TYPED_ARRAY(pvd::uint64, arr, pk); } break;
                case pvd::pvFloat:   { PACK_TYPED_ARRAY(float, arr, pk); } break;
                case pvd::pvDouble:  { PACK_TYPED_ARRAY(double, arr, pk); } break;
                case pvd::pvString:  { PACK_TYPED_ARRAY(std::string, arr, pk); } break;
                default: pk.pack_nil(); break;
                // clang-format on
            }
            break;
        }
    case pvd::structure:
        {
            epicsToMsgpack(std::dynamic_pointer_cast<pvd::PVStructure>(field), pk);
            break;
        }
    case pvd::structureArray:
        {
            auto arr = std::dynamic_pointer_cast<pvd::PVStructureArray>(field);
            auto view = arr->view();
            pk.pack_array(view.size());
            for (auto const& s : view)
            {
                epicsToMsgpack(s, pk);
            }
            break;
        }
    case pvd::union_:
        {
            auto u = std::dynamic_pointer_cast<pvd::PVUnion>(field);
            pk.pack_map(1);
            pk.pack(u->getSelectedFieldName());
            packField(pk, u->get());
            break;
        }
    case pvd::unionArray:
        {
            auto ua = std::dynamic_pointer_cast<pvd::PVUnionArray>(field);
            auto view = ua->view();
            pk.pack_array(view.size());
            for (const auto& u : view)
            {
                pk.pack_map(1);
                pk.pack(u->getSelectedFieldName());
                packField(pk, u->get());
            }
            break;
        }
    default: pk.pack_nil();
    }
}

pvd::PVStructurePtr MsgpackEpicsConverter::msgpackToEpics(const msgpack::object& obj, const pvd::StructureConstPtr& schema)
{
    pvd::PVStructurePtr pvStruct = pvd::getPVDataCreate()->createPVStructure(schema);
    if (obj.type != msgpack::type::MAP)
        return pvStruct;

    for (uint32_t i = 0; i < obj.via.map.size; ++i)
    {
        const msgpack::object_kv& kv = obj.via.map.ptr[i];
        std::string               key = kv.key.as<std::string>();
        pvd::FieldConstPtr        fieldDesc = schema->getField(key);
        if (!fieldDesc)
            continue;
        // FIX: Call as static method of the class
        pvd::PVFieldPtr value = MsgpackEpicsConverter::unpackMsgpackToPVField(kv.val, fieldDesc);
        if (value)
        {
            pvStruct->getSubField(key)->copy(*value);
        }
    }
    return pvStruct;
}

#define UNPACK_TYPED_ARRAY(T, arr, obj)                        \
    {                                                          \
        pvd::shared_vector<T> vals;                            \
        for (uint32_t i = 0; i < obj.via.array.size; ++i)      \
        {                                                      \
            vals.push_back(obj.via.array.ptr[i].as<T>());      \
        }                                                      \
        arr->putFrom(pvd::freeze(vals));                       \
    }

pvd::PVFieldPtr MsgpackEpicsConverter::unpackMsgpackToPVField(const msgpack::object& obj, const pvd::FieldConstPtr& field)
{
    auto& pvCreate = *pvd::getPVDataCreate();

    switch (field->getType())
    {
    case pvd::scalar:
        {
            auto scalar = std::dynamic_pointer_cast<const pvd::Scalar>(field);
            switch (scalar->getScalarType())
            {
                // clang-format off
                case pvd::pvBoolean: { auto pv = pvCreate.createPVScalar(pvd::pvBoolean); std::static_pointer_cast<pvd::PVBoolean>(pv)->put(obj.as<bool>()); return pv; }
                case pvd::pvByte:    { auto pv = pvCreate.createPVScalar(pvd::pvByte);    std::static_pointer_cast<pvd::PVByte>(pv)->put(obj.as<int8_t>()); return pv; }
                case pvd::pvShort:   { auto pv = pvCreate.createPVScalar(pvd::pvShort);   std::static_pointer_cast<pvd::PVShort>(pv)->put(obj.as<int16_t>()); return pv; }
                case pvd::pvInt:     { auto pv = pvCreate.createPVScalar(pvd::pvInt);     std::static_pointer_cast<pvd::PVInt>(pv)->put(obj.as<int32_t>()); return pv; }
                case pvd::pvLong:    { auto pv = pvCreate.createPVScalar(pvd::pvLong);    std::static_pointer_cast<pvd::PVLong>(pv)->put(obj.as<int64_t>()); return pv; }
                case pvd::pvUByte:   { auto pv = pvCreate.createPVScalar(pvd::pvUByte);   std::static_pointer_cast<pvd::PVUByte>(pv)->put(obj.as<uint8_t>()); return pv; }
                case pvd::pvUShort:  { auto pv = pvCreate.createPVScalar(pvd::pvUShort);  std::static_pointer_cast<pvd::PVUShort>(pv)->put(obj.as<uint16_t>()); return pv; }
                case pvd::pvUInt:    { auto pv = pvCreate.createPVScalar(pvd::pvUInt);    std::static_pointer_cast<pvd::PVUInt>(pv)->put(obj.as<uint32_t>()); return pv; }
                case pvd::pvULong:   { auto pv = pvCreate.createPVScalar(pvd::pvULong);   std::static_pointer_cast<pvd::PVULong>(pv)->put(obj.as<uint64_t>()); return pv; }
                case pvd::pvFloat:   { auto pv = pvCreate.createPVScalar(pvd::pvFloat);   std::static_pointer_cast<pvd::PVFloat>(pv)->put(obj.as<float>()); return pv; }
                case pvd::pvDouble:  { auto pv = pvCreate.createPVScalar(pvd::pvDouble);  std::static_pointer_cast<pvd::PVDouble>(pv)->put(obj.as<double>()); return pv; }
                case pvd::pvString:  { auto pv = pvCreate.createPVScalar(pvd::pvString);  std::static_pointer_cast<pvd::PVString>(pv)->put(obj.as<std::string>()); return pv; }
                default: return nullptr;
                // clang-format on
            }
        }
    case pvd::scalarArray:
        {
            auto sa = std::dynamic_pointer_cast<const pvd::ScalarArray>(field);
            auto et = sa->getElementType();
            auto arr = pvCreate.createPVScalarArray(et);

            if (obj.type == msgpack::type::ARRAY)
            {
                switch (et)
                {
                    // clang-format off
                    case pvd::pvBoolean: { UNPACK_TYPED_ARRAY(pvd::boolean, arr, obj); break; }
                    case pvd::pvByte:    { UNPACK_TYPED_ARRAY(pvd::int8, arr, obj); break; }
                    case pvd::pvShort:   { UNPACK_TYPED_ARRAY(pvd::int16, arr, obj); break; }
                    case pvd::pvInt:     { UNPACK_TYPED_ARRAY(pvd::int32, arr, obj); break; }
                    case pvd::pvLong:    { UNPACK_TYPED_ARRAY(pvd::int64, arr, obj); break; }
                    case pvd::pvUByte:   { UNPACK_TYPED_ARRAY(pvd::uint8, arr, obj); break; }
                    case pvd::pvUShort:  { UNPACK_TYPED_ARRAY(pvd::uint16, arr, obj); break; }
                    case pvd::pvUInt:    { UNPACK_TYPED_ARRAY(pvd::uint32, arr, obj); break; }
                    case pvd::pvULong:   { UNPACK_TYPED_ARRAY(pvd::uint64, arr, obj); break; }
                    case pvd::pvFloat:   { UNPACK_TYPED_ARRAY(float, arr, obj); break; }
                    case pvd::pvDouble:  { UNPACK_TYPED_ARRAY(double, arr, obj); break; }
                    case pvd::pvString:  { UNPACK_TYPED_ARRAY(std::string, arr, obj); break; }
                    default: break;
                    // clang-format on
                }
            }
            return arr;
        }
    case pvd::structure:
        {
            auto s = std::dynamic_pointer_cast<const pvd::Structure>(field);
            return MsgpackEpicsConverter::msgpackToEpics(obj, s);
        }
    case pvd::structureArray:
        {
            auto                                    sa = std::dynamic_pointer_cast<const pvd::StructureArray>(field);
            auto                                    st = sa->getStructure();
            auto                                    arr = pvCreate.createPVStructureArray(sa);
            pvd::shared_vector<pvd::PVStructurePtr> elements;
            for (uint32_t i = 0; i < obj.via.array.size; ++i)
            {
                elements.push_back(msgpackToEpics(obj.via.array.ptr[i], st));
            }
            arr->replace(pvd::freeze(elements));
            return arr;
        }
    case pvd::union_:
        {
            auto udef = std::dynamic_pointer_cast<const pvd::Union>(field);
            if (obj.type != msgpack::type::MAP || obj.via.map.size != 1)
                return nullptr;

            std::string fieldName = obj.via.map.ptr[0].key.as<std::string>();
            auto        subObj = obj.via.map.ptr[0].val;
            auto        subField = udef->getField(fieldName);
            if (!subField)
                return nullptr;

            auto value = unpackMsgpackToPVField(subObj, subField);
            auto u = pvCreate.createPVUnion(udef);
            u->select(fieldName);
            u->get()->copy(*value);
            return u;
        }
    case pvd::unionArray:
        {
            auto                                ua = std::dynamic_pointer_cast<const pvd::UnionArray>(field);
            auto                                udef = ua->getUnion();
            pvd::shared_vector<pvd::PVUnionPtr> result;

            for (uint32_t i = 0; i < obj.via.array.size; ++i)
            {
                auto entry = obj.via.array.ptr[i];
                if (entry.type != msgpack::type::MAP || entry.via.map.size != 1)
                    continue;

                std::string fname = entry.via.map.ptr[0].key.as<std::string>();
                auto        fdesc = udef->getField(fname);
                if (!fdesc)
                    continue;

                auto value = unpackMsgpackToPVField(entry.via.map.ptr[0].val, fdesc);
                auto u = pvCreate.createPVUnion(udef);
                u->select(fname);
                u->get()->copy(*value);
                result.push_back(u);
            }

            auto arr = pvCreate.createPVUnionArray(ua);
            arr->replace(pvd::freeze(result));
            return arr;
        }
    default: return nullptr;
    }
}