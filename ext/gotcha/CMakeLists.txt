include(ExternalProject)

ExternalProject_Add(gotcha
  GIT_REPOSITORY "https://www.github.com/LLNL/gotcha"
  CMAKE_ARGS     -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/gotcha-build -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DGOTCHA_ENABLE_TESTS=Off
  BUILD_COMMAND "make"
)

