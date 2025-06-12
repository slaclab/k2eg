#include <gtest/gtest.h>
#include <iostream>
#include <k2eg/service/epics/MsgpackEpicsConverter.h>
#include <msgpack.hpp>
#include <pv/nt.h>
#include <pv/nttable.h>
#include <pv/pvData.h>

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
        std::cout << "Unpacked msgpack object: " << oh.get() << std::endl;
        auto                   restored = MsgpackEpicsConverter::msgpackToEpics(oh.get(), original->getStructure());

        ASSERT_TRUE(restored != nullptr);
        ASSERT_TRUE(original->equals(*restored));
    }
};

TEST_F(EpicsMsgpackConversionTest, BooleanScalar)
{
    auto f = getPVDataCreate()->createPVScalar(pvBoolean);
    std::static_pointer_cast<PVBoolean>(f)->put(true);
    roundtripTest(wrapInStructure("value", f));
}

TEST_F(EpicsMsgpackConversionTest, IntScalar)
{
    auto f = getPVDataCreate()->createPVScalar(pvInt);
    std::static_pointer_cast<PVInt>(f)->put(42);
    roundtripTest(wrapInStructure("value", f));
}

TEST_F(EpicsMsgpackConversionTest, DoubleScalar)
{
    auto f = getPVDataCreate()->createPVScalar(pvDouble);
    std::static_pointer_cast<PVDouble>(f)->put(3.14);
    roundtripTest(wrapInStructure("value", f));
}

TEST_F(EpicsMsgpackConversionTest, StringScalar)
{
    auto f = getPVDataCreate()->createPVScalar(pvString);
    std::static_pointer_cast<PVString>(f)->put("hello");
    roundtripTest(wrapInStructure("value", f));
}

TEST_F(EpicsMsgpackConversionTest, FloatArray)
{
    shared_vector<float> values = {1.1f, 2.2f, 3.3f};
    auto st = getFieldCreate()->createFieldBuilder()->addArray("value", pvFloat)->createStructure();
    auto arr_struct = getPVDataCreate()->createPVStructure(st);
    arr_struct->getSubField<PVFloatArray>("value")->putFrom(freeze(values));
    roundtripTest(arr_struct);
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