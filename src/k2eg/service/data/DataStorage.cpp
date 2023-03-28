#include <k2eg/service/data/DataStorage.h>

#include <filesystem>

using namespace k2eg::service::data;
using namespace k2eg::service::data::repository;

using namespace sqlite_orm;

namespace fs = std::filesystem;

#define PUBLIC_SHARED_ENABLER(x)                                                      \
    struct x##SharedEnabler : public x                                                \
    {                                                                                 \
        x##SharedEnabler(DataStorage &data_storage) : ChannelRepository(data_storage) \
        {                                                                             \
        }                                                                             \
    };

DataStorage::DataStorage(const std::string &path)
{
    const fs::path db_path = path;
    std::string file_path;

    if(db_path.filename().has_filename()) {
        file_path = path;
    } else if(fs::exists(db_path) && fs::is_directory(db_path)) {
        file_path = db_path / "database.sqlite";
    } else {
        throw std::runtime_error("The path "+ path + " not exists");
    }

    storage = std::make_shared<Storage>(initStorage(file_path));
    if (!storage)
    {
        throw std::runtime_error("Error creating database");
    }
    storage->sync_schema();
}

std::weak_ptr<repository::ChannelRepository> DataStorage::getChannelRepository()
{
    PUBLIC_SHARED_ENABLER(ChannelRepository)
    std::unique_lock<std::mutex> m(repository_access_mutex);
    if (!channel_repository_instance)
    {
        channel_repository_instance = std::make_shared<ChannelRepositorySharedEnabler>(*this);
    }
    return channel_repository_instance;
}

StorageLockedRef DataStorage::getLockedStorage()
{
    //create unique lock without owning the lock
    return std::move(StorageLockedRef(
            std::move(std::unique_lock<std::recursive_mutex>(storage_mutex, std::defer_lock)),
            storage
            ));
}