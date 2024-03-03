package com.example.memoryescape

import android.app.ActivityManager
import android.app.ActivityManager.MemoryInfo
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.example.memoryescape.ui.theme.JvmtidmeoTheme


class MainActivity : ComponentActivity() {
    private val los = arrayListOf<ByteArray>()
    private fun printMemory() {
        Log.e("chenglei_jni", "当前应用占用内存:${Runtime.getRuntime().totalMemory()}")
    }

    var index = 0
    private val runnable = object : Runnable {
        override fun run() {
            Log.e("chenglei","大对象${index+1}")
            los.add(ByteArray(200 * 1024 * 1024))
            printMemory()
            index++
            Handler(Looper.getMainLooper()).postDelayed(this, 2000L)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.layout_second_activity)
        String(ByteArray(10 * 1024))
        Handler(Looper.getMainLooper()).postDelayed(runnable,2*1000L)
    }

}

@Composable
fun Greeting(name: String, modifier: Modifier = Modifier) {
    Text(
        text = "Hello $name!",
        modifier = modifier
    )
}

@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    JvmtidmeoTheme {
        Greeting("Android")
    }
}