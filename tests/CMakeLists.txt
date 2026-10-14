add_executable(landerviz_protocol_test
  protocol_test.cpp
  ../src/audio_analysis.cpp
  ../src/usb_status.cpp
)
target_include_directories(landerviz_protocol_test PRIVATE ../include)
target_compile_features(landerviz_protocol_test PRIVATE cxx_std_17)
target_compile_options(landerviz_protocol_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME landerviz_protocol_test COMMAND landerviz_protocol_test)
