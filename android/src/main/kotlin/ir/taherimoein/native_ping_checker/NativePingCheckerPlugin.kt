package ir.taherimoein.native_ping_checker

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.Build
import androidx.annotation.NonNull
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.plugin.common.MethodChannel.Result
import java.net.InetAddress
import kotlinx.coroutines.*

class NativePingCheckerPlugin: FlutterPlugin, MethodCallHandler {
    private lateinit var channel : MethodChannel
    private lateinit var context: Context
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    override fun onAttachedToEngine(@NonNull flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
        channel = MethodChannel(flutterPluginBinding.binaryMessenger, "native_ping_checker")
        channel.setMethodCallHandler(this)
        context = flutterPluginBinding.applicationContext
    }

    override fun onMethodCall(@NonNull call: MethodCall, @NonNull result: Result) {
        if (call.method == "checkInternet") {
            val target = call.argument<String>("target") ?: "8.8.8.8"
            val pingCount = call.argument<Int>("pingCount") ?: 5
            val timeoutMs = call.argument<Int>("timeoutMs") ?: 2000
            val minSuccess = call.argument<Int>("minSuccess") ?: 5

            scope.launch {
                val isOnline = checkInternetWithPing(target, pingCount, timeoutMs, minSuccess)
                withContext(Dispatchers.Main) {
                    result.success(isOnline)
                }
            }
        } else {
            result.notImplemented()
        }
    }

    private fun checkInternetWithPing(target: String, pingCount: Int, timeoutMs: Int, minSuccess: Int): Boolean {
        // First check if network is available
        if (!isNetworkAvailable()) {
            return false
        }

        var successCount = 0

        for (i in 0 until pingCount) {
            try {
                val address = InetAddress.getByName(target)
                val reachable = address.isReachable(timeoutMs)
                if (reachable) {
                    successCount++
                }
            } catch (e: Exception) {
                // Ping failed, continue to next attempt
            }
        }

        return successCount >= minSuccess
    }

    private fun isNetworkAvailable(): Boolean {
        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val network = connectivityManager.activeNetwork ?: return false
            val capabilities = connectivityManager.getNetworkCapabilities(network) ?: return false
            return capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
        } else {
            @Suppress("DEPRECATION")
            val networkInfo = connectivityManager.activeNetworkInfo
            @Suppress("DEPRECATION")
            return networkInfo?.isConnected == true
        }
    }

    override fun onDetachedFromEngine(@NonNull binding: FlutterPlugin.FlutterPluginBinding) {
        channel.setMethodCallHandler(null)
        scope.cancel()
    }
}
