
#ifndef k2eg_SERVICE_DATA_DATASTORAGE_H_
#define k2eg_SERVICE_DATA_DATASTORAGE_H_

#include <sqlite_orm/sqlite_orm.h>
#include <k2eg/service/data/repository/PVRepository.h>

#include <memory>
#include <mutex>
namespace k2eg::service::data
{
    /**
     *
     */
    inline auto initStorage(const std::string &path)
    {
        using namespace sqlite_orm;
        return make_storage(path,
                            make_table("pv_monitor",
                                       make_column("id", &repository::PVMonitorType::id, primary_key().autoincrement()),
                                       make_column("pv_name", &repository::PVMonitorType::pv_name),
                                       make_column("event_serialization", &repository::PVMonitorType::event_serialization),
                                       make_column("pv_protocol", &repository::PVMonitorType::pv_protocol),
                                       make_column("pv_destination", &repository::PVMonitorType::pv_destination),
                                       make_column("requested_instance", &repository::PVMonitorType::requested_instance)));
    }

    using Storage = decltype(initStorage(""));

    /**
     *
     */
    inline std::shared_ptr<Storage> getStorageFromWPtr(std::weak_ptr<Storage> ws)
    {
        auto ss = ws.lock();
        if (!ss)
            throw std::runtime_error("Error locking storage: " + std::string(typeid(ss).name()));
        return ss;
    }

    /**
     * Contains lockable access to storage for multithreading operation
     */
    struct StorageLockedRef
    {
        StorageLockedRef(
            std::unique_lock<std::recursive_mutex>&& s_lock,
            std::weak_ptr<Storage>&&ss_wptr):s_lock(std::move(s_lock)),ss_wptr(std::move(ss_wptr)) {}
        std::weak_ptr<Storage> ss_wptr;
        /*
        * thelock is owned on the first call of this method
        * and is release when the StorageLockedRef instance
        * is destroyed
        */
        std::shared_ptr<Storage> get() {
            //cquire lock
            s_lock.lock();
            return std::move(getStorageFromWPtr(ss_wptr));
        }
        private:
        std::unique_lock<std::recursive_mutex> s_lock;
    };

    typedef std::unique_ptr<DataStorage> DataStorageUPtr;

    /**
     *
     */
    class DataStorage
    {
        std::mutex repository_access_mutex;
        std::recursive_mutex storage_mutex;
        std::shared_ptr<Storage> storage;
        std::shared_ptr<repository::PVRepository> channel_repository_instance;

    public:
        DataStorage(const std::string &path);
        ~DataStorage() = default;
        std::weak_ptr<repository::PVRepository> getPVRepository();
        /**
         * The result @StorageLockedRef reference should be used for single operation
        */
        StorageLockedRef getLockedStorage();
    };

}

#endif // k2eg_SERVICE_DATA_DATASTORAGE_H_