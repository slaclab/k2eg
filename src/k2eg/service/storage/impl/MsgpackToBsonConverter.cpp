#include <k2eg/service/storage/impl/MsgpackToBsonConverter.h>

#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>

using namespace bsoncxx::builder::basic;
using namespace bsoncxx::types;

using namespace k2eg::service::storage::impl;

bsoncxx::document::value MsgPackToBsonConverter::convertToBson(const std::vector<uint8_t>& msgpack_data) {
    try {
        // Unpack msgpack data
        msgpack::object_handle oh = msgpack::unpack(
            reinterpret_cast<const char*>(msgpack_data.data()), 
            msgpack_data.size()
        );
        
        auto doc = document{};
        
        // Handle different msgpack root types
        const msgpack::object& root = oh.get();
        
        if (root.type == msgpack::type::MAP) {
            // Most EPICS data comes as a map
            convertMapToBson(root, doc);
        } else if (root.type == msgpack::type::ARRAY && root.via.array.size >= 2) {
             // Arrays should be stored as arrays, not decomposed
            convertObject(root, doc, "raw_value");
        } else {
            // Store as raw value if structure is unexpected
            convertObject(root, doc, "raw_value");
        }
        
        return doc.extract();
    }
    catch (const std::exception& e) {
        // If conversion fails, store error info
        auto doc = document{};
        doc.append(kvp("conversion_error", e.what()));
        doc.append(kvp("raw_data_size", static_cast<int64_t>(msgpack_data.size())));
        return doc.extract();
    }
}

void MsgPackToBsonConverter::convertMapToBson(const msgpack::object& map_obj, document& doc) {
    if (map_obj.type != msgpack::type::MAP) return;
    
    auto& map = map_obj.via.map;
    
    for (uint32_t i = 0; i < map.size; ++i) {
        const auto& kv = map.ptr[i];
        
        // Get key as string
        std::string key;
        if (kv.key.type == msgpack::type::STR) {
            key = kv.key.as<std::string>();
        } else {
            key = "key_" + std::to_string(i); // Fallback for non-string keys
        }
        
        // Convert value based on its type
        convertObject(kv.val, doc, key);
    }
}

void MsgPackToBsonConverter::convertCompactArrayToBson(const msgpack::object& array_obj, document& doc) {
    if (array_obj.type != msgpack::type::ARRAY) return;
    
    auto& arr = array_obj.via.array;
    
    // First element should be PV name
    if (arr.size > 0 && arr.ptr[0].type == msgpack::type::STR) {
        doc.append(kvp("pv_name", arr.ptr[0].as<std::string>()));
    }
    
    // Convert remaining elements as indexed values
    for (uint32_t i = 1; i < arr.size; ++i) {
        std::string key = "value_" + std::to_string(i - 1);
        convertObject(arr.ptr[i], doc, key);
    }
}

void MsgPackToBsonConverter::convertObject(const msgpack::object& obj, document& doc, const std::string& key) {
    switch (obj.type) {
        case msgpack::type::NIL:
            doc.append(kvp(key, b_null{}));
            break;
            
        case msgpack::type::BOOLEAN:
            doc.append(kvp(key, obj.as<bool>()));
            break;
            
        case msgpack::type::POSITIVE_INTEGER:
            doc.append(kvp(key, static_cast<int64_t>(obj.as<uint64_t>())));
            break;
            
        case msgpack::type::NEGATIVE_INTEGER:
            doc.append(kvp(key, obj.as<int64_t>()));
            break;
            
        case msgpack::type::FLOAT32:
        case msgpack::type::FLOAT64:
            doc.append(kvp(key, obj.as<double>()));
            break;
            
        case msgpack::type::STR:
            doc.append(kvp(key, obj.as<std::string>()));
            break;
            
        case msgpack::type::BIN:
            {
                auto& bin = obj.via.bin;
                doc.append(kvp(key, b_binary{
                    bsoncxx::binary_sub_type::k_binary,
                    static_cast<uint32_t>(bin.size),
                    reinterpret_cast<const uint8_t*>(bin.ptr)
                }));
            }
            break;
            
        case msgpack::type::ARRAY:
            {
                auto arr_builder = array{};
                auto& arr = obj.via.array;
                
                for (uint32_t i = 0; i < arr.size; ++i) {
                    convertObject(arr.ptr[i], arr_builder);
                }
                
                doc.append(kvp(key, arr_builder));
            }
            break;
            
        case msgpack::type::MAP:
            {
                auto subdoc = document{};
                convertMapToBson(obj, subdoc);
                doc.append(kvp(key, subdoc));
            }
            break;
            
        case msgpack::type::EXT:
            {
                auto& ext = obj.via.ext;
                // Store extension as binary with type info
                auto ext_doc = document{};
                ext_doc.append(kvp("type", static_cast<int32_t>(ext.type())));
                ext_doc.append(kvp("data", b_binary{
                    bsoncxx::binary_sub_type::k_binary,
                    static_cast<uint32_t>(ext.size),
                    reinterpret_cast<const uint8_t*>(ext.data())
                }));
                doc.append(kvp(key, ext_doc));
            }
            break;
            
        default:
            doc.append(kvp(key, b_null{}));
            break;
    }
}

void MsgPackToBsonConverter::convertObject(const msgpack::object& obj, array& arr) {
    switch (obj.type) {
        case msgpack::type::NIL:
            arr.append(b_null{});
            break;
        case msgpack::type::BOOLEAN:
            arr.append(obj.as<bool>());
            break;
        case msgpack::type::POSITIVE_INTEGER:
            arr.append(static_cast<int64_t>(obj.as<uint64_t>()));
            break;
        case msgpack::type::NEGATIVE_INTEGER:
            arr.append(obj.as<int64_t>());
            break;
        case msgpack::type::FLOAT32:
        case msgpack::type::FLOAT64:
            arr.append(obj.as<double>());
            break;
        case msgpack::type::STR:
            arr.append(obj.as<std::string>());
            break;
        case msgpack::type::BIN:
            {
                auto& bin = obj.via.bin;
                arr.append(b_binary{
                    bsoncxx::binary_sub_type::k_binary,
                    static_cast<uint32_t>(bin.size),
                    reinterpret_cast<const uint8_t*>(bin.ptr)
                });
            }
            break;
        case msgpack::type::ARRAY:
            {
                auto nested_arr = array{};
                auto& nested = obj.via.array;
                for (uint32_t i = 0; i < nested.size; ++i) {
                    convertObject(nested.ptr[i], nested_arr);
                }
                arr.append(nested_arr);
            }
            break;
        case msgpack::type::MAP:
            {
                auto subdoc = document{};
                convertMapToBson(obj, subdoc);
                arr.append(subdoc);
            }
            break;
        case msgpack::type::EXT:
            {
                auto& ext = obj.via.ext;
                auto ext_doc = document{};
                ext_doc.append(kvp("type", static_cast<int32_t>(ext.type())));
                ext_doc.append(kvp("data", b_binary{
                    bsoncxx::binary_sub_type::k_binary,
                    static_cast<uint32_t>(ext.size),
                    reinterpret_cast<const uint8_t*>(ext.data())
                }));
                arr.append(ext_doc);
            }
            break;
        default:
            arr.append(b_null{});
            break;
    }
}

std::optional<std::string> MsgPackToBsonConverter::extractPvName(const std::vector<uint8_t>& msgpack_data) {
    try {
        msgpack::object_handle oh = msgpack::unpack(
            reinterpret_cast<const char*>(msgpack_data.data()), 
            msgpack_data.size()
        );
        
        const msgpack::object& root = oh.get();
        
        if (root.type == msgpack::type::MAP) {
            // Look for PV name in map keys
            auto& map = root.via.map;
            for (uint32_t i = 0; i < map.size; ++i) {
                if (map.ptr[i].key.type == msgpack::type::STR) {
                    return map.ptr[i].key.as<std::string>();
                }
            }
        } else if (root.type == msgpack::type::ARRAY && root.via.array.size > 0) {
            // Compact format: first element is PV name
            if (root.via.array.ptr[0].type == msgpack::type::STR) {
                return root.via.array.ptr[0].as<std::string>();
            }
        }
        
        return std::nullopt;
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::chrono::system_clock::time_point> MsgPackToBsonConverter::extractEpicsTimestamp(const std::vector<uint8_t>& msgpack_data) {
    try {
        msgpack::object_handle oh = msgpack::unpack(
            reinterpret_cast<const char*>(msgpack_data.data()), 
            msgpack_data.size()
        );
        
        const msgpack::object& root = oh.get();
        
        if (root.type == msgpack::type::MAP) {
            auto& map = root.via.map;
            
            // Look for timestamp in EPICS structure
            for (uint32_t i = 0; i < map.size; ++i) {
                if (map.ptr[i].key.type == msgpack::type::STR) {
                    std::string key = map.ptr[i].key.as<std::string>();
                    
                    // Check for timeStamp object in EPICS data structure
                    if (key.find("timeStamp") != std::string::npos && map.ptr[i].val.type == msgpack::type::MAP) {
                        return parseEpicsTimestamp(map.ptr[i].val);
                    }
                    
                    // Also check if this is a PV map and look inside the PV data
                    if (map.ptr[i].val.type == msgpack::type::MAP) {
                        auto pv_timestamp = searchTimestampInPvData(map.ptr[i].val);
                        if (pv_timestamp) {
                            return pv_timestamp;
                        }
                    }
                }
            }
        }
        
        return std::nullopt;
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::chrono::system_clock::time_point> MsgPackToBsonConverter::parseEpicsTimestamp(const msgpack::object& timestamp_obj) {
    if (timestamp_obj.type != msgpack::type::MAP) return std::nullopt;
    
    auto& map = timestamp_obj.via.map;
    std::optional<int64_t> seconds;
    std::optional<int64_t> nanoseconds;
    
    for (uint32_t i = 0; i < map.size; ++i) {
        if (map.ptr[i].key.type == msgpack::type::STR) {
            std::string key = map.ptr[i].key.as<std::string>();
            
            if (key == "secondsPastEpoch" && 
                (map.ptr[i].val.type == msgpack::type::POSITIVE_INTEGER || 
                 map.ptr[i].val.type == msgpack::type::NEGATIVE_INTEGER)) {
                seconds = map.ptr[i].val.as<int64_t>();
            } else if (key == "nanoseconds" && 
                      (map.ptr[i].val.type == msgpack::type::POSITIVE_INTEGER || 
                       map.ptr[i].val.type == msgpack::type::NEGATIVE_INTEGER)) {
                nanoseconds = map.ptr[i].val.as<int64_t>();
            }
        }
    }
    
    if (seconds) {
        auto tp = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(*seconds));
        if (nanoseconds) {
            tp += std::chrono::nanoseconds(*nanoseconds);
        }
        return tp;
    }
    
    return std::nullopt;
}

std::optional<std::chrono::system_clock::time_point> MsgPackToBsonConverter::searchTimestampInPvData(const msgpack::object& pv_obj) {
    if (pv_obj.type != msgpack::type::MAP) return std::nullopt;
    
    auto& map = pv_obj.via.map;
    
    for (uint32_t i = 0; i < map.size; ++i) {
        if (map.ptr[i].key.type == msgpack::type::STR) {
            std::string key = map.ptr[i].key.as<std::string>();
            
            if (key == "timeStamp" && map.ptr[i].val.type == msgpack::type::MAP) {
                return parseEpicsTimestamp(map.ptr[i].val);
            }
        }
    }
    
    return std::nullopt;
}