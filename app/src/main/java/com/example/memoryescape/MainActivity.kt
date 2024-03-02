package com.example.memoryescape

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.core.os.postDelayed
import com.example.memoryescape.ui.theme.JvmtidmeoTheme
import com.example.mylibrary.MemoryEscapeInit

class MainActivity : ComponentActivity() {
    private var intArray : Array<Int>? = null

    private val runnable = object : Runnable {
        override fun run() {
            MemoryEscapeInit.getBytesAlloc()
//            Handler(Looper.getMainLooper()).postDelayed(this,2000L)
        }
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            JvmtidmeoTheme {
                // A surface container using the 'background' color from the theme
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    Greeting("Android", modifier = Modifier.clickable {
                    })
                }
            }
        }
        StringWrapper("StringWrapper")
        Log.e("chenglei","分配一个数组")
        intArray = arrayOf(1,2,3,5,6)
        Handler(Looper.getMainLooper()).postDelayed(runnable,2*1000L)
    }

}


class StringWrapper(val string: String) {

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