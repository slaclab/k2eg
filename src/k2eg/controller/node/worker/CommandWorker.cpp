#include <k2eg/controller/node/worker/CommandWorker.h>

using namespace k2eg::controller::node::worker;

ReplySerializer::ReplySerializer(
    std::shared_ptr<boost::json::object> reply_content, 
    k2eg::common::SerializationType ser_type)
    :reply_content(reply_content)
    , ser_type(ser_type){}