cmake_minimum_required(VERSION 3.5)
project(k2eg)

# ----------------------------------------------------
# Set default install directory if not specified
# ----------------------------------------------------
IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  SET(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/local CACHE PATH "Default install directory" FORCE)
ENDIF()

include(ExternalProject)
include(FetchContent)

set(IS_DEBUG_BUILD CMAKE_BUILD_TYPE STREQUAL "Debug")

find_package(Git)
message(STATUS "GIT_EXECUTABLE='${GIT_EXECUTABLE}'")
message(STATUS "Using compiler ${CMAKE_CXX_COMPILER_ID}_${CMAKE_CXX_COMPILER_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(BOOST_VERSION 1.87.0)
set(EPICS_VERSION R7.0.9)
set(LIBLZ4_VERSION v1.9.2)
set(LIBRDKAFKA_VERSION v2.3.0)
set(SQLITE_VERSION version-3.41.2)
set(SQLITEORM_VERSION v1.9.1)
set(MSGPACK_VERSION cpp-7.0.0)
set(CURL_VERSION curl-7_88_1)
set(ZLIB_VERSION v1.2.13)
set(PROMETHEUS_VERSION v1.3.0)
SET(CRONCPP_VERSION v2023.03.30)
SET(OATPP_VERSION 1.3.0)
set(MONGOCXX_VERSION r4.1.1)
set(GTEST_VERSION 1.8.0)

set(CMAKE_BINARY_DIR ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_BINARY_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin)
set(SQLITE_INCLUDE ${CMAKE_INSTALL_PREFIX}/include)


# ----------------------------------------------------
# Suppress deprecated-declarations warnings (e.g. from Boost)
# ----------------------------------------------------
add_compile_options(-Wno-deprecated-declarations)

# ----------------------------------------------------
#manage code coverage
# ----------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/configuration/coverage.cmake)

# ----------------------------------------------------
#manage Address sanitizer
# ----------------------------------------------------
if(${ENABLE_ASAN})
  message(STATUS "Enable address sanitizer")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -fno-omit-frame-pointer -fsanitize=address -static-libasan")
  set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -ggdb -fno-omit-frame-pointer -fsanitize=address -static-libasan")
else()
  message(STATUS "Disable address sanitizer")
endif()

option(ENABLE_TSAN "Enable thread sanitizer" OFF)
if(${ENABLE_TSAN})
  message(STATUS "Enable thread sanitizer")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -fno-omit-frame-pointer -fsanitize=thread")
  set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -ggdb -fno-omit-frame-pointer -fsanitize=thread")
  # Disable ASan if both are set
  set(ENABLE_ASAN OFF)
endif()

configure_file(k2eg.h.in ../src/k2eg/config.h)

if(${IS_DEBUG_BUILD})
  MESSAGE(STATUS "Add debug macro __DEBUG__")
  add_compile_definitions(__DEBUG__)
endif()

set(COMMON_SOURCE_FILES
  src/k2eg/k2eg.cpp
  src/k2eg/common/ProgramOptions.cpp
  src/k2eg/common/ProcSystemMetrics.cpp

  src/k2eg/service/data/DataStorage.cpp
  src/k2eg/service/data/repository/ChannelRepository.cpp

  src/k2eg/service/storage/impl/MongoDBStorageService.cpp
  src/k2eg/service/storage/StorageServiceFactory.cpp

  src/k2eg/service/epics/EpicsData.cpp
  src/k2eg/service/epics/PVStructureMerger.cpp
  src/k2eg/service/epics/EpicsChannel.cpp
  src/k2eg/service/epics/EpicsPutOperation.cpp
  src/k2eg/service/epics/EpicsGetOperation.cpp
  src/k2eg/service/epics/EpicsMonitorOperation.cpp
  src/k2eg/service/epics/EpicsServiceManager.cpp
  src/k2eg/service/epics/JsonSerialization.cpp
  src/k2eg/service/epics/MsgPackSerialization.cpp
  src/k2eg/service/epics/MsgpackEpicsConverter.cpp
  src/k2eg/service/epics/MsgpackCompactSerialization.cpp
  
  src/k2eg/service/pubsub/IPublisher.cpp
  src/k2eg/service/pubsub/ISubscriber.cpp

  src/k2eg/service/pubsub/impl/kafka/RDKafkaBase.cpp
  src/k2eg/service/pubsub/impl/kafka/RDKafkaPublisher.cpp
  src/k2eg/service/pubsub/impl/kafka/RDKafkaSubscriber.cpp

  src/k2eg/service/metric/IMetricService.cpp
  src/k2eg/service/metric/impl/prometheus/PrometheusEpicsMetric.cpp
  src/k2eg/service/metric/impl/prometheus/PrometheusMetricService.cpp
  src/k2eg/service/metric/impl/prometheus/PrometheusCMDControllerMetric.cpp
  src/k2eg/service/metric/impl/prometheus/PrometheusNodeControllerMetric.cpp
  src/k2eg/service/metric/impl/prometheus/PrometheusNodeControllerSystemMetric.cpp
  src/k2eg/service/metric/impl/DummyMetricService.cpp

  src/k2eg/service/log/impl/BoostLogger.cpp

  src/k2eg/service/scheduler/Scheduler.cpp
  src/k2eg/service/scheduler/Task.cpp

  src/k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.cpp

  src/k2eg/controller/command/CMDController.cpp
  src/k2eg/controller/command/CMDCommand.cpp

  src/k2eg/controller/node/NodeController.cpp
  src/k2eg/controller/node/configuration/NodeConfiguration.cpp
  src/k2eg/controller/node/worker/CommandWorker.cpp
  src/k2eg/controller/node/worker/MonitorCommandWorker.cpp
  src/k2eg/controller/node/worker/monitor/MonitorChecker.cpp
  src/k2eg/controller/node/worker/GetCommandWorker.cpp
  src/k2eg/controller/node/worker/PutCommandWorker.cpp
  src/k2eg/controller/node/worker/SnapshotCommandWorker.cpp
  src/k2eg/controller/node/worker/StorageWorker.cpp
  src/k2eg/controller/node/worker/snapshot/ContinuousSnapshotManager.cpp
  src/k2eg/controller/node/worker/snapshot/SnapshotOpInfo.cpp
  src/k2eg/controller/node/worker/snapshot/SnapshotRepeatingOpInfo.cpp
  src/k2eg/controller/node/worker/snapshot/BackTimedBufferedSnapshotOpInfo.cpp

  src/k2eg/controller/node/archiver/ArchiverWorker.cpp
)

set(SOURCE_FILES
  src/main.cpp
  ${COMMON_SOURCE_FILES}
)

ADD_EXECUTABLE(
  ${PROJECT_NAME}
  ${SOURCE_FILES}
)

set_target_properties(${PROJECT_NAME}
  PROPERTIES
  PUBLIC_HEADER "${EPICS_k2eg_INCLUDE}"
)
install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC src)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include)
TARGET_LINK_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/lib)

# ----------------------------------------------------
# Tests
# ----------------------------------------------------
set(TEST_SOURCE_FILES
  test/test.cpp
  test/k2eg.cpp
  test/data/DataStorage.cpp
  test/epics/epics.cpp
  test/epics/epics_msgpack_converter.cpp
  test/epics/epics_data_serialization.cpp
  test/pubsub/kafka.cpp
  test/common/ProgramOptions.cpp
  test/controller/CMDController.cpp
  test/controller/NodeController.cpp
  test/controller/NodeControllerSnapshot.cpp
  test/controller/NodeControllerMonitorChecker.cpp
  test/controller/NodeControllerReplyMessages.cpp
  test/metric/EpicsMetric.cpp
  test/metric/CMDMetric.cpp
  test/metric/NodeControllerMetric.cpp
  test/metric/NodeControllerSystemMetric.cpp
  test/scheduler/Scheduler.cpp
  test/configuration/ConsulConfigurationTest.cpp
  ${COMMON_SOURCE_FILES}
)

ADD_EXECUTABLE(
  ${PROJECT_NAME}-test
  ${TEST_SOURCE_FILES}
)

add_test(
  NAME UnitTest
  COMMAND ${PROJECT_NAME}-test
)

install(TARGETS ${PROJECT_NAME}-test
  RUNTIME DESTINATION bin
)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC src)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include)
TARGET_LINK_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/lib)

# ----------------------------------------------------
# Configure Boost dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure Boost version ${BOOST_VERSION}")

if(WIN32)
  set(BOOST_BOOTSTRAP_CMD ${BOOST_GIT_REPO_DIR}/bootstrap.bat)

  # It was bjam at one point; it is now b2
  set(BOOST_BUILD_CMD ${BOOST_GIT_REPO_DIR}/b2.exe)
elseif(UNIX)
  set(BOOST_BOOTSTRAP_CMD ./${BOOST_GIT_REPO_DIR}/bootstrap.sh)
  set(BOOST_BUILD_CMD ./${BOOST_GIT_REPO_DIR}/b2)
endif()

ExternalProject_Add(
  boost
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/boost
  GIT_REPOSITORY https://github.com/boostorg/boost.git
  GIT_TAG boost-${BOOST_VERSION}
  BUILD_IN_SOURCE 1
  GIT_SUBMODULES_RECURSE true
  UPDATE_COMMAND ${BOOST_BOOTSTRAP_CMD}
  BUILD_COMMAND ${BOOST_BUILD_CMD} cxxstd=17 --with-system --with-program_options --with-log --with-json link=shared install --prefix=${CMAKE_INSTALL_PREFIX}
  INSTALL_COMMAND ""
  LOG_DOWNLOAD 1
  LOG_UPDATE 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
)

SET(BUILD_LIBRARY ${BUILD_LIBRARY} boost_program_options boost_log boost_log_setup boost_thread boost_json boost_filesystem boost_atomic boost_container boost_chrono)
ADD_DEPENDENCIES(${PROJECT_NAME} boost)
ADD_DEPENDENCIES(${PROJECT_NAME}-test boost)

# ----------------------------------------------------
# Configure EPICS dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure EPICS version ${EPICS_VERSION}")
MESSAGE(STATUS "Configure EPICS includes: ${CMAKE_INSTALL_PREFIX}/include/os/${CMAKE_SYSTEM_NAME}, ${CMAKE_INSTALL_PREFIX}/include/pv, ${CMAKE_INSTALL_PREFIX}/include/pva")

ExternalProject_Add(
  epics
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/epics
  GIT_REPOSITORY https://github.com/epics-base/epics-base.git
  GIT_TAG ${EPICS_VERSION}
  BUILD_IN_SOURCE 1
  PATCH_COMMAND ${PROJECT_SOURCE_DIR}/tools/epics-patch.sh ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CXX_COMPILER} ${CMAKE_C_COMPILER}
  CONFIGURE_COMMAND ""
  UPDATE_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_COMMAND make -j 3 INSTALL_LOCATION=${CMAKE_INSTALL_PREFIX} install
)

SET(BUILD_LIBRARY ${BUILD_LIBRARY} nt pvaClient pvAccess Com pvData pvDatabase pvAccessCA)
ADD_DEPENDENCIES(${PROJECT_NAME} epics)
ADD_DEPENDENCIES(${PROJECT_NAME}-test epics)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include/os/${CMAKE_SYSTEM_NAME})
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include/pv)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include/pva)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include/os/${CMAKE_SYSTEM_NAME})
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include/pv)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include/pva)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  MESSAGE(STATUS "Configure EPICS include for clang compiler")
  MESSAGE(STATUS "Configure EPICS includes: ${CMAKE_INSTALL_PREFIX}/include/compiler/clang")
  TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include/compiler/clang)
  TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include/compiler/clang)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # using GCC
  MESSAGE(STATUS "Configure EPICS include for gcc compiler")
  MESSAGE(STATUS "Configure EPICS includes: ${CMAKE_INSTALL_PREFIX}/include/compiler/gcc")
  TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include/compiler/gcc)
  TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include/compiler/gcc)
endif()

if(APPLE)
  MESSAGE(STATUS "Configure EPICS includes: {CMAKE_INSTALL_PREFIX}/lib/darwin-x86")  
  TARGET_LINK_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/lib/darwin-x86)
  TARGET_LINK_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/lib/darwin-x86)
elseif(UNIX)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message(STATUS "Configure EPICS includes: ${CMAKE_INSTALL_PREFIX}/lib/linux-x86_64")
    TARGET_LINK_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/lib/linux-x86_64)
    TARGET_LINK_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/lib/linux-x86_64)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    message(STATUS "Configure EPICS includes: ${CMAKE_INSTALL_PREFIX}/lib/linux-aarch64")
    TARGET_LINK_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/lib/linux-aarch64)
    TARGET_LINK_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/lib/linux-aarch64)
  endif()
endif()

# ----------------------------------------------------
# Configure liblz4 dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure liblz4 version ${LIBLZ4_VERSION}")
ExternalProject_Add(
  liblz4
  GIT_REPOSITORY https://github.com/lz4/lz4.git
  GIT_TAG ${LIBLZ4_VERSION}
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/liblz4
  SOURCE_SUBDIR contrib/cmake_unofficial
  CMAKE_ARGS
  -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
  -DCMAKE_BUILD_TYPE=Release
  -DBUILD_STATIC_LIBS=OFF
  -DBUILD_SHARED_LIBS=ON
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  LOG_DOWNLOAD ON
  LOG_CONFIGURE ON
  LOG_BUILD ON
)

# ----------------------------------------------------
# Configure librdkafka dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure librdkafka version ${LIBRDKAFKA_VERSION}")
ExternalProject_Add(
  librdkafka
  GIT_TAG ${LIBRDKAFKA_VERSION}
  GIT_REPOSITORY https://github.com/edenhill/librdkafka.git
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/librdkafa
  CMAKE_ARGS
    -DWITH_SSL=ON
    -DWITH_CURL=ON
    -DRDKAFKA_BUILD_EXAMPLES=OFF
    -DRDKAFKA_BUILD_STATIC=OFF
    -DRDKAFKA_BUILD_TESTS=OFF
    -DWITH_SASL=ON
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} 
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  LOG_DOWNLOAD 1
  LOG_UPDATE 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
)
SET(BUILD_LIBRARY ${BUILD_LIBRARY} rdkafka rdkafka++)
ADD_DEPENDENCIES(librdkafka liblz4)
ADD_DEPENDENCIES(${PROJECT_NAME} librdkafka liblz4)
ADD_DEPENDENCIES(${PROJECT_NAME}-test librdkafka liblz4)

# ----------------------------------------------------
# Configure sqlite dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure sqlite version ${SQLITE_VERSION}")
ExternalProject_Add(
  sqlite_dist
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/sqlite
  GIT_TAG ${SQLITE_VERSION}
  GIT_REPOSITORY https://github.com/sqlite/sqlite.git
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/sqlite/src/sqlite_dist/configure --enable-tempstore=yes --prefix=${CMAKE_INSTALL_PREFIX}
  INSTALL_COMMAND make install
)

# ----------------------------------------------------
# Configure sqlite orm dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure sqlite orm version ${SQLITEORM_VERSION}")
ExternalProject_Add(
  sqlite_orm
  GIT_TAG ${SQLITEORM_VERSION}
  GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/sqlite_orm
  CMAKE_ARGS
    -DSQLITE_ORM_ENABLE_CXX_20=ON
    -DBUILD_TESTING=OFF
    -DBUILD_EXAMPLES=OFF
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} 
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  LOG_DOWNLOAD 1
  LOG_UPDATE 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
)

ADD_DEPENDENCIES(sqlite_orm sqlite_dist)
SET(BUILD_LIBRARY ${BUILD_LIBRARY} sqlite3)
ADD_DEPENDENCIES(${PROJECT_NAME} sqlite_orm sqlite_dist)
ADD_DEPENDENCIES(${PROJECT_NAME}-test sqlite_orm sqlite_dist)

# ----------------------------------------------------
# Configure msgpack dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure msgpack version ${MSGPACK_VERSION}")
ExternalProject_Add(
  msgpack
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/msgpack
  GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git
  GIT_TAG ${MSGPACK_VERSION}
  GIT_PROGRESS 1
  LOG_DOWNLOAD 1
  LOG_UPDATE 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
  CMAKE_ARGS
  -DMSGPACK_USE_BOOST=ON
  -DMSGPACK_CXX20=ON
  -DMSGPACK_USE_CPP03=ON
  -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
  -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} 
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
)
SET(BUILD_LIBRARY ${BUILD_LIBRARY})
ADD_DEPENDENCIES(msgpack boost)
ADD_DEPENDENCIES(${PROJECT_NAME} msgpack)
ADD_DEPENDENCIES(${PROJECT_NAME}-test msgpack)

# ----------------------------------------------------
# Configure zlib dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure zlib version ${ZLIB_VERSION}")
ExternalProject_Add(
  zlib
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG ${ZLIB_VERSION}
  GIT_PROGRESS 1
  LOG_DOWNLOAD 1
  LOG_UPDATE 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
  CMAKE_ARGS
  -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
  -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} 
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
)
SET(BUILD_LIBRARY ${BUILD_LIBRARY} z)

# ----------------------------------------------------
# Configure prometheus dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure prometheus version ${PROMETHEUS_VERSION}")
ExternalProject_Add(
  prometheus
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/prometheus
  GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
  GIT_TAG ${PROMETHEUS_VERSION}
  GIT_PROGRESS 1
  LOG_DOWNLOAD 1
  LOG_UPDATE 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
  CMAKE_ARGS
  -DENABLE_TESTING=OFF
  -DENABLE_PUSH=OFF 
  -DENABLE_COMPRESSION=ON
  -DBUILD_SHARED_LIBS=ON
  -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
  -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} 
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
)
ADD_DEPENDENCIES(prometheus zlib)
SET(BUILD_LIBRARY ${BUILD_LIBRARY} prometheus-cpp-pull prometheus-cpp-core curl)
ADD_DEPENDENCIES(${PROJECT_NAME} prometheus)
ADD_DEPENDENCIES(${PROJECT_NAME}-test prometheus)

# ----------------------------------------------------
# Configure cron scheduler dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure croncpp version ${CRONCPP_VERSION}")
ExternalProject_Add(
  croncpp
  GIT_REPOSITORY https://github.com/mariusbancila/croncpp.git
  GIT_TAG ${CRONCPP_VERSION}
  PREFIX "${CMAKE_CURRENT_BINARY_DIR}/croncpp"
  LOG_DOWNLOAD ON
  LOG_CONFIGURE ON
  LOG_BUILD ON
  CMAKE_ARGS
  -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
  -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} 
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
)
#SET(BUILD_LIBRARY ${BUILD_LIBRARY} cron)
ADD_DEPENDENCIES(${PROJECT_NAME} croncpp)
ADD_DEPENDENCIES(${PROJECT_NAME}-test croncpp)

# ----------------------------------------------------
# External Project: oatpp
# ----------------------------------------------------
MESSAGE(STATUS "Configure oatpp-framework version ${OATPP_VERSION}")
ExternalProject_Add(
    oatpp-framework
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/oatpp-framework
    GIT_REPOSITORY    https://github.com/oatpp/oatpp.git
    GIT_TAG           ${OATPP_VERSION}
    GIT_PROGRESS      1
    LOG_DOWNLOAD      1
    LOG_UPDATE        1
    LOG_CONFIGURE     1
    LOG_BUILD         1
    LOG_INSTALL       1
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} 
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DBUILD_SHARED_LIBS=ON
)
MESSAGE(STATUS "oatpp include: ${CMAKE_INSTALL_PREFIX}/include/oatpp-${OATPP_VERSION}/oatpp")
MESSAGE(STATUS "oatpp lib: ${CMAKE_INSTALL_PREFIX}/lib/oatpp-${OATPP_VERSION}/oatpp")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include/oatpp-${OATPP_VERSION}/oatpp)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include/oatpp-${OATPP_VERSION}/oatpp)

# ----------------------------------------------------
# External Project: oatpp-curl
# ----------------------------------------------------
MESSAGE(STATUS "Configure oatpp-curl version ${OATPP_VERSION}")
ExternalProject_Add(
  oatpp-curl-framework
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/oatpp-curl-framework
    GIT_REPOSITORY    https://github.com/oatpp/oatpp-curl.git
    GIT_TAG           ${OATPP_VERSION}
    GIT_PROGRESS      1
    LOG_DOWNLOAD      1
    LOG_UPDATE        1
    LOG_CONFIGURE     1
    LOG_BUILD         1
    LOG_INSTALL       1
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} 
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DBUILD_SHARED_LIBS=ON
)
ADD_DEPENDENCIES(oatpp-curl-framework oatpp-framework)
ADD_DEPENDENCIES(${PROJECT_NAME} oatpp-curl-framework)
ADD_DEPENDENCIES(${PROJECT_NAME}-test oatpp-curl-framework)
MESSAGE(STATUS "oatpp-curl include: ${CMAKE_INSTALL_PREFIX}/include/oatpp-${OATPP_VERSION}/oatpp-curl")
MESSAGE(STATUS "oatpp-curl lib: ${CMAKE_INSTALL_PREFIX}/lib/oatpp-${OATPP_VERSION}/oatpp-curl")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include/oatpp-${OATPP_VERSION}/oatpp-curl)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include/oatpp-${OATPP_VERSION}/oatpp-curl)
# ----------------------------------------------------
# External Project: oatpp-consul
# ----------------------------------------------------
MESSAGE(STATUS "Configure oatpp-consul version ${OATPP_VERSION}")
ExternalProject_Add(
    oatpp-consul-framework
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/oatpp-consul-framework
    GIT_REPOSITORY    https://github.com/oatpp/oatpp-consul.git
    GIT_TAG           ${OATPP_VERSION}
    GIT_PROGRESS      1
    LOG_DOWNLOAD      1
    LOG_UPDATE        1
    LOG_CONFIGURE     1
    LOG_BUILD         1
    LOG_INSTALL       1
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} 
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DBUILD_SHARED_LIBS=ON
)
ADD_DEPENDENCIES(oatpp-consul-framework oatpp-curl-framework)
ADD_DEPENDENCIES(${PROJECT_NAME} oatpp-consul-framework)
ADD_DEPENDENCIES(${PROJECT_NAME}-test oatpp-consul-framework)
MESSAGE(STATUS "oatpp-consul include: ${CMAKE_INSTALL_PREFIX}/include/oatpp-${OATPP_VERSION}/oatpp-consul")
MESSAGE(STATUS "oatpp-consul lib: ${CMAKE_INSTALL_PREFIX}/lib/oatpp-${OATPP_VERSION}/oatpp-consul")
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include/oatpp-${OATPP_VERSION}/oatpp-consul)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include/oatpp-${OATPP_VERSION}/oatpp-consul)
TARGET_LINK_DIRECTORIES(${PROJECT_NAME} PUBLIC  ${CMAKE_INSTALL_PREFIX}/lib/oatpp-${OATPP_VERSION})
TARGET_LINK_DIRECTORIES(${PROJECT_NAME}-test PUBLIC  ${CMAKE_INSTALL_PREFIX}/lib/oatpp-${OATPP_VERSION})
SET(BUILD_LIBRARY ${BUILD_LIBRARY} oatpp-consul oatpp-curl oatpp)


# ----------------------------------------------------
# Configure MongoDB CXX driver dependencies
# ----------------------------------------------------
MESSAGE(STATUS "Configure MongoDB CXX driver version ${MONGOCXX_VERSION}")
ExternalProject_Add(
  mongo-cxx-driver
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/mongo-cxx-driver
  GIT_REPOSITORY https://github.com/mongodb/mongo-cxx-driver.git
  GIT_TAG ${MONGOCXX_VERSION}
  GIT_PROGRESS 1
  LOG_DOWNLOAD 1
  LOG_UPDATE 1
  LOG_CONFIGURE 1
  LOG_BUILD 1
  LOG_INSTALL 1
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=ON
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
    -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
    -DENABLE_TESTS=OFF
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} 
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_STANDARD=17
)

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include/bsoncxx/v_noabi)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/include/mongocxx/v_noabi)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include/bsoncxx/v_noabi)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}-test PUBLIC ${CMAKE_INSTALL_PREFIX}/include/mongocxx/v_noabi)


SET(BUILD_LIBRARY ${BUILD_LIBRARY} mongocxx bsoncxx)
ADD_DEPENDENCIES(${PROJECT_NAME} mongo-cxx-driver)
ADD_DEPENDENCIES(${PROJECT_NAME}-test mongo-cxx-driver)

# ----------------------------------------------------
# Configure Google Test dependencies
# ----------------------------------------------------
message(STATUS "Setup GTest version ${GTEST_VERSION}")
ExternalProject_Add(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-${GTEST_VERSION}
  PREFIX "${CMAKE_CURRENT_BINARY_DIR}/gtest"
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
  LOG_DOWNLOAD ON
  LOG_CONFIGURE ON
  LOG_BUILD ON
)

ADD_DEPENDENCIES(${PROJECT_NAME}-test googletest)

ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK)
ADD_DEFINITIONS(-DBS_THREAD_POOL_NATIVE_EXTENSIONS)

# Link libraries and set include directories
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${BUILD_LIBRARY} pthread)
TARGET_LINK_LIBRARIES(${PROJECT_NAME}-test PUBLIC ${BUILD_LIBRARY} pthread gtest)
MESSAGE(STATUS "Linked libraries: ${BUILD_LIBRARY}")
MESSAGE(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")