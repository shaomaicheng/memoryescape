#include <jni.h>
#include "android/log.h"
#include "utils.h"
#include "shadowhook.h"


// shadowhook 获取LargeObjectSpace对象
void *los = nullptr;
void *heap = nullptr;

// freelist
void *(*los_alloc_freelist_orig)(void *thiz, void *self, size_t num_bytes, size_t *bytes_allocated,
                                 size_t *usable_size, size_t *bytes_tl_bulk_allocated);

void *los_alloc_freelist_stub = nullptr;

void *los_Alloc_freelist_proxy(void *thiz, void *self, size_t num_bytes, size_t *bytes_allocated,
                               size_t *usable_size, size_t *bytes_tl_bulk_allocated) {
    // log
    __android_log_print(logLevel, TAG,
                        "调用了FreeListSpace:Alloc函数,num_bytes:%lu,bytes_allocated:%lu,usable_size:%lu,bytes_tl_bulk_allocated:%lu",
                        num_bytes, *bytes_allocated, *usable_size, *bytes_tl_bulk_allocated);
    void *orig_ret = (los_alloc_freelist_orig)(thiz, self, num_bytes, bytes_allocated, usable_size,
                                               bytes_tl_bulk_allocated);
    los = thiz;
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
    los_alloc_freelist_stub = shadowhook_hook_sym_name("libart.so", freelist_func_name,
                                                       (void *) los_Alloc_freelist_proxy,
                                                       (void **) &los_alloc_freelist_orig);
    if (los_alloc_freelist_stub != nullptr) {
        __android_log_print(logLevel, TAG, "freelist_los:Alloc hook 成功");
    } else {
        __android_log_print(logLevel, TAG, "freelist_los:Alloc hook 失败");
    }

    los_alloc_map_stub = shadowhook_hook_sym_name("libart.so", map_func_name,
                                                  (void *) los_Alloc_map_proxy,
                                                  (void **) &los_alloc_map_orig);
    if (los_alloc_map_stub != nullptr) {
        __android_log_print(logLevel, TAG, "map_los:Alloc hook 成功");
    } else {
        __android_log_print(logLevel, TAG, "map_los:Alloc hook 失败");
    }
}

void *allocObjectWithAllocator_stub = nullptr;

void (*allocObjectWithAllocator_orig)(void *, void *, void *, size_t, void *, void *) = nullptr;

void allocObjectWithAllocator_proxy(void *thiz, void *self, void *kclass, size_t byte_count,
                                    void *allocator, void *pre_fence_visitor) {
//    __android_log_print(logLevel, TAG, "Heap::AllocObjectWithAllocator");
    heap = thiz;
    allocObjectWithAllocator_orig(thiz, self, kclass, byte_count, allocator, pre_fence_visitor);
}

void hookAllocObjectWithAllocator() {
    const char *hookAllocObjectWithAllocatorName = "_ZN3art2gc4Heap24AllocObjectWithAllocatorILb1ELb1ENS_6mirror21SetStringCountVisitorEEEPNS3_6ObjectEPNS_6ThreadENS_6ObjPtrINS3_5ClassEEEmNS0_13AllocatorTypeERKT1_";
    allocObjectWithAllocator_stub = shadowhook_hook_sym_name("libart.so",
                                                             hookAllocObjectWithAllocatorName,
                                                             (void *) allocObjectWithAllocator_proxy,
                                                             (void **) &allocObjectWithAllocator_orig);
    if (allocObjectWithAllocator_stub != nullptr) {
        __android_log_print(logLevel, TAG, "hook AllocObjectWithAllocator 成功");
    } else {
        __android_log_print(logLevel, TAG, "hook AllocObjectWithAllocator 失败");
    }
}

// 删除los的大小
void deleteLospaceMemory(void *heap, int64_t size) {
    const char *freeFuncName = "_ZN3art2gc4Heap10RecordFreeEml";
    void *art_handler = shadowhook_dlopen("libart.so");
    void *recordFree_func = shadowhook_dlsym(art_handler, freeFuncName);
    __android_log_print(logLevel, TAG, "调用Heap::RecordFree, size:%ld", size);
    ((void (*)(void *, uint64_t, int64_t)) recordFree_func)(heap, -1, size);
}

// 获取largeobjectspace大小的函数 LargeObjectSpace::GetBytesAllocated
// _ZN3art2gc5space16LargeObjectSpace17GetBytesAllocatedEv
// 获取大小后删除这部分大小
void dlopenGetBytesAllocAndDelete() {
    void *art_handler = shadowhook_dlopen("libart.so");
    const char *getBytesAllocName = "_ZN3art2gc5space16LargeObjectSpace17GetBytesAllocatedEv";
    void *getBytesAlloc_func = shadowhook_dlsym(art_handler, getBytesAllocName);
    if (los == nullptr) {
        __android_log_print(logLevel, TAG, "largetobjectspace is null, ignore");
    } else {
        uint64_t allocBytes = ((uint64_t (*)(void *)) getBytesAlloc_func)(los);
        __android_log_print(logLevel, TAG, "largetobjectspace allocBytes:%ld", allocBytes);
        if (heap == nullptr) {
            __android_log_print(logLevel, TAG, "heap is null,ignore");
        } else {
            deleteLospaceMemory(heap, allocBytes);
        }
    }
}

// 初始化后执行hook逻辑
extern "C"
JNIEXPORT void JNICALL
Java_com_example_mylibrary_MemoryEscapeInit_init(JNIEnv *env, jclass clazz) {
    __android_log_print(logLevel, TAG, "内存逃逸init");
    // 1. 获取 LargeObjectSpace 大小
    // 2. 删除 LargeObjectSpace 大小 (使用 Heap::RecordFree)
    // 3. gc内存校验
    hookAlloc();
    // hook
    //_ZN3art2gc4Heap24AllocObjectWithAllocatorILb1ELb1ENS_6mirror21SetStringCountVisitorEEEPNS3_6ObjectEPNS_6ThreadENS_6ObjPtrINS3_5ClassEEEmNS0_13AllocatorTypeERKT1_
    hookAllocObjectWithAllocator();
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_mylibrary_MemoryEscapeInit_getBytesAllocInner(JNIEnv *env, jclass clazz) {
    dlopenGetBytesAllocAndDelete();
}