#include <k2eg/service/epics/EpicsGetOperation.h>
#include <k2eg/service/epics/EpicsData.h>
#include <pv/createRequest.h>
#include <pvData.h>

#include <client.h>
#include <pvIntrospect.h>

using namespace k2eg::service::epics_impl;

namespace pvd = epics::pvData;

CombinedGetOperation::CombinedGetOperation(GetOperationShrdPtr get_op_a, GetOperationShrdPtr get_op_b) : get_op_a(get_op_a), get_op_b(get_op_b) {}

bool
CombinedGetOperation::isDone() const {
  return get_op_a->isDone() && get_op_b->isDone();
}

// return the bad one
const pvac::GetEvent&
CombinedGetOperation::getState() const {
  auto state_a = get_op_a->getState();
  auto state_b = get_op_b->getState();
  if (state_a.event == state_b.event) {
    return get_op_a->getState();
  } else if (state_a.event == pvac::PutEvent::Fail || state_a.event == pvac::PutEvent::Cancel) {
    return get_op_a->getState();
  } else {
    return get_op_b->getState();
  }
}

// combine all the data
ConstChannelDataUPtr
CombinedGetOperation::getChannelData() const {
  ConstChannelDataUPtr result;
  if (isDone() == false) return result;
  if (getState().event != pvac::PutEvent::Success) return result;

  // we have data so combine it
auto                          builder  = pvd::getFieldCreate()->createFieldBuilder();
copyStructure(builder, get_op_a->getChannelData()->data.get());
copyStructure(builder, get_op_b->getChannelData()->data.get());
auto pvStructure = builder->createStructure()->build();
std::cout << pvStructure << std::endl;
std::cout << get_op_a->getChannelData()->data << std::endl;
std::cout << get_op_b->getChannelData()->data << std::endl;
pvStructure->copyUnchecked(*get_op_a->getChannelData()->data);
pvStructure->copyUnchecked(*get_op_b->getChannelData()->data);
return MakeChannelDataUPtr(
  get_op_a->getChannelData()->pv_name,
  pvStructure
);
}

void 
CombinedGetOperation::copyStructure(pvd::FieldBuilderPtr builder, const pvd::PVStructure* structure) const {
const pvd::StructureConstPtr& type     = structure->getStructure();
const pvd::PVFieldPtrArray&   children = structure->getPVFields();
const pvd::StringArray&       names    = type->getFieldNames();
for (size_t i = 0, N = names.size(); i < N; i++) {
    auto const& fld  = children[i].get();
    auto        type = fld->getField()->getType();
    switch(type) {
      case epics::pvData::scalar:
      builder->add( names[i], static_cast<const pvd::PVScalar*>(fld)->getScalar()->getScalarType());
      break;
      case epics::pvData::scalarArray:
      builder->addArray(names[i], static_cast<const pvd::PVScalarArray*>(fld)->getScalarArray()->getElementType());
      break;
      case epics::pvData::structure:{
        auto nested_buider = builder->addNestedStructure(names[i]);
        copyStructure(nested_buider, static_cast<const pvd::PVStructure*>(fld));
        nested_buider->endNested();
        break;
      }
      case epics::pvData::structureArray:{
        int a = 0;
        break;
      }
      case epics::pvData::union_:{
        int a = 0;
        break;
      }
      case epics::pvData::unionArray:{
        int a = 0;
        break;
      }
    }
    if (type == pvd::Type::scalar) {
      
    } else if (type == pvd::Type::scalarArray) {
      
    } else if (type == pvd::Type::structure) {
      // const pv::StructureConstPtr& structType = std::dynamic_pointer_cast<const pv::Structure>(field->getType());
      // builder.add(fieldName, structType);
    }
  }
}

bool
CombinedGetOperation::hasData() const {
  return get_op_a->hasData() && get_op_b->hasData();
}

//----------------- SingleGetOperation  ------------------
SingleGetOperation::SingleGetOperation(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& field)
    : channel(channel), pv_name(pv_name), field(field), is_done(false) {
  channel->addConnectListener(this);
}

SingleGetOperation::~SingleGetOperation() {
  channel->removeConnectListener(this);
  op.cancel();
}

void
SingleGetOperation::getDone(const pvac::GetEvent& event) {
  switch (event.event) {
    case pvac::GetEvent::Fail: break;
    case pvac::GetEvent::Cancel: break;
    case pvac::GetEvent::Success: {
      break;
    }
  }
  evt     = event;
  is_done = true;
}

void
SingleGetOperation::connectEvent(const pvac::ConnectEvent& evt) {
  if (evt.connected) { op = channel->get(this, pvd::createRequest(field)); }
}

bool
SingleGetOperation::isDone() const {
  return is_done;
}

const pvac::GetEvent&
SingleGetOperation::getState() const {
  return evt;
}

ConstChannelDataUPtr
SingleGetOperation::getChannelData() const {
  return std::make_unique<ChannelData>(ChannelData{pv_name, evt.value});
}
bool
SingleGetOperation::hasData() const {
  return evt.value != nullptr;
}