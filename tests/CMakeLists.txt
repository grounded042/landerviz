add_executable(landerviz_protocol_test
  protocol_test.cpp
  ../src/audio_analysis.cpp
  ../src/effects.cpp
  ../src/flow_v2.cpp
  ../src/flow_v3.cpp
  ../src/flow_v3_attack.cpp
  ../src/flow_v3_color.cpp
  ../src/flow_v3_combined.cpp
  ../src/flow_v3_energy.cpp
  ../src/flow_v3_engine.cpp
  ../src/flow_v3_mid.cpp
  ../src/flow_v3_texture.cpp
  ../src/visualizer.cpp
  ../src/usb_status.cpp
)
target_include_directories(landerviz_protocol_test PRIVATE ../include)
target_compile_features(landerviz_protocol_test PRIVATE cxx_std_17)
target_compile_definitions(landerviz_protocol_test PRIVATE
  LANDERVIZ_PREVIEW_FLOW_V2
  LANDERVIZ_PREVIEW_FLOW_V3
)
target_compile_options(landerviz_protocol_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME landerviz_protocol_test COMMAND landerviz_protocol_test)
