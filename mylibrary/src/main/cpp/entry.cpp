#include <jni.h>
#include "android/log.h"
#include "utils.h"
#include "shadowhook.h"


// shadowhook 获取LargeObjectSpace对象
void *los = nullptr;

// freelist
void *(*los_alloc_freelist_orig)(void *thiz, void *self, size_t num_bytes, size_t bytes_allocated,
                        size_t *usable_size, size_t *bytes_tl_bulk_allocated) = nullptr;

void *los_alloc_freelist_stub = nullptr;

void *los_Alloc_freelist_proxy(void *thiz, void *self, size_t num_bytes, size_t bytes_allocated,
                      size_t *usable_size, size_t *bytes_tl_bulk_allocated) {
    // log
    __android_log_print(logLevel, TAG, "调用了FreeListSpace：Alloc函数");
    void *orig_ret = (los_alloc_freelist_orig)(thiz, self, num_bytes, bytes_allocated, usable_size,
                                      bytes_tl_bulk_allocated);
    return orig_ret;
}
// map
void *(*los_alloc_map_orig)(void *thiz, void *self, size_t num_bytes, size_t bytes_allocated,
                                 size_t *usable_size, size_t *bytes_tl_bulk_allocated) = nullptr;

void *los_alloc_map_stub = nullptr;

void *los_Alloc_map_proxy(void *thiz, void *self, size_t num_bytes, size_t bytes_allocated,
                               size_t *usable_size, size_t *bytes_tl_bulk_allocated) {
    // log
    __android_log_print(logLevel, TAG, "调用了LargeObjectMapSpace：Alloc函数");
    void *orig_ret = (los_alloc_map_orig)(thiz, self, num_bytes, bytes_allocated, usable_size,
                                               bytes_tl_bulk_allocated);
    return orig_ret;
}

void hookAlloc() {
    const char *freelist_func_name = "_ZN3art2gc5space13FreeListSpace5AllocEPNS_6ThreadEmPmS5_S5_"; // FreeListSpace
    const char *map_func_name = "_ZThn296_N3art2gc5space19LargeObjectMapSpace5AllocEPNS_6ThreadEmPmS5_S5_"; // LargeObjectMapSpace
    los_alloc_freelist_stub = shadowhook_hook_sym_name("libart.so", freelist_func_name, (void *) los_Alloc_freelist_proxy,
                                              (void **) &los_alloc_freelist_stub);
    if (los_alloc_freelist_stub != nullptr) {
        __android_log_print(logLevel, TAG, "freelist_los:Alloc hook 成功");
    } else {
        __android_log_print(logLevel, TAG, "freelist_los:Alloc hook 失败");
    }

    los_alloc_map_stub = shadowhook_hook_sym_name("libart.so", map_func_name, (void *) los_Alloc_map_proxy,
                                                       (void **) &los_alloc_map_stub);
    if (los_alloc_map_stub != nullptr) {
        __android_log_print(logLevel, TAG, "map_los:Alloc hook 成功");
    } else {
        __android_log_print(logLevel, TAG, "map_los:Alloc hook 失败");
    }
}




// 获取largeobjectspace大小的函数
// _ZN3art2gc5space16LargeObjectSpace17GetBytesAllocatedEv



// 初始化后执行hook逻辑
extern "C"
JNIEXPORT void JNICALL
Java_com_example_mylibrary_MemoryEscapeInit_init(JNIEnv *env, jclass clazz) {
    __android_log_print(logLevel, TAG, "内存逃逸init");
    // 1. 获取 LargeObjectSpace 大小
    // 2. 删除 LargeObjectSpace 大小 (使用 Heap::RecordFree)
    // 3. gc内存校验
    hookAlloc();
}