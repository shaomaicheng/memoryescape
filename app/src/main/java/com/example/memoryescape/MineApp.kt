package com.example.memoryescape

import android.app.Application
import com.example.mylibrary.MemoryEscapeInit

/**
 * @author chenglei01
 * @date 2023/9/16
 * @time 17:51
 */
class MineApp:Application() {
    override fun onCreate() {
        super.onCreate()
        MemoryEscapeInit.init(true)
    }
}