#ifndef K2EG_COMMON_BASE64_H_
#define K2EG_COMMON_BASE64_H_

#include <boost/beast/core/detail/base64.hpp>
#include <string>
#include <vector>

namespace k2eg::common {

namespace base64 = boost::beast::detail::base64;

class Base64
{
public:
    static std::string encode(const std::vector<unsigned char>& data)
    {
        std::string encoded(base64::encoded_size(data.size()), '\0');
        auto        len = base64::encode(encoded.data(), data.data(), data.size());
        encoded.resize(len);
        return encoded;
    }

    static std::vector<unsigned char> decode(const std::string& encoded)
    {
        std::vector<unsigned char> decoded(base64::decoded_size(encoded.size()));
        auto                       len = base64::decode(decoded.data(), encoded.data(), encoded.size());
        decoded.resize(len.first);
        return decoded;
    }
};

} // namespace k2eg::common

#endif // K2EG_COMMON_BASE64_H_