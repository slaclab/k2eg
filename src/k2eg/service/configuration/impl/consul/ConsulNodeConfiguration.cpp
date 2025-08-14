
#include <k2eg/common/utility.h>
#include <k2eg/common/Base64.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>
#include <k2eg/service/log/ILogger.h>

#include <oatpp/core/base/Environment.hpp>
#include <oatpp/core/data/share/MemoryLabel.hpp>
#include <oatpp/network/tcp/client/ConnectionProvider.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>
#include <oatpp/web/client/HttpRequestExecutor.hpp>
#include <oatpp/web/protocol/http/outgoing/Request.hpp>

#include <memory>
#include <stdexcept>
#include <ctime>


using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

using namespace oatpp::base;
using namespace oatpp::data::mapping;
using namespace oatpp::network;
using namespace oatpp::network::tcp::client;

using namespace oatpp::consul;

ConsulNodeConfiguration::ConsulNodeConfiguration(ConstConfigurationServiceConfigUPtr _config)
    : INodeConfiguration(std::move(_config))
{
    // Initialize Oat++ environment.
    oatpp::base::Environment::init();

    oatpp::String hostname = oatpp::String(config->config_server_host);
    v_uint16      port = config->config_server_port;
    // Create a TCP connection provider and an HTTP request executor for Consul.
    auto connectionProvider = oatpp::network::tcp::client::ConnectionProvider::createShared({hostname, port, oatpp::network::Address::Family::IP_4});
    requestExecutor = oatpp::web::client::HttpRequestExecutor::createShared(connectionProvider);

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
    {
        std::lock_guard<std::mutex> lock(session_mutex);
        session_active = false;
    }
    session_cv.notify_one();
    if (session_renewal_thread.joinable())
    {
        session_renewal_thread.join();
    }

    // Destroy session
    destroySession();

    // Shutdown Oat++ environment.
    oatpp::base::Environment::destroy();
}

void ConsulNodeConfiguration::registerService()
{
    /* get oatpp::consul::rest::Client */
    auto restClient = client->getRestClient();

    auto checkPayload = oatpp::consul::rest::AgentCheckRegisterPayload::createShared();
    checkPayload->id = "service_check_id";
    checkPayload->name = "service_check_name";
    checkPayload->notes = "Check on the MyService/Health endpoint";
    // checkPayload->http = "http://localhost:8000/check/health";
    // checkPayload->method = "GET";
    // checkPayload->interval = "30s";
    // checkPayload->timeout = "15s";

    auto payload = oatpp::consul::rest::AgentServiceRegisterPayload::createShared();
    payload->id = getNodeName();
    payload->name = "service_name";

    /* make API call */
    auto response = restClient->agentServiceRegister(payload);
    if (response->getStatusCode() != 200)
    {
        // Service registration failed
        throw std::runtime_error("Failed to register service with Consul");
    }
}

void ConsulNodeConfiguration::deregisterService()
{
    auto restClient = client->getRestClient();
    auto response = restClient->agentServiceDeregister(getNodeName());
    if (response->getStatusCode() != 200)
    {
        // Service registration failed
        throw std::runtime_error("Failed to register service with Consul");
    }
}

bool ConsulNodeConfiguration::createSession()
{
    try
    {
        // Create session request body as JSON string manually
        std::string node_name = getNodeName(); // Should match a registered Consul node
        std::string sessionJson = STRING_FORMAT(
            R"({"Name": "k2eg-gateway-%1%", "Node": "%1%", "TTL": "30s", "Behavior": "delete"})",
            node_name);

        auto sessionJsonStr = oatpp::String(sessionJson);
        auto requestBody = std::make_shared<oatpp::web::protocol::http::outgoing::BufferBody>(
            sessionJsonStr,
            oatpp::data::share::StringKeyLabel("application/json"));
        // Make HTTP PUT request to create session
        auto headers = oatpp::web::protocol::http::Headers();
        headers.put("Content-Type", "application/json");

        auto response = requestExecutor->execute("PUT", "v1/session/create", headers, requestBody, nullptr);
        auto statusCode = response->getStatusCode();
        if (statusCode == 200)
        {
            auto responseBody = response->readBodyToString();
            // use boost to decode json response
            boost::json::value parsedResponse = boost::json::parse(responseBody.getValue(""));
            if (parsedResponse.is_object())
            {
                auto obj = parsedResponse.as_object();
                if (obj.contains("ID") && obj["ID"].is_string())
                {
                    session_id = obj["ID"].as_string();
                    return !session_id.empty();
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
    std::unique_lock<std::mutex> lock(session_mutex);
    while (session_active)
    {
        try
        {
            session_cv.wait_for(lock, std::chrono::seconds(15), [this]
                                {
                                    return !session_active;
                                });

            if (session_active && !session_id.empty())
            {
                // Make HTTP PUT request to renew session
                auto        headers = oatpp::web::protocol::http::Headers();
                std::string url = STRING_FORMAT("v1/session/renew/%1%", session_id);

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
            std::string url = STRING_FORMAT("v1/session/destroy/%1%", session_id);

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
        std::string url = STRING_FORMAT("v1/kv/%1%?keys", prefix);
        auto        response = requestExecutor->execute("GET", url, headers, nullptr, nullptr);
        int         statusCode = response->getStatusCode();
        if (statusCode == 200)
        {
            auto responseBody = response->readBodyToString();
            if (responseBody)
            {
                // convert to boost json
                boost::json::value parsedResponse = boost::json::parse(responseBody.getValue(""));
                if (parsedResponse.is_array())
                {
                    auto arr = parsedResponse.as_array();
                    for (const auto& item : arr)
                    {
                        if (item.is_string())
                        {
                            keys.push_back(std::string(item.as_string()));
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& err)
    {
    }
    return keys;
}

std::string ConsulNodeConfiguration::getNodeKey() const
{
    char hostname[HOST_NAME_MAX];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        return STRING_FORMAT("k2eg/nodes/%1%/configuration", std::string(hostname));
    }
    const char* envHostname = std::getenv("HOSTNAME");
    if (envHostname == nullptr)
    {
        throw std::runtime_error("Failed to get hostname");
    }
    return STRING_FORMAT("k2eg/nodes/%1%/configuration", std::string(envHostname));
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
        return std::make_shared<NodeConfiguration>(NodeConfiguration::fromJson(json_str));
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(STRING_FORMAT("Failed to parse JSON: %1%", ex.what()));
    }
}

bool ConsulNodeConfiguration::setNodeConfiguration(NodeConfigurationShrdPtr node_configuration)
{
    // store configuration in Consul KV store
    auto res = client->kvPut(node_configuration_key, NodeConfiguration::toJson(*node_configuration));
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
        auto update_timestamp_str = client->kvGet(base_key + "/update_timestamp");
        auto config_json_str = client->kvGet(base_key + "/config_json");

        auto snapshot_config = std::make_shared<SnapshotConfiguration>();
        if (weight_str)
            snapshot_config->weight = std::stoi(weight_str);
        if (weight_unit_str)
            snapshot_config->weight_unit = weight_unit_str.getValue("");
        if (update_timestamp_str)
            snapshot_config->update_timestamp = update_timestamp_str.getValue("");
        if (config_json_str)
            snapshot_config->config_json = config_json_str.getValue("");
        return snapshot_config;
    }
    catch (const Client::Error& err)
    {
        // Return nullptr if any error occurs
        return nullptr;
    }
}

bool ConsulNodeConfiguration::setSnapshotConfiguration(const std::string& snapshot_id, SnapshotConfigurationShrdPtr snapshot_config)
{
    std::string base_key = getSnapshotKey(snapshot_id);
    try
    {
        client->kvPut(base_key + "/weight", std::to_string(snapshot_config->weight));
        client->kvPut(base_key + "/weight_unit", snapshot_config->weight_unit);
        client->kvPut(base_key + "/update_timestamp", snapshot_config->update_timestamp);
        client->kvPut(base_key + "/config_json", snapshot_config->config_json);
    }
    catch (const Client::Error& err)
    {
        return false;
    }
    return true;
}

bool ConsulNodeConfiguration::deleteSnapshotConfiguration(const std::string& snapshot_id)
{
    std::string snapshot_key = getSnapshotKey(snapshot_id) + "/";
    try
    {
        // Compose the URL for Consul's HTTP API with ?recurse to delete all keys under the prefix
        std::string url = STRING_FORMAT("v1/kv/%1%?recurse=true", snapshot_key);

        auto headers = oatpp::web::protocol::http::Headers();
        // No body needed for DELETE
        auto response = requestExecutor->execute("DELETE", url, headers, nullptr, nullptr);

        // Consul returns 200 OK if successful, 404 if nothing to delete
        int statusCode = response->getStatusCode();
        return statusCode == 200 || statusCode == 404;
    }
    catch (const std::exception& err)
    {
        // Optionally log error
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

// Distributed snapshot management methods

bool ConsulNodeConfiguration::isSnapshotRunning(const std::string& snapshot_id) const
{
    try
    {
        auto running_status = client->kvGet(getSnapshotKey(snapshot_id) + "/running_status");
        return running_status && (running_status.getValue("false") == "true");
    }
    catch (Client::Error& err)
    {
        return false;
    }
}

void ConsulNodeConfiguration::setSnapshotRunning(const std::string& snapshot_id, bool running)
{
    std::string base_key = getSnapshotKey(snapshot_id);
    std::string running_key = base_key + "/running_status";

    try
    {
        auto        headers = oatpp::web::protocol::http::Headers();
        std::string url = STRING_FORMAT("v1/kv/%1%", running_key);
        client->kvPut(running_key, running ? "true" : "false");
    }
    catch (const std::exception& err)
    {
        throw std::runtime_error(STRING_FORMAT("Failed to set snapshot running status: %1%", err.what()));
    }
}

bool ConsulNodeConfiguration::isSnapshotArchiveRequested(const std::string& snapshot_id) const
{
    try
    {
        auto archiving_status = client->kvGet(getSnapshotKey(snapshot_id) + "/archive/requested");
        return archiving_status && (archiving_status.getValue("false") == "true");
    }
    catch (Client::Error& err)
    {
        return false;
    }
}

void ConsulNodeConfiguration::setSnapshotArchiveRequested(const std::string& snapshot_id, bool archived)
{
    if (session_id.empty())
        throw std::runtime_error("Session ID is empty, cannot set snapshot archiving status");

    std::string base_key = getSnapshotKey(snapshot_id);
    std::string archive_requested_key = base_key + "/archive/requested";

    try
    {
        client->kvPut(archive_requested_key, archived ? "true" : "false");
    }
    catch (const std::exception& err)
    {
        throw std::runtime_error(STRING_FORMAT("Failed to set snapshot archiving status: %1%", err.what()));
    }
}

void ConsulNodeConfiguration::setSnapshotArchiveStatus(const std::string& snapshot_id, ArchiveStatusInfo status)
{
    std::string base_key = getSnapshotKey(snapshot_id);
    std::string archive_status_key = base_key + "/archive/status";
}

ArchiveStatusInfo ConsulNodeConfiguration::getSnapshotArchiveStatus(const std::string& snapshot_id) const
{
    std::string base_key = getSnapshotKey(snapshot_id);
    std::string archive_status_key = base_key + "/archive/status";
    return ArchiveStatusInfo();
}

const std::string ConsulNodeConfiguration::getSnapshotGateway(const std::string& snapshot_id) const
{
    try
    {
        std::string base_key = getSnapshotKey(snapshot_id);
        std::string lock_key = base_key + "/lock_gateway";
        auto        gateway_id = client->kvGet(lock_key);
        return gateway_id ? gateway_id.getValue("") : "";
    }
    catch (const Client::Error& err)
    {
        return "";
    }
}

bool ConsulNodeConfiguration::tryAcquireSnapshot(const std::string& snapshot_id, bool for_gateway)
{
    if (session_id.empty())
        return false;

    std::string base_key = getSnapshotKey(snapshot_id);
    std::string lock_key = base_key + "/lock_" + (for_gateway ? "gateway" : "storage");

    try
    {
        // Gateway path: keep existing simple acquire behavior
        if (for_gateway)
        {
            auto headers = oatpp::web::protocol::http::Headers();
            std::string url = STRING_FORMAT("v1/kv/%1%?acquire=%2%", lock_key % session_id);
            auto body = std::make_shared<oatpp::web::protocol::http::outgoing::BufferBody>(
                oatpp::String(getNodeName()),
                oatpp::data::share::StringKeyLabel("application/json"));

            auto response = requestExecutor->execute("PUT", url, headers, body, nullptr);
            if (response->getStatusCode() == 200)
            {
                auto responseBody = response->readBodyToString();
                boost::json::value parsedResponse = boost::json::parse(responseBody.getValue(""));
                return (parsedResponse.is_bool() && parsedResponse.get_bool());
            }
            return false;
        }

        // Storage path: build a Consul txn that atomically locks lock_storage (session-bound)
        // and writes durable archive/status/* subkeys.
        // Build ISO8601 UTC timestamp
        std::time_t t = std::time(nullptr);
        std::tm tm_utc{};
#if defined(_MSC_VER)
        gmtime_s(&tm_utc, &t);
#else
        gmtime_r(&t, &tm_utc);
#endif
        char tsbuf[64];
        std::strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
        std::string now_iso(tsbuf);

        // durable fields initial values
        const std::string owner = getNodeName();
        const std::string state = "ARCHIVING";
        const std::string started_at = now_iso;
        const std::string updated_at = now_iso;
        const std::string progress = std::to_string(-1);
        const std::string generation = std::to_string(1);
        const std::string archive_id = std::string();
        const std::string error_message = std::string();

        boost::json::array txn_ops;

        // 1) lock op on lock_storage (session-bound)
        {
            boost::json::object kv;
            boost::json::object meta;
            meta["Verb"] = "lock";
            meta["Key"] = lock_key;
            meta["Value"] = k2eg::common::Base64::encode(owner);
            meta["Session"] = session_id;
            kv["KV"] = meta;
            txn_ops.push_back(kv);
        }

        // helper to push a set (durable) op for status subkeys
        auto pushSet = [&](const std::string &subkey, const std::string &val) {
            boost::json::object kv;
            boost::json::object meta;
            meta["Verb"] = "set";
            meta["Key"] = STRING_FORMAT("%1%/archive/status/%2%", base_key % subkey);
            meta["Value"] = k2eg::common::Base64::encode(val);
            kv["KV"] = meta;
            txn_ops.push_back(kv);
        };

        pushSet("owner", owner);
        pushSet("state", state);
        pushSet("started_at", started_at);
        pushSet("updated_at", updated_at);
        pushSet("progress", progress);
        pushSet("generation", generation);
        pushSet("archive_id", archive_id);
        pushSet("error_message", error_message);

        // serialize txn payload
        boost::json::value txn_val = txn_ops;
        std::string txn_body = boost::json::serialize(txn_val);

        auto requestBody = std::make_shared<oatpp::web::protocol::http::outgoing::BufferBody>(
            oatpp::String(txn_body.c_str(), (v_buff_size)txn_body.size()),
            oatpp::data::share::StringKeyLabel("application/json"));

        auto headers = oatpp::web::protocol::http::Headers();
        headers.put("Content-Type", "application/json");

        auto response = requestExecutor->execute("PUT", "v1/txn", headers, requestBody, nullptr);
        if (response->getStatusCode() == 200)
        {
            // Transaction applied: we hold the lock and durable status fields were written.
            return true;
        }
    }
    catch (const std::exception& err)
    {
        // Optional: emit logger if available
        try {
            auto logger = ServiceResolver<k2eg::service::log::ILogger>::resolve();
            if (logger) logger->logMessage(STRING_FORMAT("tryAcquireSnapshot error: %1%", err.what()), log::LogLevel::ERROR);
        } catch (...) {}
    }
    return false;
}

bool ConsulNodeConfiguration::releaseSnapshot(const std::string& snapshot_id, bool for_gateway)
{
    if (session_id.empty())
        return false;

    std::string base_key = getSnapshotKey(snapshot_id);
    std::string lock_key = base_key + "/lock_" + (for_gateway ? "gateway" : "storage");

    try
    {
        std::string current_gateway = getSnapshotGateway(snapshot_id);
        if (current_gateway != getNodeName())
            return false;

        client->kvDelete(lock_key); // Clear running status
        return true;
    }
    catch (const std::exception& err)
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
            if (isSnapshotRunning(snapshot_id) && getSnapshotGateway(snapshot_id) == getNodeName())
                running_snapshots.push_back(snapshot_id);
        }
    }
    catch (const Client::Error& err)
    {
        // Return empty vector on error
    }
    return running_snapshots;
}

const std::vector<std::string> ConsulNodeConfiguration::getSnapshots() const
{
    std::vector<std::string> gateway_snapshots;
    try
    {
        auto all_snapshots = getSnapshotIds();
        for (const auto& snapshot_id : all_snapshots)
        {
            std::string snapshot_gateway = getSnapshotGateway(snapshot_id);
            if (snapshot_gateway == getNodeName())
                gateway_snapshots.push_back(snapshot_id);
        }
    }
    catch (const Client::Error& err)
    {
    }
    return gateway_snapshots;
}

const std::vector<std::string> ConsulNodeConfiguration::getAvailableSnapshot() const
{
    std::vector<std::string> available_snapshots;
    // Find the first snapshot that is not running and not locked by any node
    auto all_snapshots = getSnapshotIds();
    for (const auto& snapshot_id : all_snapshots)
    {
        // return all snapshots that have not been locked by gateway
        // and was in running state == true
        auto is_running = isSnapshotRunning(snapshot_id);
        auto gateway = getSnapshotGateway(snapshot_id);
        if (is_running && gateway.empty())
        {
            available_snapshots.push_back(snapshot_id);
        }
    }
    return available_snapshots;
}