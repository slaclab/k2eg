#ifndef K2EG_SERVICE_STORAGE_IMPL_MSGPACKTOBSONCONVERTER_H_
#define K2EG_SERVICE_STORAGE_IMPL_MSGPACKTOBSONCONVERTER_H_

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/types.hpp>
#include <msgpack.hpp>

namespace k2eg::service::storage::impl {

/**
 * Converts a MessagePack byte array to a BSON document.
 */
class MsgPackToBsonConverter
{
public:
    /**
     * Converts a MessagePack byte array to a BSON document.
     */
    static bsoncxx::document::value   convertToBson(const std::vector<uint8_t>& msgpack_data);
    /**
     * Extracts the PV name from the MessagePack data.
     * Returns std::nullopt if not found or if the data is malformed.
     */
    static std::optional<std::string> extractPvName(const std::vector<uint8_t>& msgpack_data);
    /**
     * Extracts the EPICS timestamp from the MessagePack data.
     * Returns std::nullopt if not found or if the data is malformed.
     */
    static std::optional<std::chrono::system_clock::time_point> extractEpicsTimestamp(const std::vector<uint8_t>& msgpack_data);
private:
    /**
     * Converts a MessagePack map object to a BSON document.
     */
    static void convertMapToBson(const msgpack::object& map_obj, bsoncxx::builder::basic::document& doc);
    /**
     * Converts a MessagePack object to a BSON document or array.
     */
    static void convertCompactArrayToBson(const msgpack::object& array_obj, bsoncxx::builder::basic::document& doc);
    /**
     * Converts a MessagePack object to a BSON document or array.
     */
    static void convertObject(const msgpack::object& obj, bsoncxx::builder::basic::document& doc, const std::string& key);
    /**
     * Converts a MessagePack object to a BSON array.
     */
    static void convertObject(const msgpack::object& obj, bsoncxx::builder::basic::array& arr);
    /**
     * Converts a MessagePack object to a BSON value.
     */
    static std::optional<std::chrono::system_clock::time_point> parseEpicsTimestamp(const msgpack::object& timestamp_obj);
    /**
     * Searches for the PV name in the MessagePack data.
     */
    static std::optional<std::chrono::system_clock::time_point> searchTimestampInPvData(const msgpack::object& pv_obj);
};

} // namespace k2eg::service::storage::impl

#endif // K2EG_SERVICE_STORAGE_IMPL_MSGPACKTOBSONCONVERTER_H_