package com.hotspotIPbrowser

import android.service.notification.NotificationListenerService
import android.app.Notification
import android.content.Intent
import android.service.notification.StatusBarNotification
import android.text.SpannableString
import androidx.localbroadcastmanager.content.LocalBroadcastManager

class MusicNotificationListenerService : NotificationListenerService() {
    /*    private val musicAppPackageNames = listOf(
            "com.google.android.apps.youtube.music",
            "com.google.android.music",
            "com.maxmpz.audioplayer", // PowerAmp
            "com.amazon.music",
            "com.soundcloud.android"
        )*/

    override fun onNotificationPosted(sbn: StatusBarNotification) {

        //if (sbn.packageName in musicAppPackageNames) {
        if ((sbn.packageName.contains("music") ||
                    sbn.packageName.contains("sound") ||
                    sbn.packageName.contains("audio"))
        ) {

            val extras = sbn.notification.extras

            val title: String? = extras?.run {
                getString(Notification.EXTRA_TITLE) ?:
                (getCharSequence(Notification.EXTRA_TITLE) as? SpannableString)?.toString() ?:
                getString("android.title") ?:
                (getCharSequence("android.title") as? SpannableString)?.toString()
            }

            val artist: String? = extras?.run {
                getString(Notification.EXTRA_TEXT) ?:
                (getCharSequence(Notification.EXTRA_TEXT) as? SpannableString)?.toString() ?:
                getString("android.text") ?:
                (getCharSequence("android.text") as? SpannableString)?.toString()
            }

            var songInfo = buildString {
                if (!title.isNullOrEmpty()) append(title)
                if (!artist.isNullOrEmpty()) append(" - $artist")
            }.toString() // Convert StringBuilder to String

            // Cut songInfo to 100 characters if it's longer
            if (songInfo.length > 100) {
                songInfo = songInfo.substring(0, 100) + "..."
            }
            if (songInfo.isNotEmpty()) {
                updateOverlay(songInfo)
            }
        }
    }

    private fun updateOverlay(songInfo: String) {
        val intent = Intent("com.hotspotIPbrowser.MUSIC_INFO_UPDATE").apply {
            putExtra("songInfo", songInfo)
        }
        LocalBroadcastManager.getInstance(this).sendBroadcast(intent)
    }
}


