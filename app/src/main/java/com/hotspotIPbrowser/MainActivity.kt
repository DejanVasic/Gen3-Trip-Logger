package com.hotspotIPbrowser

import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.os.Build
import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.view.GestureDetector
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import android.webkit.WebSettings
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.edit
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.net.InetSocketAddress
import java.net.NetworkInterface
import java.net.Socket
import java.util.Collections
import android.content.ComponentName
import android.support.v4.media.MediaBrowserCompat
import android.support.v4.media.session.MediaControllerCompat
import android.support.v4.media.session.PlaybackStateCompat

class MainActivity : AppCompatActivity() {
    private lateinit var swipeRefreshLayout: SwipeRefreshLayout
    private lateinit var myWebView: WebView
    private val handler = Handler(Looper.getMainLooper()) // Used for general delays/error retries

    private var newLastSegment = 89 // Default value, will be loaded or set by user
    private val perfName = "AppSettings"
    private val prefLastSegment = "newLastSegment"
    private val prefLastIpAddress = "lastIpAddress"
    private val prefLastMusicApp = "lastMusicApp"
    private lateinit var songOverlayTextView: TextView
    private val perfNotificationPrompt = "notification_prompted"
    private var notificationPrompted = false
    private var musicAppPackage: String? = null
    private lateinit var gestureDetector: GestureDetector
    private var isScreenOnReceiverRegistered = false
    private val connectionTimeoutMs = 3000 // 3 seconds timeout for connection check
    private val targetPort = 80 // Default HTTP port

    // --- NEW PROPERTIES FOR NETWORK LISTENER ---
    private var connectivityManager: ConnectivityManager? = null
    private lateinit var networkCallback: ConnectivityManager.NetworkCallback
    // -------------------------------------------


    private val musicInfoReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            var songInfo = intent?.getStringExtra("songInfo")
            musicAppPackage = intent?.getStringExtra("packageName") // Get the package name
            songOverlayTextView.text = songInfo
            songOverlayTextView.visibility = if (songInfo.isNullOrEmpty()) View.GONE else View.VISIBLE
            // Save the last music app package
            if (!musicAppPackage.isNullOrEmpty()) {
                getSharedPreferences(perfName, MODE_PRIVATE).edit {
                    putString(prefLastMusicApp, musicAppPackage)
                }
            }
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {

        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        swipeRefreshLayout = findViewById(R.id.swipeRefreshLayout)
        myWebView = findViewById(R.id.webview)
        songOverlayTextView = findViewById(R.id.songOverlayTextView)
        myWebView.setBackgroundColor(android.graphics.Color.parseColor("#141414"))

        // Initialize GestureDetector for single-finger input
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

                // Check for vertical swipe up (single-finger music launch)
                if (Math.abs(diffX) < Math.abs(diffY) && Math.abs(diffY) > SWIPE_THRESHOLD && Math.abs(velocityY) > SWIPE_VELOCITY_THRESHOLD) {
                    if (diffY > 0) {
                        // Swipe down (handled by SwipeRefreshLayout)
                        return false
                    } else {
                        // Swipe up: Launch Music Player
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
                if (Math.abs(diffX) < Math.abs(diffY) && diffY < 0) { // Swipe up
                    return true // Consume event to prevent WebView scrolling
                }
                return false // Otherwise, let WebView handle scrolling
            }


        })

        // Attach touch listener to the WebView to detect gestures
        myWebView.setOnTouchListener { v, event ->

            // Disable swipe-to-refresh on multitouch to prevent interference with zoom/scrolling
            // If pointer count > 1, the user is likely trying to zoom or multi-scroll, which should bypass the pull-to-refresh logic.
            if (event.pointerCount > 1) {
                swipeRefreshLayout.isEnabled = false
                return@setOnTouchListener false // Pass multitouch event to WebView for native handling
            }

            // Re-enable SwipeRefreshLayout when the touch sequence ends (single or multi-finger).
            if (event.action == MotionEvent.ACTION_UP || event.action == MotionEvent.ACTION_CANCEL) {
                swipeRefreshLayout.isEnabled = true
            }

            // Pass the event to the gesture detector (only for single-finger input)
            val handledByGestureDetector = gestureDetector.onTouchEvent(event)

            // If the gesture detector handled the event (e.g., single-finger swipe up), return true to consume it.
            // Otherwise, return false to let the WebView handle it (like normal single-finger scrolling/tap/swipe down).
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
        myWebView.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true
            loadWithOverviewMode = true
            useWideViewPort = true
            //cacheMode = WebSettings.LOAD_NO_CACHE
            setSupportMultipleWindows(false)
        }
        myWebView.keepScreenOn = true
        //myWebView.settings.javaScriptEnabled = true
        myWebView.webViewClient = object : WebViewClient() {
            override fun onReceivedError(
                view: WebView?,
                errorCode: Int,
                description: String?,
                failingUrl: String?
            ) {
                swipeRefreshLayout.isRefreshing = false
                showErrorPage("$description${errorCode.toString()}<br>", failingUrl)
                handleLoadError(errorCode)
            }

            override fun onPageFinished(view: WebView?, url: String?) {
                super.onPageFinished(view, url)
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
                handleLoadError(0)
            }
        }
        // Set the listener for pull-to-refresh action
        swipeRefreshLayout.setOnRefreshListener {
            val currentUrl = myWebView.url
            val path = currentUrl?.let { android.net.Uri.parse(it).path }
            val isRoot = path.isNullOrEmpty() || path == "/"

            if (!isRoot && currentUrl != null && currentUrl.startsWith("http")) {
                // non-root page (/debug, /heap, etc.)
                myWebView.reload()
            } else {
                // Root or error page
                checkAndLoadUrl(getWifiIpAddress(0))
            }
        }
        // Register BroadcastReceiver
        LocalBroadcastManager.getInstance(this).registerReceiver(
            musicInfoReceiver,
            IntentFilter("com.hotspotIPbrowser.MUSIC_INFO_UPDATE")
        )

        // Initial load using connection check
        checkAndLoadUrl(getWifiIpAddress(0))

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

        // --- ADD CALL TO NEW LISTENER FUNCTION ---
        setupNetworkStateListener()
        // ----------------------------------------

        if (!isScreenOnReceiverRegistered) {
            registerReceiver(screenOnReceiver, IntentFilter(Intent.ACTION_SCREEN_ON))
            isScreenOnReceiverRegistered = true
        }
    }


    private suspend fun checkIpConnection(ip: String, port: Int): Boolean {
        return try {
            Socket().use { socket ->
                socket.connect(InetSocketAddress(ip, port), connectionTimeoutMs)
                true
            }
        } catch (e: Exception) {
            false
        }
    }


    private fun checkAndLoadUrl(ip: String?) {
        if (ip == null || ip == "127.0.0.1") {
            swipeRefreshLayout.isRefreshing = false
            showErrorPage("Could not determine local IP address or local loopback address returned.")
            return
        }

        // Show refreshing indicator while checking
        swipeRefreshLayout.isRefreshing = true

        GlobalScope.launch(Dispatchers.IO) {
            val isConnected = checkIpConnection(ip, targetPort)

            withContext(Dispatchers.Main) {
                swipeRefreshLayout.isRefreshing = false // Stop refreshing indicator

                if (isConnected) {
                    loadUrl(ip) // Success: load the URL in WebView
                } else {
                    // Failure: show a custom connection error
                    showErrorPage("Could not establish a connection to $ip:$targetPort. <br>Check the web server is running on the correct port.")
                    // Use a custom error code (-1000) to force a retry and IP rotation
                    handleLoadError(-1000)
                }
            }
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
             <svg width="420" height="70" viewBox="0 1013.3181 310 30.793" xmlns="http://www.w3.org/2000/svg">
  <g>
    <path id="path3451" fill="#91E8CE"
      d="m 57.417761,1013.3181 c 14.611789,0 20.224582,2.4459 20.224582,9.9487 0,6.9392 -6.62572,8.985 -20.224582,8.985 l -23.007045,0 0,11.8587 -18.674308,0 0,-19.1946 35.267174,0 c 3.597746,0 6.831085,-0.1075 6.794027,-2.5972 -0.02631,-1.8498 -2.465923,-2.3655 -6.794027,-2.3655 l -40.297842,0 3.122164,-6.6351 43.589857,0 z"/>
    <path id="path3455" fill="#91E8CE"
      d="m 102.67986,1044.1105 0,-12.1504 c 0,0 3.25341,-0.037 6.49911,-0.012 7.75908,0.056 9.92854,1.4252 13.20048,3.6039 4.67707,3.1098 7.3638,8.5589 7.3638,8.5589 l 19.52819,0 c 0,0 -2.79636,-5.4414 -7.21402,-9.2553 -2.08762,-1.8004 -4.10729,-2.9415 -6.45123,-3.6101 1.27233,-0.2069 9.39737,-0.9434 9.39737,-8.2779 0,-7.16 -5.8722,-9.4869 -17.78182,-9.4869 l -42.323698,0 -3.196281,6.4729 39.453219,0 c 3.38312,0 5.1928,0.9172 5.2978,2.3563 0.16059,2.1926 -1.99961,2.767 -5.16809,2.767 l -36.440691,0 0,19.034 17.835861,0 z"/>
    <path id="path3457" fill="#91E8CE"
      d="m 154.23742,1044.1111 17.43749,0 0,-30.6302 -17.43749,0 0,30.6302 z"/>
    <path id="path3461" fill="#91E8CE"
      d="m 180.15029,1013.4803 c 0,0 7.52439,-0.016 17.32014,0 0,4.3589 0.007,11.6425 0.007,16.5342 0,4.5705 2.11541,7.177 9.69075,7.177 1.86064,0 3.49121,0.012 5.23758,0.012 5.79963,0 9.24142,-2.1061 9.24142,-7.018 l 0,-16.7009 17.54558,0 c 0,0 0.0385,6.6612 0,12.3898 -0.0278,4.0225 0,6.9995 0,8.6887 0,5.6236 -5.93243,9.5518 -18.75461,9.5518 l -10.77316,0 -10.76699,0 c -14.34929,0 -18.74842,-4.112 -18.74842,-9.6908 0,-1.4932 0.009,-4.8917 0,-9.2645 -0.0124,-5.0786 0,-11.675 0,-11.675"/>
    <path id="path3465" fill="#91E8CE"
      d="m 247.77402,1037.0309 c 0,0 34.44263,0.012 38.79235,0.012 4.37288,0 5.14802,-1.2507 5.04456,-2.8304 -0.0632,-0.9666 -0.84771,-1.927 -5.04765,-1.927 l -39.35903,0 0,-7.3344 c 0,-8.542 8.00305,-11.3615 15.14141,-11.4711 10.00112,-0.1544 47.16136,0 47.16136,0 l -2.801,6.6349 -37.40266,0 c -3.16694,0 -4.58133,1.1474 -4.63229,2.4135 -0.0509,1.2384 1.1905,2.3872 4.54428,2.3872 l 25.88215,0 c 11.38463,0 15.82391,4.8191 15.61854,9.4205 -0.28874,6.4914 -5.22059,9.7741 -15.61854,9.7741 l -51.03394,0 3.71046,-7.0796 z"/>
  </g>
    </svg>

        </p>
    </div>
    <div class="error-message">
        <h1>Connection lost</h1>
        <p> $errorMessage </p>
        ${if (failingUrl != null) "<p>url: $failingUrl</p>" else ""}
    </div>
</body>
</html>
        """.trimIndent()
        myWebView.loadDataWithBaseURL(null, errorHtml, "text/html", "utf-8", null)
    }

    private fun loadUrl(ip: String?) {
        myWebView.loadUrl("http://$ip")
        // Store the successfully connected IP address
        getSharedPreferences(perfName, MODE_PRIVATE).edit {
            putString(prefLastIpAddress, ip)
        }
    }

    private fun handleLoadError(errorCode: Int) {
        // This is only called when an error occurs (either in WebView or from checkAndLoadUrl failure)
        handler.postDelayed({
            // errorCode != 0 will trigger IP rotation only if the connection check failed
            checkAndLoadUrl(getWifiIpAddress(errorCode))
        }, 3000L)
    }

    private var nextIpIndex = 0

    /**
     * Finds the local IP address and modifies the last octet.
     * @param errorCode If 0 (e.g., on initial load/resume), the internal IP index is NOT rotated.
     * If != 0 (e.g., after a connection failure), the IP index is rotated
     * to try the next available network interface in the next retry.
     * @return The modified target IP address.
     */
    // --- THIS IS THE MODIFIED FUNCTION THAT FILTERS FOR WI-FI ---
    private fun getWifiIpAddress(errorCode: Int): String {
        val privateIps = mutableListOf<String>()

        try {
            val interfaces = NetworkInterface.getNetworkInterfaces()
            for (interfc in Collections.list(interfaces)) {

                // Get the interface name and make it lowercase for reliable comparison
                val interfaceName = interfc.displayName.lowercase()

                // Filter for Wi-Fi interfaces only.
                // "wlan" is standard for Wi-Fi clients.
                // "ap" is common when the device is in Access Point (hotspot) mode.
                if (!interfaceName.startsWith("wlan") && !interfaceName.startsWith("ap")) {
                    continue // Not a Wi-Fi interface, skip it.
                }

                // Also, ensure the interface is "up" (active)
                if (!interfc.isUp) {
                    continue // Interface is down, skip it.
                }

                for (addr in Collections.list(interfc.inetAddresses)) {
                    val ip = addr.hostAddress ?: continue
                    if (!addr.isLoopbackAddress && ip.indexOf(':') == -1) { // IPv4 only
                        if (ip.startsWith("192.168.") ||
                            ip.startsWith("172.") ||
                            ip.startsWith("10.")
                        ) {
                            privateIps.add(ip)
                        }
                    }
                }
            }

            if (privateIps.isEmpty()) {
                // No Wi-Fi IPs were found
                return "127.0.0.1"
            }

            // Ensure index is valid, resetting to 0 if list changed size.
            if (nextIpIndex >= privateIps.size) {
                nextIpIndex = 0
            }

            val selectedIp = privateIps[nextIpIndex]

            // CRITICAL FIX: Only rotate the index if an error occurred (errorCode != 0).
            if (errorCode != 0) {
                nextIpIndex = (nextIpIndex + 1) % privateIps.size
            }

            return modifyLastIpSegment(
                ip = selectedIp,
                newLastSegment = newLastSegment
            )

        } catch (e: Exception) {
            e.printStackTrace()
        }

        return "127.0.0.1"
    }
    // -----------------------------------------------------------

    private fun modifyLastIpSegment(ip: String?, newLastSegment: Int): String {
        if (ip == null) return "127.0.0.1"
        val segments = ip.split(".")
        if (segments.size != 4) return ip
        return "${segments[0]}.${segments[1]}.${segments[2]}.$newLastSegment"
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        if (myWebView.canGoBack()) {
            myWebView.goBack()
        } else {
            super.onBackPressed()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        // Clean up general handler
        handler.removeCallbacksAndMessages(null)
        LocalBroadcastManager.getInstance(this).unregisterReceiver(musicInfoReceiver)
        if (isScreenOnReceiverRegistered) {
            unregisterReceiver(screenOnReceiver)
            isScreenOnReceiverRegistered = false
        }

        // --- ADD CLEANUP FOR NETWORK LISTENER ---
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            connectivityManager?.unregisterNetworkCallback(networkCallback)
        }
        // ----------------------------------------
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
                }
                // Store the user's input
                val sharedPrefs = getSharedPreferences(perfName, MODE_PRIVATE)
                sharedPrefs.edit {
                    putInt(prefLastSegment, newLastSegment)
                }
                checkAndLoadUrl(getWifiIpAddress(0))//reload using connection check
            } catch (_: NumberFormatException) {
                // Handle the exception if the input is not a valid number
                Toast.makeText(this, "Invalid input. Number must be between 1 and 255", Toast.LENGTH_LONG).show()
                askForLastSegment()
            }
        }

        builder.setNegativeButton("Cancel") { dialog, which ->
            dialog.cancel()
            checkAndLoadUrl(getWifiIpAddress(0))//load with default using connection check
        }

        builder.show()
    }

    private fun requestNotificationAccess() {
        val intent = Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)
        startActivity(intent)
    }


    override fun onResume() {
        super.onResume()
        val sharedPrefs = getSharedPreferences(perfName, MODE_PRIVATE)

        // Get the last successfully connected IP. This is the most stable candidate.
        val lastGoodIp = sharedPrefs.getString(prefLastIpAddress, null)

        // Determine the target IP: use the last successful one if available, otherwise discover the base IP (without rotation).
        val targetIp = if (!lastGoodIp.isNullOrEmpty()) {
            lastGoodIp
        } else {
            // Use 0 as errorCode to ensure getWifiIpAddress returns the *most stable* IP without rotating the index.
            getWifiIpAddress(0)
        }

        // --- IP Load Stability Logic ---
        if (targetIp != "127.0.0.1") {
            val expectedUrlPrefix = "http://$targetIp"

            // 1. Check if the WebView's current URL already matches the expected target IP URL.
            val isCurrentUrlCorrect = myWebView.url?.startsWith(expectedUrlPrefix) == true

            if (!isCurrentUrlCorrect) {
                // URL is incorrect (e.g., error page or blank).
                // Force a connection check and reload using the most stable IP candidate.
                checkAndLoadUrl(targetIp)
            }
        } else {
            // Handle case where IP cannot be determined
            showErrorPage("Could not determine local IP address.")
        }
        // --- End IP Load Stability Logic ---

        notificationPrompted = getSharedPreferences(perfName, MODE_PRIVATE).getBoolean(perfNotificationPrompt, false)

        if (!isNotificationServiceEnabled()) {
            if (!notificationPrompted) {
                requestNotificationAccess()
                getSharedPreferences(perfName, MODE_PRIVATE).edit { putBoolean(perfNotificationPrompt, true) }
            } else {
                Toast.makeText( this, "Please grant notification access for music info.", Toast.LENGTH_LONG).show()
                getSharedPreferences(perfName, MODE_PRIVATE).edit { putBoolean(perfNotificationPrompt, false) }
            }

        } else {
            startService(Intent(this, MusicNotificationListenerService::class.java))
            handler.postDelayed({
                launchLastMusicApp()
            }, 3000)
        }

    }



    private var mediaBrowser: MediaBrowserCompat? = null

    private fun launchLastMusicApp() {
        val prefs = getSharedPreferences(perfName, MODE_PRIVATE)
        val lastMusicApp = prefs.getString(prefLastMusicApp, null)

        if (lastMusicApp.isNullOrEmpty()) {
            sendMediaPlayCommand()
            return
        }

        if (musicAppPackage == null) {
            musicAppPackage = lastMusicApp
        }

        val serviceComponent = findMediaBrowserService(lastMusicApp)

        if (serviceComponent != null) {
            connectToMediaBrowser(serviceComponent)
        } else {
            sendMediaPlayCommand()
        }
    }

    private fun findMediaBrowserService(packageName: String): ComponentName? {
        val intent = Intent("android.media.browse.MediaBrowserService")
        val resolveInfos = packageManager.queryIntentServices(intent, 0)

        for (info in resolveInfos) {
            if (info.serviceInfo.packageName == packageName) {
                return ComponentName(packageName, info.serviceInfo.name)
            }
        }
        return null
    }

    private fun connectToMediaBrowser(serviceComponent: ComponentName) {
        mediaBrowser?.disconnect()
        mediaBrowser = MediaBrowserCompat(
            this,
            serviceComponent,
            object : MediaBrowserCompat.ConnectionCallback() {
                override fun onConnected() {
                    try {
                        val token = mediaBrowser?.sessionToken ?: return
                        val controller = MediaControllerCompat(this@MainActivity, token)

                        val state = controller.playbackState?.state
                        if (state != PlaybackStateCompat.STATE_PLAYING) {
                            controller.transportControls.play()
                        }

                        handler.postDelayed({
                            mediaBrowser?.disconnect()
                            mediaBrowser = null
                        }, 2000)
                    } catch (e: Exception) {
                        e.printStackTrace()
                    }
                }

                override fun onConnectionFailed() {
                    // Fallback na media key
                    sendMediaPlayCommand()
                    mediaBrowser = null
                }

                override fun onConnectionSuspended() {
                    mediaBrowser = null
                }
            },
            null
        )

        try {
            mediaBrowser?.connect()
        } catch (e: Exception) {
            e.printStackTrace()
            sendMediaPlayCommand()
        }
    }

    private val screenOnReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action == Intent.ACTION_SCREEN_ON) {
                launchLastMusicApp()
            }
        }
    }


    private fun isAppRunning(packageName: String): Boolean {
        val activityManager = getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager
        val processes = activityManager.runningAppProcesses ?: return false
        return processes.any { it.processName == packageName }
    }
    private fun sendMediaPlayCommand() {
        val audioManager = getSystemService(Context.AUDIO_SERVICE) as android.media.AudioManager
        val downEvent = KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PLAY)
        // Corrected the upEvent creation to use ACTION_UP and the correct key code
        val upEvent = KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_MEDIA_PLAY)

        audioManager.dispatchMediaKeyEvent(downEvent)
        audioManager.dispatchMediaKeyEvent(upEvent)
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
        val sharedPrefs = getSharedPreferences(perfName, MODE_PRIVATE)
        if (musicAppPackage == null) {
            musicAppPackage = sharedPrefs.getString(prefLastMusicApp, null)
        }

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

    private fun setupNetworkStateListener() {
        // We only use the modern NetworkCallback API available on M (API 23) and newer.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            connectivityManager =
                getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

            // We are specifically asking to listen for a Wi-Fi network
            val networkRequest = NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .build()

            networkCallback = object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) {
                    super.onAvailable(network)
                    // This is the magic! A Wi-Fi network just became available.

                    // We must run the connection check on the Main (UI) thread
                    // because checkAndLoadUrl updates the UI (swipeRefreshLayout.isRefreshing).
                    handler.post {
/*                        Toast.makeText(
                            applicationContext,
                            "Wi-Fi detected. Connecting...",
                            Toast.LENGTH_SHORT
                        ).show()*/

                        // Use 0 as the error code to get the base IP without rotation
                        checkAndLoadUrl(getWifiIpAddress(0))
                    }
                }

                override fun onLost(network: Network) {
                    super.onLost(network)
                    // The Wi-Fi network was lost.
                    handler.post {
 /*                       Toast.makeText(
                            applicationContext,
                            "Wi-Fi connection lost.",
                            Toast.LENGTH_SHORT
                        ).show()*/

                        // We can show the error page immediately.
                        showErrorPage("Wi-Fi connection was lost.")
                    }
                }
            }

            // Register the listener
            connectivityManager?.registerNetworkCallback(networkRequest, networkCallback)
        }
    }
}