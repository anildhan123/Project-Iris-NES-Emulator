#include "iris.h"

#include <jni.h>

#include <cstdint>

namespace {

IrisSystem* fromHandle(jlong handle) {
    return reinterpret_cast<IrisSystem*>(static_cast<std::uintptr_t>(handle));
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL Java_iris_core_IrisNative_create(JNIEnv*, jclass) {
    return static_cast<jlong>(reinterpret_cast<std::uintptr_t>(iris_create()));
}

extern "C" JNIEXPORT void JNICALL Java_iris_core_IrisNative_destroy(JNIEnv*, jclass, jlong handle) {
    iris_destroy(fromHandle(handle));
}

extern "C" JNIEXPORT jint JNICALL Java_iris_core_IrisNative_load(
    JNIEnv* env, jclass, jlong handle, jbyteArray bytes, jstring name) {
    if (env == nullptr || bytes == nullptr) {
        return 0;
    }
    const jsize length = env->GetArrayLength(bytes);
    jbyte* data = env->GetByteArrayElements(bytes, nullptr);
    if (data == nullptr) {
        return 0;
    }
    const char* title = name == nullptr ? nullptr : env->GetStringUTFChars(name, nullptr);
    char error[256] = {};
    const int loaded = iris_load_bytes(
        fromHandle(handle), reinterpret_cast<const uint8_t*>(data), static_cast<size_t>(length), title, error, sizeof(error));
    if (title != nullptr) {
        env->ReleaseStringUTFChars(name, title);
    }
    env->ReleaseByteArrayElements(bytes, data, JNI_ABORT);
    if (loaded == 0) {
        jclass exception = env->FindClass("java/lang/IllegalArgumentException");
        if (exception != nullptr) {
            env->ThrowNew(exception, error[0] == '\0' ? "The ROM could not be loaded." : error);
        }
    }
    return loaded;
}

extern "C" JNIEXPORT void JNICALL Java_iris_core_IrisNative_runFrame(JNIEnv*, jclass, jlong handle) {
    iris_run_frame(fromHandle(handle));
}

extern "C" JNIEXPORT jbyteArray JNICALL Java_iris_core_IrisNative_frame(JNIEnv* env, jclass, jlong handle) {
    if (env == nullptr) {
        return nullptr;
    }
    int width = 0;
    int height = 0;
    const uint8_t* rgba = iris_frame(fromHandle(handle), &width, &height);
    if (rgba == nullptr || width <= 0 || height <= 0) {
        return nullptr;
    }
    const jsize bytes = static_cast<jsize>(width) * static_cast<jsize>(height) * 4;
    jbyteArray array = env->NewByteArray(bytes);
    if (array == nullptr) {
        return nullptr;
    }
    env->SetByteArrayRegion(array, 0, bytes, reinterpret_cast<const jbyte*>(rgba));
    return array;
}

extern "C" JNIEXPORT jlong JNICALL Java_iris_core_IrisNative_framePeriodNs(JNIEnv*, jclass, jlong handle) {
    return static_cast<jlong>(iris_frame_period_ns(fromHandle(handle)));
}
