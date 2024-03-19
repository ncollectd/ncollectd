set(LIBMETRIC_SRC histogram.c histogram.h
                  summary.c summary.h
                  metric.c metric.h
                  notification.c notification.h
                  label_set.c label_set.h
                  state_set.c state_set.h
                  metric_chars.c metric_chars.h
                  metric_match.c metric_match.h
                  parser.c parser.h
                  marshal.c marshal.h
                  rate.c rate.h)

add_library(libmetric STATIC ${LIBMETRIC_SRC})
set_target_properties(libmetric PROPERTIES PREFIX "")
set_target_properties(libmetric PROPERTIES POSITION_INDEPENDENT_CODE ON)
# add_dependencies(libmetric libutils)

add_executable(test_libmetric_metric EXCLUDE_FROM_ALL metric_test.c)
target_link_libraries(test_libmetric_metric libutils libmetric libtest)
add_dependencies(build_tests test_libmetric_metric)
add_test(NAME test_libmetric_metric COMMAND test_libmetric_metric)

add_executable(test_libmetric_label_set EXCLUDE_FROM_ALL label_set_test.c)
target_link_libraries(test_libmetric_label_set libutils libmetric libtest)
add_dependencies(build_tests test_libmetric_label_set)
add_test(NAME test_libmetric_label_set COMMAND test_libmetric_label_set)
