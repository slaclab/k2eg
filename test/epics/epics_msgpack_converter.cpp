#include "pvType.h"
#include <gtest/gtest.h>
#include <iostream>
#include <k2eg/service/epics/MsgpackEpicsConverter.h>
#include <msgpack.hpp>
#include <pv/nt.h>
#include <pv/nttable.h>
#include <pv/pvData.h>
#include <pvType.h>

using namespace epics::pvData;
using namespace epics::nt;
using namespace k2eg::service::epics_impl;

class EpicsMsgpackConversionTest : public ::testing::Test
{
protected:
    static PVStructurePtr wrapInStructure(const std::string& name, const PVFieldPtr& field)
    {
        auto st = getFieldCreate()->createFieldBuilder()->add(name, field->getField())->createStructure();
        auto str = getPVDataCreate()->createPVStructure(st);
        str->getSubField(name)->copy(*field);
        return str;
    }

    static void roundtripTest(const PVStructurePtr& original)
    {
        msgpack::sbuffer                  buffer;
        msgpack::packer<msgpack::sbuffer> pk(buffer);
        MsgpackEpicsConverter::epicsToMsgpack(original, pk);

        msgpack::object_handle oh = msgpack::unpack(buffer.data(), buffer.size());
        std::cout << "Unpacked Msgpack object: " << oh.get() << std::endl;
        auto                   restored = MsgpackEpicsConverter::msgpackToEpics(oh.get(), original->getStructure());

        ASSERT_TRUE(restored != nullptr);
        ASSERT_TRUE(original->equals(*restored));
    }
};

TEST_F(EpicsMsgpackConversionTest, ScalarTypes)
{
    // Boolean
    {
        auto f = getPVDataCreate()->createPVScalar(pvBoolean);
        std::static_pointer_cast<PVBoolean>(f)->put(true);
        roundtripTest(wrapInStructure("value", f));
    }
    // Byte
    {
        auto f = getPVDataCreate()->createPVScalar(pvByte);
        std::static_pointer_cast<PVByte>(f)->put(-8);
        roundtripTest(wrapInStructure("value", f));
    }
    // Short
    {
        auto f = getPVDataCreate()->createPVScalar(pvShort);
        std::static_pointer_cast<PVShort>(f)->put(-1234);
        roundtripTest(wrapInStructure("value", f));
    }
    // Int
    {
        auto f = getPVDataCreate()->createPVScalar(pvInt);
        std::static_pointer_cast<PVInt>(f)->put(42);
        roundtripTest(wrapInStructure("value", f));
    }
    // Long
    {
        auto f = getPVDataCreate()->createPVScalar(pvLong);
        std::static_pointer_cast<PVLong>(f)->put(123456789LL);
        roundtripTest(wrapInStructure("value", f));
    }
    // UByte
    {
        auto f = getPVDataCreate()->createPVScalar(pvUByte);
        std::static_pointer_cast<PVUByte>(f)->put(200);
        roundtripTest(wrapInStructure("value", f));
    }
    // UShort
    {
        auto f = getPVDataCreate()->createPVScalar(pvUShort);
        std::static_pointer_cast<PVUShort>(f)->put(60000);
        roundtripTest(wrapInStructure("value", f));
    }
    // UInt
    {
        auto f = getPVDataCreate()->createPVScalar(pvUInt);
        std::static_pointer_cast<PVUInt>(f)->put(4000000000U);
        roundtripTest(wrapInStructure("value", f));
    }
    // ULong
    {
        auto f = getPVDataCreate()->createPVScalar(pvULong);
        std::static_pointer_cast<PVULong>(f)->put(9000000000000000000ULL);
        roundtripTest(wrapInStructure("value", f));
    }
    // Float
    {
        auto f = getPVDataCreate()->createPVScalar(pvFloat);
        std::static_pointer_cast<PVFloat>(f)->put(1.23f);
        roundtripTest(wrapInStructure("value", f));
    }
    // Double
    {
        auto f = getPVDataCreate()->createPVScalar(pvDouble);
        std::static_pointer_cast<PVDouble>(f)->put(3.14);
        roundtripTest(wrapInStructure("value", f));
    }
    // String
    {
        auto f = getPVDataCreate()->createPVScalar(pvString);
        std::static_pointer_cast<PVString>(f)->put("hello");
        roundtripTest(wrapInStructure("value", f));
    }
}

TEST_F(EpicsMsgpackConversionTest, ScalarArrayTypes)
{
    // BooleanArray
    {
        shared_vector<epics::pvData::boolean> values = {true, true, false};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvBoolean)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVBooleanArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // ByteArray
    {
        shared_vector<epics::pvData::int8> values = {1, 2, 0};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvByte)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVByteArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // ShortArray
    {
        shared_vector<epics::pvData::int16> values = {1,2, 0};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvShort)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVShortArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // IntArray
    {
        shared_vector<epics::pvData::int32> values = {1,2, 0};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvInt)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVIntArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // LongArray
    {
        shared_vector<epics::pvData::int64> values = {1,2, 0};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvLong)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVLongArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // UByteArray
    {
        shared_vector<epics::pvData::uint8> values = {1,2, 0};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvUByte)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVUByteArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // UShortArray
    {
        shared_vector<epics::pvData::uint16> values = {60000, 123, 0};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvUShort)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVUShortArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // UIntArray
    {
        shared_vector<epics::pvData::uint32> values = {4000000000U, 123U, 0U};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvUInt)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVUIntArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // ULongArray
    {
        shared_vector<epics::pvData::uint64> values = {9000000000000000000ULL, 123ULL, 0ULL};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvULong)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVULongArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // FloatArray
    {
        shared_vector<float> values = {1.1f, 2.2f, 3.3f};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvFloat)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVFloatArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // DoubleArray
    {
        shared_vector<double> values = {3.14, 2.71, 0.0};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvDouble)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVDoubleArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
    // StringArray
    {
        shared_vector<std::string> values = {"hello", "world", ""};
        auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvString)->createStructure();
        auto arr_struct = getPVDataCreate()->createPVStructure(st);
        arr_struct->getSubField<PVStringArray>("value")->putFrom(freeze(values));
        roundtripTest(arr_struct);
    }
}

TEST_F(EpicsMsgpackConversionTest, NTTableRoundtrip)
{
    NTTablePtr                 table = NTTable::createBuilder()->addColumn("x", pvInt)->addColumn("y", pvInt)->create();
    shared_vector<std::string> labels = {"x", "y"};
    shared_vector<int32>       col1 = {1, 2};
    shared_vector<int32>       col2 = {3, 4};

    table->getLabels()->replace(freeze(labels));
    table->getColumn<PVIntArray>("x")->replace(freeze(col1));
    table->getColumn<PVIntArray>("y")->replace(freeze(col2));

    roundtripTest(table->getPVStructure());
}

TEST_F(EpicsMsgpackConversionTest, StructureArray)
{
    auto innerStruct = getFieldCreate()->createFieldBuilder()->add("a", pvInt)->createStructure();
    auto s1 = getPVDataCreate()->createPVStructure(innerStruct);
    s1->getSubField<PVInt>("a")->put(1);
    auto s2 = getPVDataCreate()->createPVStructure(innerStruct);
    s2->getSubField<PVInt>("a")->put(2);

    auto stArray =
        getFieldCreate()->createFieldBuilder()->addNestedStructureArray("value")->add("a", pvInt)->endNested()->createStructure();
    auto arrStruct = getPVDataCreate()->createPVStructure(stArray);

    // Use shared_vector for replace
    shared_vector<PVStructurePtr> tmp;
    tmp.push_back(s1);
    tmp.push_back(s2);
    shared_vector<const PVStructurePtr> arr = freeze(tmp);
    arrStruct->getSubField<PVStructureArray>("value")->replace(arr);

    roundtripTest(arrStruct);
}

TEST_F(EpicsMsgpackConversionTest, UnionScalar)
{
    auto utype = getFieldCreate()->createFieldBuilder()->add("a", pvInt)->add("b", pvString)->createUnion();
    auto u = getPVDataCreate()->createPVUnion(utype);
    u->select("a");
    std::static_pointer_cast<PVInt>(u->get())->put(99);

    auto outer = getFieldCreate()->createFieldBuilder()->add("value", utype)->createStructure();
    auto uStruct = getPVDataCreate()->createPVStructure(outer);
    uStruct->getSubField<PVUnion>("value")->copy(*u);

    roundtripTest(uStruct);
}

TEST_F(EpicsMsgpackConversionTest, UnionArray)
{
    auto utype = getFieldCreate()->createFieldBuilder()->add("a", pvInt)->add("b", pvString)->createUnion();
    auto u1 = getPVDataCreate()->createPVUnion(utype);
    u1->select("a");
    std::static_pointer_cast<PVInt>(u1->get())->put(1);

    auto u2 = getPVDataCreate()->createPVUnion(utype);
    u2->select("b");
    std::static_pointer_cast<PVString>(u2->get())->put("test");

    auto uaField = getFieldCreate()->createUnionArray(utype);
    auto st = getFieldCreate()->createFieldBuilder()->add("value", uaField)->createStructure();
    auto arrStruct = getPVDataCreate()->createPVStructure(st);

    // Prepare the array of unions
    shared_vector<PVUnionPtr> tmp;
    tmp.push_back(u1);
    tmp.push_back(u2);
    shared_vector<const PVUnionPtr> arr_const = freeze(tmp);

    arrStruct->getSubField<PVUnionArray>("value")->replace(arr_const);
    roundtripTest(arrStruct);
}