package dev.net64.bellwood

import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.ScrollView
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import java.io.File
import java.io.FileOutputStream
import kotlin.concurrent.thread

class LauncherActivity : ComponentActivity() {

    private lateinit var status: TextView
    private lateinit var chooseButton: Button
    private lateinit var progress: ProgressBar

    private val pickDisc =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
            if (uri != null) extractDisc(uri)
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buildUi()
        prepare()
    }

    private fun prepare() {
        setBusy(true, getString(R.string.launcher_preparing))

        thread(name = "bellwood-prepare") {
            val stagingError = GameAssets.stageBundledAssets(this)
            val ready = stagingError == null && GameAssets.isExtracted(this)

            runOnUiThread {
                when {
                    stagingError != null -> {
                        setBusy(false, stagingError)
                        chooseButton.isEnabled = false
                    }
                    ready -> startGame()
                    else -> setBusy(false, discInstructions)
                }
            }
        }
    }

    private fun buildUi() {
        val padding = (24 * resources.displayMetrics.density).toInt()
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(padding, padding, padding, padding)
            setBackgroundColor(Color.rgb(20, 32, 24))
        }

        status = TextView(this).apply {
            setTextColor(Color.WHITE)
            textSize = 16f
            gravity = Gravity.CENTER
        }
        chooseButton = Button(this).apply {
            text = getString(R.string.launcher_choose_disc)
            setOnClickListener { pickDisc.launch(arrayOf("*/*")) }
        }
        progress = ProgressBar(this).apply { visibility = View.GONE }

        val scroller = ScrollView(this).apply {
            addView(
                status,
                ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            )
        }

        root.addView(scroller, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        root.addView(chooseButton)
        root.addView(progress)
        setContentView(root)
        root.padForSystemBars()
    }

    private fun extractDisc(uri: Uri) {
        setBusy(true, getString(R.string.launcher_verifying))

        thread(name = "bellwood-extract") {
            val error = runCatching { extractFrom(uri) }.getOrElse { it.message ?: it.toString() }

            runOnUiThread {
                if (error == null) {
                    startGame()
                } else {
                    setBusy(false, "$error\n\n$discInstructions")
                }
            }
        }
    }

    private fun extractFrom(uri: Uri): String? {
        val descriptor = contentResolver.openFileDescriptor(uri, "r")
            ?: return "The document provider did not open the file."

        descriptor.use {
            val directPath = "/proc/self/fd/${it.fd}"
            if (GameAssets.identifyDisc(directPath) != null) {
                runOnUiThread { setBusy(true, getString(R.string.launcher_extracting)) }
                return GameAssets.generateGameArchive(this, directPath)
            }
        }

        val copy = File(cacheDir, "disc" + extensionOf(uri))
        try {
            copy.delete()
            contentResolver.openInputStream(uri).use { input ->
                requireNotNull(input) { "The document provider did not open the file." }
                FileOutputStream(copy).use { output -> input.copyTo(output, COPY_BUFFER_BYTES) }
            }
            if (GameAssets.identifyDisc(copy.absolutePath) == null) {
                return GameAssets.lastError().ifEmpty { "That is not a supported Animal Crossing disc." }
            }
            runOnUiThread { setBusy(true, getString(R.string.launcher_extracting)) }
            return GameAssets.generateGameArchive(this, copy.absolutePath)
        } finally {
            copy.delete()
        }
    }

    private fun extensionOf(uri: Uri): String {
        val name = contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
            ?.use { cursor -> if (cursor.moveToFirst()) cursor.getString(0) else null }
            ?: return ".iso"
        val dot = name.lastIndexOf('.')
        return if (dot >= 0) name.substring(dot).lowercase() else ".iso"
    }

    private fun setBusy(busy: Boolean, message: String) {
        status.text = message
        chooseButton.isEnabled = !busy
        progress.visibility = if (busy) View.VISIBLE else View.GONE
        if (busy) {
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        } else {
            window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }
    }

    private fun startGame() {
        startActivity(Intent(this, MainActivity::class.java))
        finish()
        Runtime.getRuntime().exit(0)
    }

    private val discInstructions: String
        get() = "Select your own Animal Crossing (USA, GAFE01) disc image: .iso, .gcm, .ciso or .gcz. " +
            "It is read on this device and converted into the game's asset archive. " +
            "No game data ships with the app.\n\n" +
            "Mods go in ${GameAssets.modsDir(this)}."

    private companion object {
        const val COPY_BUFFER_BYTES = 1 shl 17
    }
}
