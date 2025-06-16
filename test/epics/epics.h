#include "msgpack/v3/pack_decl.hpp"
#include <k2eg/common/MsgpackSerialization.h>

#define INIT_PVA_PROVIDER()                                                                                                      \
  epics::pvAccess::Configuration::shared_pointer test_pva_conf     = epics::pvAccess::ConfigurationBuilder().push_env().build(); \
  std::unique_ptr<pvac::ClientProvider>          test_pva_provider = std::make_unique<pvac::ClientProvider>("pva", test_pva_conf);

#define INIT_CA_PROVIDER()                                                                                                      \
  epics::pvAccess::Configuration::shared_pointer test_ca_conf     = epics::pvAccess::ConfigurationBuilder().push_env().build(); \
    std::unique_ptr<pvac::ClientProvider> test_ca_provider = std::make_unique<pvac::ClientProvider>("ca", \
     epics::pvAccess::ConfigurationBuilder().add("PATH", "build/local/bin/linux-x86_64").push_map().push_env().build());

#define WHILE_OP(x, v) \
  do { std::this_thread::sleep_for(std::chrono::milliseconds(250)); } while (x->isDone() == v)

#define WHILE_MONITOR(monitor_op, cond) \
 do { monitor_op->poll(); } while (cond)

#define TIMEOUT(x, v, retry, sleep_m_sec) \
  int r = retry; \
  do { std::this_thread::sleep_for(std::chrono::milliseconds(sleep_m_sec)); r--;} while (x == v && r > 0)

template <typename Scalar>
std::unique_ptr<k2eg::common::MsgpackObjectWithZone> msgpack_from_scalar(const std::string& key, const Scalar& value)
{
    std::map<std::string, Scalar> val = {{key, value}};
    // Create a MsgpackObjectWithZone to hold the packed data
    return std::make_unique<k2eg::common::MsgpackObjectWithZone>(val);
}

#define MOVE_MSGPACK_SCALAR(f, t, x) std::move(msgpack_from_scalar<t>(f, x))