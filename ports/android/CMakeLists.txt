add_library(iris_jni SHARED iris_jni.cpp)

target_link_libraries(iris_jni PRIVATE iris_core)
iris_set_warnings(iris_jni)
