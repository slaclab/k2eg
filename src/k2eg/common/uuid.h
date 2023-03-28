#ifndef UUID_H
#define UUID_H

#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.
#include <boost/lexical_cast.hpp>

namespace k2eg
{
    namespace common
    {

        /*
         Class for UUID managment
         */
        class UUID
        {

        public:
            /*
             Return the first part(of the four) of a random UUID
             */
            static std::string generateUUIDLite()
            {
                boost::uuids::uuid _uuid = boost::uuids::random_generator()();
                std::string str_uuid = boost::lexical_cast<std::string>(_uuid);
                size_t foundFirstSegment = str_uuid.find("-");
                return (foundFirstSegment != std::string::npos) ? str_uuid.substr(0, foundFirstSegment) : str_uuid;
            }

            /*
             Return the first part(of the four) of a random UUID
             */
            static std::string generateUUID()
            {
                boost::uuids::uuid _uuid = boost::uuids::random_generator()();
                return boost::lexical_cast<std::string>(_uuid);
            }
        };
    }
}
#endif
