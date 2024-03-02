package com.example.mylibrary;

import com.bytedance.shadowhook.ShadowHook;

public class MemoryEscapeInit {
    public static native void init();
    public static native void getBytesAllocInner();
    public static void getBytesAlloc() {
        getBytesAllocInner();
    }

    public static void init(boolean enable) {
        if (!enable) {
            return;
        }
        System.loadLibrary("memoryescape");
        ShadowHook.init(new ShadowHook.ConfigBuilder()
                .setMode(ShadowHook.Mode.UNIQUE)
                        .setDebuggable(true)
                .build());
        init();
    }
}
