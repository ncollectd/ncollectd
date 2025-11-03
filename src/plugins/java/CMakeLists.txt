plugin_option(PLUGIN_JAVA "Embed the Java Virtual Machine" ON)

include(UseJava)

set(BUILD_PLUGIN_JAVA 0)

if(PLUGIN_JAVA)
    find_package(JNI)
    find_package(Java)
    if(JNI_FOUND AND JAVA_FOUND)
        set(BUILD_PLUGIN_JAVA 1)
    else()
        set(BUILD_PLUGIN_JAVA_REASON "java(JNI) not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_JAVA ${BUILD_PLUGIN_JAVA} PARENT_SCOPE)

if(BUILD_PLUGIN_JAVA)
    set(PLUGIN_JAVA_SRC java.c jutil.c jutil.h notification.c metric.c config.c)
    add_library(java MODULE ${PLUGIN_JAVA_SRC})
    target_include_directories(java PRIVATE ${JNI_INCLUDE_DIRS})
    target_link_libraries(java PRIVATE libmetric libutils ${JAVA_JVM_LIBRARY})
    set_target_properties(java PROPERTIES PREFIX "")

    set(NCOLLECTD_API_SRC
        org/ncollectd/api/NCollectd.java
        org/ncollectd/api/NCollectdConfigInterface.java
        org/ncollectd/api/NCollectdInitInterface.java
        org/ncollectd/api/NCollectdLogInterface.java
        org/ncollectd/api/NCollectdNotificationInterface.java
        org/ncollectd/api/NCollectdReadInterface.java
        org/ncollectd/api/NCollectdShutdownInterface.java
        org/ncollectd/api/NCollectdWriteInterface.java
        org/ncollectd/api/Notification.java
        org/ncollectd/api/MetricFamily.java
        org/ncollectd/api/Metric.java
        org/ncollectd/api/MetricGauge.java
        org/ncollectd/api/MetricUnknown.java
        org/ncollectd/api/MetricGauge.java
        org/ncollectd/api/MetricCounter.java
        org/ncollectd/api/MetricInfo.java
        org/ncollectd/api/MetricStateSet.java
        org/ncollectd/api/MetricSummaryQuantile.java
        org/ncollectd/api/MetricSummary.java
        org/ncollectd/api/MetricHistogramBucket.java
        org/ncollectd/api/MetricHistogram.java
        org/ncollectd/api/ConfigItem.java
        org/ncollectd/api/ConfigValue.java)
    add_jar("ncollectd-api" ${NCOLLECTD_API_SRC})

    set(GENERIC_JMX_SRC
        org/ncollectd/java/GenericJMX.java
        org/ncollectd/java/GenericJMXConfConnection.java
        org/ncollectd/java/GenericJMXConfMBean.java
        org/ncollectd/java/GenericJMXConfMetric.java
        org/ncollectd/java/JMXMemory.java)
    add_jar("generic-jmx" ${GENERIC_JMX_SRC})

#    add_executable(test_plugin_java EXCLUDE_FROM_ALL java_test.c ${PLUGIN_JAVA_SRC})
#    target_link_libraries(test_plugin_java libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_java)
#    add_test(NAME test_plugin_java COMMAND test_plugin_java WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS java DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-java.5 ncollectd-java.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-java.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
    install_jar("ncollectd-api" ${CMAKE_INSTALL_DATAROOTDIR}/ncollectd/)
    install_jar("generic-jmx" ${CMAKE_INSTALL_DATAROOTDIR}/ncollectd/)
endif()
