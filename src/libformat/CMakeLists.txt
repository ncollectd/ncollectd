set(LIBFORMAT_SRC graphite_line.c graphite_line.h
                  influxdb.c influxdb.h
                  openmetrics_text.c openmetrics_text.h
                  opentsdb_json.c opentsdb_json.h
                  opentsdb_telnet.c opentsdb_telnet.h
                  json.c json.h
                  openmetrics_proto.c openmetrics_proto.h
                  remote_proto.c remote_proto.h
                  notification_json.c notification_json.h
                  notification_text.c notification_text.h
                  notification_proto.c notification_proto.h
                  log_text.c log_text.h
                  log_logfmt.c log_logfmt.h
                  log_json.c log_json.h
                  log_logstash.c log_logstash.h
                  format.c format.h)

add_library(libformat STATIC ${LIBFORMAT_SRC})
set_target_properties(libformat PROPERTIES PREFIX "")
set_target_properties(libformat PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_dependencies(libformat libutils)
add_dependencies(libformat libxson)

add_executable(test_libformat_graphite_line EXCLUDE_FROM_ALL graphite_line_test.c)
target_link_libraries(test_libformat_graphite_line libformat libxson libutils libmetric libtest -lm)
add_dependencies(build_tests test_libformat_graphite_line)
add_test(NAME test_libformat_graphite_line COMMAND test_libformat_graphite_line)

add_executable(test_libformat_influxdb EXCLUDE_FROM_ALL influxdb_test.c)
target_link_libraries(test_libformat_influxdb libformat libxson libutils libmetric libtest -lm)
add_dependencies(build_tests test_libformat_influxdb)
add_test(NAME test_libformat_influxdb COMMAND test_libformat_influxdb)

add_executable(test_libformat_openmetrics_text EXCLUDE_FROM_ALL openmetrics_text_test.c)
target_link_libraries(test_libformat_openmetrics_text libformat libxson libutils libmetric libtest -lm)
add_dependencies(build_tests test_libformat_openmetrics_text)
add_test(NAME test_libformat_openmetrics_text COMMAND test_libformat_openmetrics_text)

add_executable(test_libformat_opentsdb_json EXCLUDE_FROM_ALL opentsdb_json_test.c)
target_link_libraries(test_libformat_opentsdb_json libformat libxson libutils libmetric libtest -lm)
add_dependencies(build_tests test_libformat_opentsdb_json)
add_test(NAME test_libformat_opentsdb_json COMMAND test_libformat_opentsdb_json)

add_executable(test_libformat_opentsdb_telnet EXCLUDE_FROM_ALL opentsdb_telnet_test.c)
target_link_libraries(test_libformat_opentsdb_telnet libformat libxson libutils libmetric libtest -lm)
add_dependencies(build_tests test_libformat_opentsdb_telnet)
add_test(NAME test_libformat_opentsdb_telnet COMMAND test_libformat_opentsdb_telnet)

add_executable(test_libformat_json EXCLUDE_FROM_ALL json_test.c)
target_link_libraries(test_libformat_json libformat libxson libutils libmetric libtest -lm)
add_dependencies(build_tests test_libformat_json)
add_test(NAME test_libformat_json COMMAND test_libformat_json)
