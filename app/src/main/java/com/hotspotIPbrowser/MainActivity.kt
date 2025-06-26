package com.hotspotIPbrowser

//import android.util.Log
import android.annotation.SuppressLint
import com.hotspotIPbrowser.MusicNotificationListenerService
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.edit
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import java.net.NetworkInterface
import java.util.Collections
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout

class MainActivity : AppCompatActivity() {
    private lateinit var swipeRefreshLayout: SwipeRefreshLayout
    private lateinit var myWebView: WebView
    private var retryCount = 0
    private val handler = Handler(Looper.getMainLooper())
    private var newLastSegment = 89 // Default value, will be loaded or set by user
    private val perfName = "AppSettings"
    private val prefLastSegment = "newLastSegment"
    private val prefLastIpAddress = "lastIpAddress"
    private lateinit var songOverlayTextView: TextView
    private val perfNotificationPrompt = "notification_prompted"
    private var notificationPrompted = false
    private var musicAppPackage: String? = null
    private lateinit var gestureDetector: GestureDetector
    private val musicInfoReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            var songInfo = intent?.getStringExtra("songInfo")
            musicAppPackage = intent?.getStringExtra("packageName") // Get the package name
            songOverlayTextView.text = songInfo
            songOverlayTextView.visibility =
                if (songInfo.isNullOrEmpty()) View.GONE else View.VISIBLE
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        swipeRefreshLayout = findViewById(R.id.swipeRefreshLayout)
        myWebView = findViewById(R.id.webview)
        songOverlayTextView = findViewById(R.id.songOverlayTextView)

        // Initialize GestureDetector
        gestureDetector = GestureDetector(this, object : GestureDetector.SimpleOnGestureListener() {
            private val SWIPE_THRESHOLD = 100
            private val SWIPE_VELOCITY_THRESHOLD = 100

            override fun onFling(
                e1: MotionEvent?,
                e2: MotionEvent,
                velocityX: Float,
                velocityY: Float
            ): Boolean {
                if (e1 == null || e2 == null) {
                    return false
                }
                val diffY = e2!!.y - e1!!.y
                val diffX = e2.x - e1.x

                // Check for vertical swipe up
                if (Math.abs(diffX) < Math.abs(diffY) && Math.abs(diffY) > SWIPE_THRESHOLD && Math.abs(velocityY) > SWIPE_VELOCITY_THRESHOLD) {
                    if (diffY > 0) {
                        // Swipe down (handled by SwipeRefreshLayout)
                        return false
                    } else {
                        // Swipe up
                        launchMusicPlayer()
                        return true
                    }
                }
                return false
            }

            override fun onScroll(
                e1: MotionEvent?,
                e2: MotionEvent,
                distanceX: Float,
                distanceY: Float
            ): Boolean {
                if (e1 == null || e2 == null) {
                    return false
                }

                val diffY = e2.y - e1.y
                val diffX = e2.x - e1.x

                // If a vertical swipe up is in progress, consume the event
                // This prevents WebView scrolling during the "drag" phase of a potential swipe up.
                if (Math.abs(diffX) < Math.abs(diffY) && diffY < 0) { // Swipe up
                    return true // Consume event
                }
                return false // Otherwise, let WebView handle scrolling
            }


        })

        // Attach touch listener to the WebView to detect gestures
        myWebView.setOnTouchListener { v, event ->
            // Pass the event to the gesture detector
            val handledByGestureDetector = gestureDetector.onTouchEvent(event)

            // If the gesture detector handled an event (like swipe up),
            // return true to consume it and prevent WebView scrolling.
            // Otherwise, return false to let the WebView handle its own touch events (like scrolling, clicks).
            handledByGestureDetector
        }


        // Load stored newLastSegment or ask user for input
        val sharedPrefs = getSharedPreferences(perfName, MODE_PRIVATE)
        newLastSegment = sharedPrefs.getInt(prefLastSegment, 89) // Load, default is 89

        // If it is the first time, or you want to ask every time.
        if (!sharedPrefs.contains(prefLastSegment)) {
            askForLastSegment() //show dialog
        }

        // Configure WebView
        myWebView.settings.javaScriptEnabled = true
        myWebView.webViewClient = object : WebViewClient() {
            override fun onReceivedError(
                view: WebView?,
                errorCode: Int,
                description: String?,
                failingUrl: String?
            ) {
                swipeRefreshLayout.isRefreshing = false
                showErrorPage("$description</br>", failingUrl)
                handleLoadError()
            }

            override fun onPageFinished(view: WebView?, url: String?) {
                super.onPageFinished(view, url)
                // Page has finished loading, make the WebView visible
                myWebView.visibility = View.VISIBLE
                swipeRefreshLayout.isRefreshing = false
            }

            override fun onReceivedHttpError(
                view: WebView?,
                request: WebResourceRequest?,
                errorResponse: WebResourceResponse?
            ) {
                val message =
                    "</br>Error: ${errorResponse?.statusCode} - ${errorResponse?.reasonPhrase}"
                showErrorPage(message, request?.url?.toString())
                handleLoadError()
            }
        }
        // Set the listener for pull-to-refresh action
        swipeRefreshLayout.setOnRefreshListener {
            // When the user pulls down, reload the WebView
            myWebView.visibility = View.GONE
            loadUrl(getWifiIpAddress())
            //myWebView.reload()
            // The isRefreshing will be set to false in onPageFinished
        }

        // Register the BroadcastReceiver
        LocalBroadcastManager.getInstance(this).registerReceiver(
            musicInfoReceiver,
            IntentFilter("com.hotspotIPbrowser.MUSIC_INFO_UPDATE")
        )

        loadUrl(getWifiIpAddress())

        // Fullscreen mode
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            window.insetsController?.let { insetsController ->
                insetsController.hide(WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars())
                insetsController.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            // For older Android versions, use the deprecated method
            window.decorView.systemUiVisibility =
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        }
    }

    private fun showErrorPage(errorMessage: String, failingUrl: String? = null) {
        val errorHtml = """
<html>
<head>
    <style>
        body {
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            background-color: #141414;
            flex-direction: column;
        }
        .header .logo {
            text-align: center;
        }
        .error-message {
            text-align: center;
            color: #A3C1AD;
        }
    </style>
</head>
<body>
    <div class="logo">
        <p>
            <img id="prius" src="https://upload.wikimedia.org/wikipedia/commons/8/89/Toyota_Prius_logo.svg" width="420"
                height="70" style="filter: invert(0.45) sepia(1) saturate(1) hue-rotate(60deg);">
        </p>
    </div>
    <div class="error-message">
        <h1>Error</h1>
        <p> $errorMessage </p>
        ${if (failingUrl != null) "<p>URL: $failingUrl</p>" else ""}
    </div>
</body>
</html>
        """.trimIndent()
        myWebView.loadDataWithBaseURL(null, errorHtml, "text/html", "utf-8", null)
    }

    private fun loadUrl(ip: String?) {
        myWebView.loadUrl("http://$ip")
        // Store the successfully loaded IP address
        getSharedPreferences(perfName, MODE_PRIVATE).edit {
            putString(prefLastIpAddress, ip)
        }
    }

    private fun handleLoadError() {
        if (retryCount < 10) {
            retryCount++
        }
        handler.postDelayed({
            loadUrl(getWifiIpAddress())
        }, 2000L * retryCount) // Exponential backoff
    }

    private fun getWifiIpAddress(): String? {
        try {
            val interfaces = NetworkInterface.getNetworkInterfaces()
            for (interfc in Collections.list(interfaces)) {
                for (addr in Collections.list(interfc.inetAddresses)) {
                    if (addr.hostAddress?.startsWith("192.168.") == true ||
                        addr.hostAddress?.startsWith("172.") == true
                    ) {
                        return modifyLastIpSegment(
                            ip = addr.hostAddress,
                            newLastSegment = newLastSegment
                        )
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return "127.0.0.1"
    }

    private fun modifyLastIpSegment(ip: String?, newLastSegment: Int): String? {
        if (ip == null) return null
        val segments = ip.split(".")
        if (segments.size != 4) return ip
        return "${segments[0]}.${segments[1]}.${segments[2]}.$newLastSegment"
    }

    override fun onDestroy() {
        super.onDestroy()
        // Clean up handler to prevent memory leaks
        handler.removeCallbacksAndMessages(null)
        LocalBroadcastManager.getInstance(this).unregisterReceiver(musicInfoReceiver)
    }


    private fun askForLastSegment() {
        val builder = AlertDialog.Builder(this)
        builder.setTitle("Enter WebServer Last IP Octet")
        builder.setMessage(
            "Please enter the last part (number) of the WebServer IP address. For example, if you use an ESP32 and its IP address is 192.168.1.89, enter '89'.\n" +
                    "To change this number in the future, you will need to clear the app's data or reinstall the app."
        )

        // Set up the input
        val input = android.widget.EditText(this)
        input.inputType = android.text.InputType.TYPE_CLASS_NUMBER
        builder.setView(input)

        // Set up the buttons
        builder.setPositiveButton("OK") { dialog, which ->
            try {
                newLastSegment = input.text.toString().toInt()
                if (newLastSegment > 255 || newLastSegment < 0) {
                    throw NumberFormatException()
                    // Toast.makeText(this, "Invalid IP octet, default 89 used", Toast.LENGTH_LONG).show()
                }
                // Store the user's input
                val sharedPrefs = getSharedPreferences(perfName, MODE_PRIVATE)
                sharedPrefs.edit {
                    putInt(prefLastSegment, newLastSegment)
                } //or commit
                loadUrl(getWifiIpAddress())//reload
            } catch (_: NumberFormatException) {
                // Handle the exception if the input is not a valid number
                Toast.makeText(
                    this,
                    "Invalid input. Number must be between 1 and 255",
                    Toast.LENGTH_LONG
                ).show()
                askForLastSegment()
            }
        }

        builder.setNegativeButton("Cancel") { dialog, which ->
            dialog.cancel()
            loadUrl(getWifiIpAddress())//load with default
        }

        builder.show()
    }

    private fun requestNotificationAccess() {
        val intent = Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)
        startActivity(intent)
    }

    override fun onResume() {
        super.onResume()

        // --- IP Change Detection Logic ---
        val sharedPrefs = getSharedPreferences(perfName, MODE_PRIVATE)
        val lastStoredIp = sharedPrefs.getString(prefLastIpAddress, null)
        val currentWifiIp = getWifiIpAddress()

        if (currentWifiIp != null && currentWifiIp != lastStoredIp) {
            // IP address has changed, reload the WebView
            loadUrl(currentWifiIp)
        } else if ((currentWifiIp == null || currentWifiIp == "127.0.0.1")  && lastStoredIp != null) {
            // Lost IP, or cannot determine it, and we previously had one.
            // You might want to display an error or attempt to reload default.
            // For now, let's just attempt to load the error page.
            showErrorPage("Could not determine local IP address. Please check Wi-Fi connection.")
        }
        // --- End IP Change Detection Logic ---

        notificationPrompted = sharedPrefs.getBoolean(perfNotificationPrompt, false)

        if (!isNotificationServiceEnabled()) {
            if (!notificationPrompted) {
                requestNotificationAccess()
                sharedPrefs.edit { putBoolean(perfNotificationPrompt, true) }
            } else {
                Toast.makeText(
                    this,
                    "Please grant notification access for music info.",
                    Toast.LENGTH_LONG
                ).show()
                sharedPrefs.edit { putBoolean(perfNotificationPrompt, false) }
            }

        } else {
            startService(Intent(this, MusicNotificationListenerService::class.java))
        }
    }

    private fun isNotificationServiceEnabled(): Boolean {
        val packageName = packageName
        val flat = Settings.Secure.getString(contentResolver, "enabled_notification_listeners")
        if (!flat.isNullOrEmpty()) {
            val listeners = flat.split(":")
            for (listener in listeners) {
                val cn = android.content.ComponentName.unflattenFromString(listener)
                if (cn != null && cn.packageName == packageName) {
                    return true
                }
            }
        }
        return false
    }
    private fun launchMusicPlayer() {
        if (musicAppPackage != null) {
            val launchIntent = packageManager.getLaunchIntentForPackage(musicAppPackage!!)
            if (launchIntent != null) {
                startActivity(launchIntent)
            } else {
                Toast.makeText(this, "Could not launch music player.", Toast.LENGTH_SHORT).show()
            }
        } else {
            Toast.makeText(this, "No music player package known yet.", Toast.LENGTH_SHORT).show()
        }
    }
}

