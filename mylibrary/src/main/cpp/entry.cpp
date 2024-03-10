#include <jni.h>
#include "android/log.h"
#include "utils.h"
#include "shadowhook.h"


// shadowhook 获取LargeObjectSpace对象
void *los = nullptr;
void *g_heap = nullptr;


// 获取largeobjectspace大小的函数 LargeObjectSpace::GetBytesAllocated
// _ZN3art2gc5space16LargeObjectSpace17GetBytesAllocatedEv
uint64_t dlopenGetBytesAllocInner() {
    void *art_handler = shadowhook_dlopen("libart.so");
    const char *getBytesAllocName = "_ZN3art2gc5space16LargeObjectSpace17GetBytesAllocatedEv";
    void *getBytesAlloc_func = shadowhook_dlsym(art_handler, getBytesAllocName);
    uint64_t allocBytes = ((uint64_t (*)(void *)) getBytesAlloc_func)(los);
//    __android_log_print(logLevel, TAG, "largetobjectspace allocBytes:%ld", allocBytes);
    return allocBytes;
}

// 删除los的大小
void deleteLospaceMemory(void* heap, int64_t size) {
    const char *freeFuncName = "_ZN3art2gc4Heap10RecordFreeEml";
    void *art_handler = shadowhook_dlopen("libart.so");
    void *recordFree_func = shadowhook_dlsym(art_handler, freeFuncName);
    __android_log_print(logLevel, TAG, "调用Heap::RecordFree, size:%ld", size);
    ((void (*)(void *, uint64_t, int64_t)) recordFree_func)(heap, -1, size);
}


// 获取大小后删除这部分大小
void dlopenGetBytesAllocAndDelete() {
    uint64_t allocBytes = dlopenGetBytesAllocInner();
    if (g_heap == nullptr) {
        __android_log_print(logLevel, TAG, "g_heap is null,ignore");
    } else {
        deleteLospaceMemory(g_heap, allocBytes);
    }
}


// freelist
void *(*los_alloc_freelist_orig)(void *thiz, void *self, size_t num_bytes, size_t *bytes_allocated,
                                 size_t *usable_size, size_t *bytes_tl_bulk_allocated);

void *los_alloc_freelist_stub = nullptr;

void *los_Alloc_freelist_proxy(void *thiz, void *self, size_t num_bytes, size_t *bytes_allocated,
                               size_t *usable_size, size_t *bytes_tl_bulk_allocated) {
    // log
//    __android_log_print(logLevel, TAG,
//                        "大对象分配内存，调用了FreeListSpace:Alloc函数,num_bytes:%lu",num_bytes);
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

void *free_list_free_stub = nullptr;
size_t (*free_list_free_orig)(void*,void*,void*) = nullptr;

size_t free_list_free_proxy(void* thiz, void* self, void* obj) {
    size_t freeSize = free_list_free_orig(thiz,self,obj);
    __android_log_print(logLevel, TAG, "大对象回收，需要逆补偿内存size：%lu", freeSize);
    deleteLospaceMemory(g_heap, -freeSize);
    return freeSize;
}

void hookFree() {
    const char * freeList_free_name = "_ZThn296_N3art2gc5space13FreeListSpace4FreeEPNS_6ThreadEPNS_6mirror6ObjectE";
    free_list_free_stub = shadowhook_hook_sym_name("libart.so", freeList_free_name, (void*) free_list_free_proxy, (void**)(&free_list_free_orig));
    if (free_list_free_stub != nullptr) {
        __android_log_print(logLevel, TAG, "freeListSpace::Free hook成功");
    } else {
        __android_log_print(logLevel, TAG, "freeListSpace::Free hook失败");
    }
}

// hook oom 抛出的地方
bool interceptOOM = false;
bool tryAgain = false;
void *throw_oom_stub = nullptr;
void (*throw_oom_orig)(void*,void*,size_t, void*);

uint64_t oldSize = 0;

void throw_oom_proxy(void* thiz,void * self,size_t byte_count, void* allocator_type) {
    __android_log_print(logLevel, TAG, "发生了oom");
    interceptOOM = true;
    if (!tryAgain) {
        __android_log_print(logLevel,TAG,"不是在gc后触发的oom,开启逃逸");
        if (los != nullptr) {
            uint64_t currentLosSize = dlopenGetBytesAllocInner();
            if (currentLosSize > oldSize) {
                uint64_t freeSize = currentLosSize - oldSize;
                deleteLospaceMemory(thiz, freeSize); // 释放大对象内存统计
                __android_log_print(logLevel,TAG,"本次逃逸内存大小为:%lu", freeSize);
                oldSize = currentLosSize;
            }
        } else {
            __android_log_print(logLevel,TAG,"los obj is null");
        }
    } else {
        __android_log_print(logLevel, TAG, "不拦截oom,直接抛出");
        interceptOOM = false;
        throw_oom_orig(thiz, self, byte_count, allocator_type);
    }
}

void hookThrowOOM() {
    const char *throw_oom_name = "_ZN3art2gc4Heap21ThrowOutOfMemoryErrorEPNS_6ThreadEmNS0_13AllocatorTypeE";
    throw_oom_stub = shadowhook_hook_sym_name("libart.so", throw_oom_name, (void*) throw_oom_proxy, (void**)&throw_oom_orig);
    if (throw_oom_stub != nullptr) {
        __android_log_print(logLevel,TAG, "ThrowOutOfMemoryError hook 成功");
    } else {
        __android_log_print(logLevel,TAG, "ThrowOutOfMemoryError hook 失败");
    }
}

// hook hookAllocateInternalWithGc, 内存不够的时候本身会在oom之前尝试gc后分配内存
// 如果返回null，说明才会继续oom

void* hookAllocateInternalWithGc_stub = nullptr;
void* (*hookAllocateInternalWithGc_orig)(void*,void*,void*,bool ,size_t, size_t*, size_t*, size_t*, void*);

void *AllocateInternalWithGc_proxy(void* thiz,void* self,void* allocator,bool instrumented,size_t num_bytes,size_t* bytes_allocated,size_t* usable_size, size_t * bytes_tl_bulk_allocated,void* klass)
{
    __android_log_print(logLevel, TAG, "AllocateInternalWithGc 调用");
    void* object = hookAllocateInternalWithGc_orig(thiz,self,allocator,instrumented,num_bytes,bytes_allocated,usable_size,bytes_tl_bulk_allocated,klass);
    tryAgain = false;
    if (object == nullptr && interceptOOM) {
        __android_log_print(logLevel,TAG,"AllocateInternalWithGc分配失败，重新分配一次");
        tryAgain = true;
        // 重试一次分配
        object = hookAllocateInternalWithGc_orig(thiz,self,allocator,instrumented,num_bytes,bytes_allocated,usable_size,bytes_tl_bulk_allocated,klass);
        if (object == nullptr) {
            __android_log_print(logLevel,TAG,"重试分配也失败，等死");
            tryAgain = false;
            return object;
        } else {
            __android_log_print(logLevel,TAG,"重试分配内存成功");
        }
        if (los != nullptr) {
            // gc后获取一次大对象空间
            uint64_t losSize = dlopenGetBytesAllocInner();
            __android_log_print(logLevel, TAG, "gc后获取一次大对象space的size:%ld,oldSize:%ld",losSize,oldSize);
            if (losSize < oldSize) {
                // 被gc回收
                uint64_t sizeByGc = oldSize - losSize;
                deleteLospaceMemory(thiz, -sizeByGc); // 统计加回来这部分
                __android_log_print(logLevel, TAG, "发生了gc导致大对象空间回收,回收size:%ld", sizeByGc);
            }
        }
        tryAgain = false;
    }
    return object;
}

void hookAllocateInternalWithGc() {
    const char *fun_name = "_ZN3art2gc4Heap22AllocateInternalWithGcEPNS_6ThreadENS0_13AllocatorTypeEbmPmS5_S5_PNS_6ObjPtrINS_6mirror5ClassEEE";
    hookAllocateInternalWithGc_stub = shadowhook_hook_sym_name("libart.so", fun_name, (void*)AllocateInternalWithGc_proxy, (void**)&hookAllocateInternalWithGc_orig);
    if (hookAllocateInternalWithGc_stub != nullptr) {
        __android_log_print(logLevel,TAG, "AllocateInternalWithGc hook成功");
    } else {
        __android_log_print(logLevel,TAG, "AllocateInternalWithGc hook失败");
    }
}


void *allocObjectWithAllocator_stub = nullptr;

void (*allocObjectWithAllocator_orig)(void *, void *, void *, size_t, void *, void *) = nullptr;

void allocObjectWithAllocator_proxy(void *thiz, void *self, void *kclass, size_t byte_count,
                                    void *allocator, void *pre_fence_visitor) {
//    __android_log_print(logLevel, TAG, "Heap::AllocObjectWithAllocator");
    g_heap = thiz;
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

// hookGrowForUtilization
void *growForUtilization_stub = nullptr;
void (*growForUtilization_orig)(void*,void*,size_t);

void growForUtilization_proxy(void* thiz, void* collector_ran, size_t bytes_allocated_before_gc) {
//    __android_log_print(logLevel,TAG,"growForUtilization调用");
    growForUtilization_orig(thiz, collector_ran, 0);
}

void hookGrowForUtilization() {
    const char *fun_name = "_ZN3art2gc4Heap18GrowForUtilizationEPNS0_9collector16GarbageCollectorEm";
    growForUtilization_stub = shadowhook_hook_sym_name("libart.so", fun_name, (void*)growForUtilization_proxy,(void**)&growForUtilization_orig);
    if (growForUtilization_stub != nullptr) {
        __android_log_print(logLevel, TAG, "growForUtilization hook 成功");
    } else {
        __android_log_print(logLevel, TAG, "growForUtilization hook 失败");
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
    // hook free,进行size的补偿
//    hookFree();
    hookThrowOOM();
    hookAllocateInternalWithGc();
    // hook
    //_ZN3art2gc4Heap24AllocObjectWithAllocatorILb1ELb1ENS_6mirror21SetStringCountVisitorEEEPNS3_6ObjectEPNS_6ThreadENS_6ObjPtrINS3_5ClassEEEmNS0_13AllocatorTypeERKT1_
    hookAllocObjectWithAllocator();
    hookGrowForUtilization();
    // hook完成之后记录下初始的大对象空间大小
//    oldSize = dlopenGetBytesAllocInner();
//    __android_log_print(logLevel,TAG,"los初始size:%lu", oldSize);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_mylibrary_MemoryEscapeInit_getBytesAllocInner(JNIEnv *env, jclass clazz) {
//    dlopenGetBytesAllocAndDelete();
}

// hook RegionSpace::Alloc
void* regionSpaceAllocStub = nullptr;
void* (*regionSpaceAlloc_orig)(void*,size_t,size_t*,size_t*,size_t*);

void* regionSpaceAlloc_proxy(void* thiz, size_t num_bytes, size_t* bytes_allocated, size_t* usable_size, size_t* bytes_tl_bulk_allocated) {
    __android_log_print(logLevel,TAG,"RegionSpace::Alloc调用");
    return regionSpaceAlloc_orig(thiz,num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
}

void hookRegionSpaceAlloc() {
    regionSpaceAllocStub = shadowhook_hook_sym_name("libart.so", "_ZN3art2gc5space11RegionSpace5AllocEPNS_6ThreadEmPmS5_S5_",
                                                    (void*)regionSpaceAlloc_proxy,(void**) &regionSpaceAlloc_orig);
    if (regionSpaceAllocStub == nullptr) {
        __android_log_print(logLevel, TAG, "RegionSpace::Alloc hook成功");
    } else {
        __android_log_print(logLevel, TAG, "RegionSpace::Alloc hook失败");
    }
}


// hook RosAllocSpace::Alloc
void* rosAllocSpaceAllocStub = nullptr;

void* (*rosAllocSpaceAlloc_orig)(void* thiz, void* self, size_t num_bytes, size_t* bytes_allocated, size_t* usable_size,size_t* bytes_tl_bulk_allocated);

void* rosAllocSpaceAlloc_proxy(void* thiz, void* self, size_t num_bytes, size_t* bytes_allocated, size_t* usable_size,size_t* bytes_tl_bulk_allocated) {
    __android_log_print(logLevel,TAG,"RosAllocSpace::Alloc调用");
    return rosAllocSpaceAlloc_orig(thiz, self, num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
}

void hookRosAllocSpaceAlloc() {
    rosAllocSpaceAllocStub = shadowhook_hook_sym_name("libart.so", "_ZN3art2gc5space13RosAllocSpace5AllocEPNS_6ThreadEmPmS5_S5_",
                                                      (void*)rosAllocSpaceAlloc_proxy, (void**) &rosAllocSpaceAlloc_orig);
    if (rosAllocSpaceAllocStub != nullptr) {
        __android_log_print(logLevel, TAG, "RosAllocSpace::Alloc hook成功");
    } else {
        __android_log_print(logLevel, TAG, "RosAllocSpace::Alloc hook失败");
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_mylibrary_MemoryEscapeInit_studyInit(JNIEnv *env, jclass clazz) {
    // 这里hook下其他space的分配，看下正常的java对象是哪个space去分配的
    hookRegionSpaceAlloc();
    hookRosAllocSpaceAlloc();
}
