#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
  #ifdef VISIONAI_ALGORITHM_EXPORTS
    #define VISIONAI_API __declspec(dllexport)
  #else
    #define VISIONAI_API
  #endif
#else
  #define VISIONAI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// VisionAIAlgorithmHandle（算法实例句柄）采用不透明指针，避免跨编译器暴露 C++ ABI（应用二进制接口）。
typedef void* VisionAIAlgorithmHandle;

// VisionAIAlgorithmCreateInfo（算法创建参数）。
// algorithm_id / config_json 的字符串内存由调用方持有，仅保证在 create 调用期间有效。
typedef struct VisionAIAlgorithmCreateInfo {
    const char* algorithm_id;
    const char* config_json;
} VisionAIAlgorithmCreateInfo;

// 创建算法实例。失败时返回 NULL；实现方不得把异常越过 C ABI 边界抛给调用方。
VISIONAI_API VisionAIAlgorithmHandle visionai_algorithm_create(
    const VisionAIAlgorithmCreateInfo* info);

// 处理一次算法输入。返回 0 表示成功，非 0 表示失败；input/output 的具体结构由能力契约定义。
VISIONAI_API int visionai_algorithm_process(
    VisionAIAlgorithmHandle handle,
    const void* input,
    void* output);

// 销毁算法实例。实现方应允许 handle 为 NULL，从而简化上层清理逻辑。
VISIONAI_API void visionai_algorithm_destroy(
    VisionAIAlgorithmHandle handle);

#ifdef __cplusplus
}
#endif
