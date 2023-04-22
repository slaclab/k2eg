#define INIT_PVA_PROVIDER()                                                                                                      \
  epics::pvAccess::Configuration::shared_pointer test_pva_conf     = epics::pvAccess::ConfigurationBuilder().push_env().build(); \
  std::unique_ptr<pvac::ClientProvider>          test_pva_provider = std::make_unique<pvac::ClientProvider>("pva", test_pva_conf);

#define INIT_CA_PROVIDER()                                                                                                      \
  epics::pvAccess::Configuration::shared_pointer test_ca_conf     = epics::pvAccess::ConfigurationBuilder().push_env().build(); \
    std::unique_ptr<pvac::ClientProvider> test_ca_provider = std::make_unique<pvac::ClientProvider>("ca", \
     epics::pvAccess::ConfigurationBuilder().add("PATH", "build/local/bin/linux-x86_64").push_map().push_env().build());
