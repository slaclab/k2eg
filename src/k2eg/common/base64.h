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
        try
        {
            if (data.empty())
            {
                return "";
            }

            std::string encoded(base64::encoded_size(data.size()), '\0');
            auto        len = base64::encode(encoded.data(), data.data(), data.size());
            encoded.resize(len);
            return encoded;
        }
        catch (const std::exception&)
        {
            return ""; // Return empty string on encoding failure
        }
    }

    static std::string encode(const std::string& data)
    {
        try
        {
            if (data.empty())
            {
                return "";
            }

            std::vector<unsigned char> vec(data.begin(), data.end());
            return encode(vec);
        }
        catch (const std::exception&)
        {
            return ""; // Return empty string on encoding failure
        }
    }

    static std::vector<unsigned char> decode(const std::string& encoded)
    {
        std::vector<unsigned char> decoded;
        try
        {
            if (encoded.empty())
            {
                return decoded; // Return empty vector for empty input
            }

            // Validate input contains only valid base64 characters
            for (char c : encoded)
            {
                if (!isValidBase64Char(c))
                {
                    return decoded; // Return empty vector for invalid input
                }
            }

            auto decoded_size = base64::decoded_size(encoded.size());
            if (decoded_size <= 0)
            {
                return decoded; // Return empty vector if decoding size is invalid
            }

            decoded.resize(decoded_size);
            auto result = base64::decode(decoded.data(), encoded.data(), encoded.size());

            // Check if decoding was successful
            if (result.second != 0)
            {
                decoded.clear(); // Clear and return empty on decode error
                return decoded;
            }

            decoded.resize(result.first);
            return decoded;
        }
        catch (const std::exception&)
        {
            decoded.clear();
            return decoded; // Return empty vector on any exception
        }
    }

    static std::string decodeToString(const std::string& encoded)
    {
        try
        {
            auto decoded_vec = decode(encoded);
            if (decoded_vec.empty())
            {
                return "";
            }
            return std::string(decoded_vec.begin(), decoded_vec.end());
        }
        catch (const std::exception&)
        {
            return ""; // Return empty string on decoding failure
        }
    }

private:
    static bool isValidBase64Char(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
    }
};

} // namespace k2eg::common

#endif // K2EG_COMMON_BASE64_H_