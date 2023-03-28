#include <gtest/gtest.h>
#include <k2eg/service/epics/EpicsChannel.h>
class TestEnvironment : public ::testing::Environment {
public:
    // Initialise the timestamp in the environment setup.
    virtual void SetUp() {
        k2eg::service::epics_impl::EpicsChannel::init();        
    }
    virtual void TearDown() {
        k2eg::service::epics_impl::EpicsChannel::deinit();        
    }
};

int main(int argc, char** argv) {
    testing::AddGlobalTestEnvironment(new TestEnvironment());
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
