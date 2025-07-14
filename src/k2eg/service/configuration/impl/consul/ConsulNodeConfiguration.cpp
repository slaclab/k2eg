#include "oatpp/core/data/share/MemoryLabel.hpp"
#include <k2eg/common/utility.h>

#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>

#include <oatpp/core/base/Environment.hpp>
#include <oatpp/network/tcp/client/ConnectionProvider.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>
#include <oatpp/web/client/HttpRequestExecutor.hpp>
#include <oatpp/web/protocol/http/outgoing/Request.hpp>

#include <memory>

using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

using namespace oatpp::base;
using namespace oatpp::data::mapping;
using namespace oatpp::network;
using namespace oatpp::network::tcp::client;

using namespace oatpp::consul;

ConsulNodeConfiguration::ConsulNodeConfiguration(ConstConfigurationServceiConfigUPtr _config)
    : INodeConfiguration(std::move(_config))
{
    // Initialize Oat++ environment.
    oatpp::base::Environment::init();

    oatpp::String hostname = oatpp::String(config->config_server_host);
    v_uint16      port = config->config_server_port;
    // Create a TCP connection provider and an HTTP request executor for Consul.
    auto connectionProvider = oatpp::network::tcp::client::ConnectionProvider::createShared({hostname, port, oatpp::network::Address::Family::IP_4});
    auto requestExecutor = oatpp::web::client::HttpRequestExecutor::createShared(connectionProvider);

    // Create the Consul client instance.
    client = Client::createShared(requestExecutor);
    if (!client)
    {
        throw std::runtime_error("Failed to create Consul client");
    }

    // Create a session for distributed locking
    if (!createSession())
    {
        throw std::runtime_error("Failed to create Consul session");
    }

    // Start session renewal thread
    session_active = true;
    session_renewal_thread = std::thread(&ConsulNodeConfiguration::renewSession, this);

    // compose the configuration key
    node_configuration_key = getNodeKey();

    if (config->reset_on_start)
    {
        // remove the old configuration
        client->kvDelete(node_configuration_key);
    }
    // in case this is the first time we are running check if we have a configuration
    // and if not create a new one
    try
    {
        auto json_value = client->kvGet(node_configuration_key);
    }
    catch (Client::Error& ex)
    {
        // create a new configuration
        setNodeConfiguration(std::make_shared<NodeConfiguration>());
    }
};

ConsulNodeConfiguration::~ConsulNodeConfiguration()
{
    // Stop session renewal
    session_active = false;
    if (session_renewal_thread.joinable())
    {
        session_renewal_thread.join();
    }

    // Destroy session
    destroySession();

    // Shutdown Oat++ environment.
    oatpp::base::Environment::destroy();
}

bool ConsulNodeConfiguration::createSession()
{
    try
    {
        // Create session request body as JSON string manually
        std::string sessionJson = STRING_FORMAT(
            R"({"Name": "k2eg-gateway-%1%", "TTL": 30, "Behavior": "release"})",
            getNodeName());

        auto sessionJsonStr = oatpp::String(sessionJson);
        auto requestBody = std::make_shared<oatpp::web::protocol::http::outgoing::BufferBody>(
            sessionJsonStr, 
            oatpp::data::share::StringKeyLabel("application/json")
        );
        // Make HTTP PUT request to create session
        auto headers = oatpp::web::protocol::http::Headers();
        headers.put("Content-Type", "application/json");

        auto response = requestExecutor->execute("PUT", "/v1/session/create", headers, requestBody, nullptr);

        if (response->getStatusCode() == 200)
        {
            auto responseBody = response->readBodyToString();
            if (responseBody)
            {
                std::string responseStr = responseBody;

                // Parse JSON response manually to extract session ID
                size_t idStart = responseStr.find("\"ID\":\"");
                if (idStart != std::string::npos)
                {
                    idStart += 6; // Skip "ID":"
                    size_t idEnd = responseStr.find("\"", idStart);
                    if (idEnd != std::string::npos)
                    {
                        session_id = responseStr.substr(idStart, idEnd - idStart);
                        return !session_id.empty();
                    }
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        // Log error
    }
    return false;
}

void ConsulNodeConfiguration::renewSession()
{
    while (session_active)
    {
        try
        {
            std::this_thread::sleep_for(std::chrono::seconds(10)); // Renew every 10 seconds

            if (session_active && !session_id.empty())
            {
                // Make HTTP PUT request to renew session
                auto        headers = oatpp::web::protocol::http::Headers();
                std::string url = STRING_FORMAT("/v1/session/renew/%1%", session_id);

                auto response = requestExecutor->execute("PUT", url, headers, nullptr, nullptr);

                if (response->getStatusCode() != 200)
                {
                    // Session renewal failed, try to recreate
                    if (session_active)
                    {
                        session_id.clear();
                        createSession();
                    }
                }
            }
        }
        catch (const std::exception& ex)
        {
            // Session renewal failed, try to recreate
            if (session_active)
            {
                session_id.clear();
                createSession();
            }
        }
    }
}

bool ConsulNodeConfiguration::destroySession()
{
    if (!session_id.empty())
    {
        try
        {
            // Make HTTP PUT request to destroy session
            auto        headers = oatpp::web::protocol::http::Headers();
            std::string url = STRING_FORMAT("/v1/session/destroy/%1%", session_id);

            auto response = requestExecutor->execute("PUT", url, headers, nullptr, nullptr);

            session_id.clear();
            return response->getStatusCode() == 200;
        }
        catch (const std::exception& ex)
        {
            // Log error but don't fail
        }
    }
    return false;
}

const std::vector<std::string> ConsulNodeConfiguration::kvGetKeys(const std::string& prefix) const
{
    std::vector<std::string> keys;
    try
    {
        auto        headers = oatpp::web::protocol::http::Headers();
        std::string url = STRING_FORMAT("/v1/kv/%1%?keys", prefix);
        auto        response = requestExecutor->execute("GET", url, headers, nullptr, nullptr);

        if (response && response->getStatusCode() == 200)
        {
            auto responseBody = response->readBodyToString();
            if (responseBody)
            {
                // Consul returns a JSON array of strings
                std::string bodyStr = responseBody.getValue("");
                // Very simple JSON array parsing (no external dependency)
                size_t pos = 0;
                while ((pos = bodyStr.find("\"")) != std::string::npos)
                {
                    size_t end = bodyStr.find("\"", pos + 1);
                    if (end == std::string::npos)
                        break;
                    keys.push_back(bodyStr.substr(pos + 1, end - pos - 1));
                    bodyStr = bodyStr.substr(end + 1);
                }
            }
        }
    }
    catch (const std::exception&)
    {
        // Ignore errors, return what we have
    }
    return keys;
}

std::string ConsulNodeConfiguration::getNodeKey() const
{
    char hostname[HOST_NAME_MAX];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        return STRING_FORMAT("k2eg-node/%1%/configuration", std::string(hostname));
    }
    const char* envHostname = std::getenv("HOSTNAME");
    if (envHostname == nullptr)
    {
        throw std::runtime_error("Failed to get hostname");
    }
    return STRING_FORMAT("k2eg-node/%1%/configuration", std::string(envHostname));
}

NodeConfigurationShrdPtr ConsulNodeConfiguration::getNodeConfiguration() const
{
    oatpp::String json_value = client->kvGet(node_configuration_key);
    if (json_value == nullptr)
    {
        return nullptr;
    }
    auto json_str = json_value.getValue("");
    try
    {
        // Parse the JSON string using Boost.JSON.
        boost::json::value  parsed = boost::json::parse(json_str);
        boost::json::object obj = parsed.as_object();
        return config_from_json(obj);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(STRING_FORMAT("Failed to parse JSON: %1%", ex.what()));
    }
}

bool ConsulNodeConfiguration::setNodeConfiguration(NodeConfigurationShrdPtr node_configuration)
{
    // store configuration in Consul KV store
    auto json_obj = config_to_json(*node_configuration);
    auto json_str = boost::json::serialize(json_obj);
    auto res = client->kvPut(node_configuration_key, json_str);
    return res;
}

std::string ConsulNodeConfiguration::getNodeName() const
{
    char hostname[HOST_NAME_MAX];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        return std::string(hostname);
    }
    const char* envHostname = std::getenv("HOSTNAME");
    if (envHostname == nullptr)
    {
        return "Failed to get hostname";
    }
    return getNodeKey();
}

const std::string ConsulNodeConfiguration::getSnapshotKey(const std::string& snapshot_id) const
{
    return STRING_FORMAT("k2eg/snapshots/%1%", snapshot_id);
}

ConstSnapshotConfigurationShrdPtr ConsulNodeConfiguration::getSnapshotConfiguration(const std::string& snapshot_id) const
{
    std::string base_key = getSnapshotKey(snapshot_id);
    try
    {
        auto weight_str = client->kvGet(base_key + "/weight");
        auto weight_unit_str = client->kvGet(base_key + "/weight_unit");
        auto gateway_id_str = client->kvGet(base_key + "/gateway_id");
        auto running_status_str = client->kvGet(base_key + "/running_status");
        auto archiving_status_str = client->kvGet(base_key + "/archiving_status");
        auto archiver_id_str = client->kvGet(base_key + "/archiver_id");
        auto timestamp_str = client->kvGet(base_key + "/timestamp");

        auto snapshot_config = std::make_shared<SnapshotConfiguration>();
        snapshot_config->snapshot_id = snapshot_id;
        if (weight_str)
            snapshot_config->weight = std::stoi(weight_str);
        if (weight_unit_str)
            snapshot_config->weight_unit = weight_unit_str.getValue("");
        if (gateway_id_str)
            snapshot_config->gateway_id = gateway_id_str.getValue("");
        if (running_status_str)
            snapshot_config->running_status = (running_status_str.getValue("false") == "true");
        if (archiving_status_str)
            snapshot_config->archiving_status = (archiving_status_str.getValue("false") == "true");
        if (archiver_id_str)
            snapshot_config->archiver_id = archiver_id_str.getValue("");
        if (timestamp_str)
            snapshot_config->timestamp = timestamp_str.getValue("");
        return snapshot_config;
    }
    catch (const Client::Error&)
    {
        return nullptr;
    }
}

bool ConsulNodeConfiguration::setSnapshotConfiguration(SnapshotConfigurationShrdPtr snapshot_config)
{
    std::string base_key = getSnapshotKey(snapshot_config->snapshot_id);
    try
    {
        bool success = true;
        success &= client->kvPut(base_key + "/weight", std::to_string(snapshot_config->weight));
        success &= client->kvPut(base_key + "/weight_unit", snapshot_config->weight_unit);
        success &= client->kvPut(base_key + "/gateway_id", snapshot_config->gateway_id);
        success &= client->kvPut(base_key + "/running_status", snapshot_config->running_status ? "true" : "false");
        success &= client->kvPut(base_key + "/archiving_status", snapshot_config->archiving_status ? "true" : "false");
        success &= client->kvPut(base_key + "/archiver_id", snapshot_config->archiver_id);
        success &= client->kvPut(base_key + "/timestamp", snapshot_config->timestamp);
        return success;
    }
    catch (const Client::Error&)
    {
        return false;
    }
}

bool ConsulNodeConfiguration::deleteSnapshotConfiguration(const std::string& snapshot_id)
{
    std::string base_key = getSnapshotKey(snapshot_id);
    try
    {
        client->kvDelete(base_key + "/"); // true = recurse
        return true;
    }
    catch (const Client::Error&)
    {
        return false;
    }
}

const std::vector<std::string> ConsulNodeConfiguration::getSnapshotIds() const
{
    std::vector<std::string> snapshot_ids;
    try
    {
        auto keys = kvGetKeys("k2eg/snapshots/");
        for (const auto& key_str : keys)
        {
            // Extract snapshot_id from key path: k2eg/snapshots/<snapshot_id>/...
            auto prefix = std::string("k2eg/snapshots/");
            if (key_str.compare(0, prefix.size(), prefix) == 0)
            {
                auto rest = key_str.substr(prefix.size());
                auto slash_pos = rest.find('/');
                if (slash_pos != std::string::npos)
                {
                    std::string snapshot_id = rest.substr(0, slash_pos);
                    if (std::find(snapshot_ids.begin(), snapshot_ids.end(), snapshot_id) == snapshot_ids.end())
                        snapshot_ids.push_back(snapshot_id);
                }
            }
        }
    }
    catch (...)
    {
        // Return empty vector on error
    }
    return snapshot_ids;
}

bool ConsulNodeConfiguration::updateSnapshotField(const std::string& snapshot_id, const std::string& field, const std::string& value)
{
    std::string key = getSnapshotKey(snapshot_id) + "/" + field;
    try
    {
        return client->kvPut(key, value);
    }
    catch (const Client::Error&)
    {
        return false;
    }
}

const std::string ConsulNodeConfiguration::getSnapshotField(const std::string& snapshot_id, const std::string& field) const
{
    std::string key = getSnapshotKey(snapshot_id) + "/" + field;
    try
    {
        auto value = client->kvGet(key);
        return value ? value.getValue("") : "";
    }
    catch (const Client::Error&)
    {
        return "";
    }
}

// Distributed snapshot management methods

bool ConsulNodeConfiguration::isSnapshotRunning(const std::string& snapshot_id) const
{
    try
    {
        auto running_status = client->kvGet(getSnapshotKey(snapshot_id) + "/running_status");
        return running_status && (running_status.getValue("false") == "true");
    }
    catch (const Client::Error&)
    {
        return false;
    }
}

const std::string ConsulNodeConfiguration::getSnapshotGateway(const std::string& snapshot_id) const
{
    try
    {
        auto gateway_id = client->kvGet(getSnapshotKey(snapshot_id) + "/gateway_id");
        return gateway_id ? gateway_id.getValue("") : "";
    }
    catch (const Client::Error&)
    {
        return "";
    }
}

bool ConsulNodeConfiguration::tryAcquireSnapshot(const std::string& snapshot_id, const std::string& gateway_id)
{
    if (session_id.empty())
        return false;

    std::string lock_key = getSnapshotKey(snapshot_id) + "/lock";
    std::string base_key = getSnapshotKey(snapshot_id);

    try
    {
        // Try to acquire lock using session - HTTP PUT with acquire parameter
        auto headers = oatpp::web::protocol::http::Headers();
        std::string url = STRING_FORMAT("/v1/kv/%1%?acquire=%2%", lock_key % session_id);
        auto body = std::make_shared<oatpp::web::protocol::http::outgoing::BufferBody>(
            gateway_id,
            oatpp::data::share::StringKeyLabel("application/json")
        );

        auto response = requestExecutor->execute("PUT", url, headers, body, nullptr);

        if (response && response->getStatusCode() == 200)
        {
            auto responseBody = response->readBodyToString();
            if (responseBody && responseBody.getValue("") == "true")
            {
                // Lock acquired, set snapshot as running and assign gateway
                bool success = true;
                success &= client->kvPut(base_key + "/gateway_id", gateway_id);
                success &= client->kvPut(base_key + "/running_status", "true");
                // Optionally set timestamp or other fields here
                return success;
            }
        }
    }
    catch (const std::exception&)
    {
        // Log error if needed
    }
    return false;
}

bool ConsulNodeConfiguration::releaseSnapshot(const std::string& snapshot_id, const std::string& gateway_id)
{
    if (session_id.empty())
        return false;

    std::string lock_key = getSnapshotKey(snapshot_id) + "/lock";
    std::string base_key = getSnapshotKey(snapshot_id);

    try
    {
        std::string current_gateway = getSnapshotGateway(snapshot_id);
        if (current_gateway != gateway_id)
            return false;

        bool success = true;
        success &= client->kvPut(base_key + "/running_status", "false");

        auto        headers = oatpp::web::protocol::http::Headers();
        std::string url = STRING_FORMAT("/v1/kv/%1%?release=%2%", lock_key % session_id);

        auto response = requestExecutor->execute("PUT", url, headers, nullptr, nullptr);

        return success && (response->getStatusCode() == 200);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

const std::vector<std::string> ConsulNodeConfiguration::getRunningSnapshots() const
{
    std::vector<std::string> running_snapshots;
    try
    {
        auto all_snapshots = getSnapshotIds();
        for (const auto& snapshot_id : all_snapshots)
        {
            if (isSnapshotRunning(snapshot_id))
                running_snapshots.push_back(snapshot_id);
        }
    }
    catch (const Client::Error&)
    {
        // Return empty vector on error
    }
    return running_snapshots;
}

const std::vector<std::string> ConsulNodeConfiguration::getSnapshotsByGateway(const std::string& gateway_id) const
{
    std::vector<std::string> gateway_snapshots;
    try
    {
        auto all_snapshots = getSnapshotIds();
        for (const auto& snapshot_id : all_snapshots)
        {
            std::string snapshot_gateway = getSnapshotGateway(snapshot_id);
            if (snapshot_gateway == gateway_id && isSnapshotRunning(snapshot_id))
                gateway_snapshots.push_back(snapshot_id);
        }
    }
    catch (const Client::Error&)
    {
        // Return empty vector on error
    }
    return gateway_snapshots;
}