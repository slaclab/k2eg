#ifndef K2EG_SERVICE_EPICS_MSGPACKEPICSCONVERTER_H_
#define K2EG_SERVICE_EPICS_MSGPACKEPICSCONVERTER_H_

#include <msgpack.hpp>
#include <pv/pvData.h>

namespace k2eg::service::epics_impl {
class MsgpackEpicsConverter
{
    static void packField(msgpack::packer<msgpack::sbuffer>& pk, const epics::pvData::PVFieldPtr& field);
    static epics::pvData::PVFieldPtr unpackMsgpackToPVField(const msgpack::object& obj, const epics::pvData::FieldConstPtr& field);

public:
    /**
     * Serialize an EPICS PVStructure (including all nested fields and arrays) into msgpack format using the provided msgpack packer.
     *
     * Handles all scalar, scalar array, structure, structure array, union, and union array fields recursively.
     * If a field is missing or null, it is packed as nil in msgpack.
     *
     * Special Handling for Major EPICS Structures:
     *
     * NTTable:
     *   - Serialized as a map with keys for each column and a 'labels' array.
     *   - Each column is packed as an array of values.
     *   - Example JSON visualization:
     *     {
     *       "labels": ["col1", "col2"],
     *       "col1": [1, 2, 3],
     *       "col2": ["a", "b", "c"]
     *     }
     *
     * NTNDArray:
     *   - Serialized as a map with keys for all standard fields: value, compressedSize, uncompressedSize, uniqueId, timeStamp, dimension, attribute, etc.
     *   - The 'value' field (which may be a union or array) is packed as an array or nil if empty.
     *   - The 'dimension' field is packed as an array of dimension objects.
     *   - The 'attribute' field is packed as an array of attribute objects, each with its own fields.
     *   - Example JSON visualization:
     *     {
     *       "value": [array of numbers or nil ],
     *       "compressedSize": 0,
     *       "uncompressedSize": 0,
     *       "uniqueId": 0,
     *       "timeStamp": { "secondsPastEpoch": 1749328744, "nanoseconds": 322463274 },
     *       "dimension": [
     *         {"size": 0, "offset": 0, "fullSize": 0, "binning": 1, "reverse": false},
     *         {"size": 0, "offset": 0, "fullSize": 0, "binning": 1, "reverse": false}
     *       ],
     *       "attribute": [
     *         {
     *           "name": "ColorMode",
     *           "value": 0,
     *           "tags": [],
     *           "descriptor": "",
     *           "alarm": {"severity": 0, "status": 0, "message": ""},
     *           "timeStamp": {"secondsPastEpoch": 0, "nanoseconds": 0, "userTag": 0},
     *           "sourceType": 0,
     *           "source": ""
     *         }
     *       ]
     *     }
     *
     * Waveform:
     *   - Typically a scalar array field (e.g., double[] value).
     *   - Serialized as a map with a single key (e.g., 'value') and the array of values.
     *   - Example JSON visualization:
     *     {
     *       "value": [1.23, 4.56, 7.89]
     *     }
     *
     * The actual msgpack output is binary, but the JSON visualizations above show the logical structure as it would appear after decoding.
     * The converter is robust to missing or null fields, always packing nil where appropriate.
     * All field names and values are preserved, and nested structures are handled recursively.
     */
    static void epicsToMsgpack(const epics::pvData::PVStructurePtr& pvStruct, msgpack::packer<msgpack::sbuffer>& pk);
    static epics::pvData::PVStructurePtr msgpackToEpics(const msgpack::object& obj, const epics::pvData::StructureConstPtr& schema);
};
} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_MSGPACKEPICSCONVERTER_H_