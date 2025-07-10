#ifndef K2EG_SERVICE_STORAGE_IMPL_MSGPACKTOBSONCONVERTER_H_
#define K2EG_SERVICE_STORAGE_IMPL_MSGPACKTOBSONCONVERTER_H_

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/types.hpp>
#include <msgpack.hpp>

namespace k2eg::service::storage::impl {

class MsgPackToBsonConverter {
public:
    static bsoncxx::document::value convertToBson(const std::vector<uint8_t>& msgpack_data);
    static std::optional<std::string> extractPvName(const std::vector<uint8_t>& msgpack_data);
    static std::optional<std::chrono::system_clock::time_point> extractEpicsTimestamp(const std::vector<uint8_t>& msgpack_data);

private:
    static void convertMapToBson(const msgpack::object& map_obj, bsoncxx::builder::basic::document& doc);
    static void convertCompactArrayToBson(const msgpack::object& array_obj, bsoncxx::builder::basic::document& doc);
    
    // Overloaded convertObject methods
    static void convertObject(const msgpack::object& obj, bsoncxx::builder::basic::document& doc, const std::string& key);
    static void convertObject(const msgpack::object& obj, bsoncxx::builder::basic::array& arr);
    
    static std::optional<std::chrono::system_clock::time_point> parseEpicsTimestamp(const msgpack::object& timestamp_obj);
    static std::optional<std::chrono::system_clock::time_point> searchTimestampInPvData(const msgpack::object& pv_obj);
};

} // namespace k2eg::service::storage::impl

#endif // K2EG_SERVICE_STORAGE_IMPL_MSGPACKTOBSONCONVERTER_H_