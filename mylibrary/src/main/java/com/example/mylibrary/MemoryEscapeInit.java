package com.example.mylibrary;

import com.bytedance.shadowhook.ShadowHook;

public class MemoryEscapeInit {
    public static native void init();

    public static void init(boolean enable) {
        if (!enable) {
            return;
        }
        ShadowHook.init(new ShadowHook.ConfigBuilder()
                .setMode(ShadowHook.Mode.UNIQUE)
                .build());
        init();
    }
}
