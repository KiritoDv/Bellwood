package dev.net64.bellwood

import android.app.AlertDialog
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import kotlin.concurrent.thread

class SavesActivity : ComponentActivity() {

    private lateinit var listContainer: LinearLayout
    private lateinit var emptyLabel: TextView
    private var dirty = false

    private val importGameCube =
        registerForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
            if (uris.isNullOrEmpty()) return@registerForActivityResult
            runInBackground("Copying…") { uris.mapNotNull { SaveStore.importGameCubeSave(this, it) } }
        }

    private val restoreBackup =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            if (uri == null) return@registerForActivityResult
            AlertDialog.Builder(this)
                .setTitle("Restore this backup?")
                .setMessage("Your current save is moved to save/backup first.")
                .setPositiveButton("Restore") { _, _ ->
                    runInBackground("Restoring…") { listOfNotNull(SaveStore.restore(this, uri)) }
                }
                .setNegativeButton("Cancel", null)
                .show()
        }

    private val exportBackup =
        registerForActivityResult(ActivityResultContracts.CreateDocument("application/zip")) { uri ->
            if (uri == null) return@registerForActivityResult
            runInBackground("Backing up…") { listOfNotNull(SaveStore.exportAll(this, uri)) }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buildUi()
        refresh()
    }

    private fun buildUi() {
        val pad = dp(16)
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, pad, pad, pad)
            setBackgroundColor(BACKGROUND)
        }

        root.addView(TextView(this).apply {
            text = getString(R.string.saves_title)
            setTextColor(Color.WHITE)
            textSize = 22f
            setTypeface(typeface, Typeface.BOLD)
        })

        root.addView(TextView(this).apply {
            text = "Back up or restore your town. GameCube saves (.gci, .gcs, .raw) are copied to the import " +
                "folder; finish the import in the game under Settings > General > Import GameCube Save."
            setTextColor(SUBTLE)
            textSize = 12f
            setPadding(0, dp(4), 0, dp(8))
        })

        emptyLabel = TextView(this).apply {
            text = "Nothing saved yet."
            setTextColor(SUBTLE)
            gravity = Gravity.CENTER
            setPadding(0, dp(24), 0, dp(24))
        }
        root.addView(emptyLabel)

        listContainer = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        root.addView(
            ScrollView(this).apply { addView(listContainer) },
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
        )

        val actions = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, dp(8), 0, 0)
        }
        actions.addView(button("Import GameCube save") { importGameCube.launch(arrayOf("*/*")) })
        actions.addView(button("Back up") { exportBackup.launch(SaveStore.BUNDLE_NAME) })
        actions.addView(button("Restore") { restoreBackup.launch(arrayOf("application/zip", "*/*")) })
        actions.addView(View(this), LinearLayout.LayoutParams(0, 1, 1f))
        actions.addView(button("Restart game") { restartGame() })
        actions.addView(button("Done") { finish() })
        root.addView(actions)

        setContentView(root)
        root.padForSystemBars()
    }

    private fun refresh() {
        val entries = SaveStore.list(this)
        val pending = SaveStore.pendingImports(this)
        listContainer.removeAllViews()
        emptyLabel.visibility = if (entries.isEmpty() && pending.isEmpty()) View.VISIBLE else View.GONE

        entries.forEachIndexed { index, entry ->
            listContainer.addView(row(index, entry.file.name, "${entry.size} bytes", null))
        }
        pending.forEachIndexed { index, file ->
            listContainer.addView(row(entries.size + index, file.name, "waiting to be imported") {
                apply(SaveStore.deletePendingImport(file))
            })
        }
    }

    private fun row(index: Int, title: String, detail: String, onRemove: (() -> Unit)?): View {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(8), dp(6), dp(8), dp(6))
            setBackgroundColor(if (index % 2 == 0) ROW_EVEN else Color.TRANSPARENT)
        }

        val label = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        label.addView(TextView(this).apply {
            text = title
            setTextColor(Color.WHITE)
            textSize = 15f
        })
        label.addView(TextView(this).apply {
            text = detail
            setTextColor(SUBTLE)
            textSize = 11f
        })
        row.addView(label, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))

        if (onRemove != null) {
            row.addView(button("Remove") { onRemove() })
        }
        return row
    }

    private fun apply(error: String?) {
        if (error != null) {
            Toast.makeText(this, error, Toast.LENGTH_LONG).show()
        }
        refresh()
    }

    private fun runInBackground(message: String, work: () -> List<String>) {
        val progress = AlertDialog.Builder(this)
            .setMessage(message)
            .setCancelable(false)
            .show()

        thread(name = "saves-io") {
            val errors = runCatching(work).getOrElse { listOf(it.message ?: it.toString()) }

            runOnUiThread {
                progress.dismiss()
                if (errors.isEmpty()) {
                    dirty = true
                } else {
                    Toast.makeText(this, errors.joinToString("\n\n"), Toast.LENGTH_LONG).show()
                }
                refresh()
            }
        }
    }

    override fun finish() {
        if (dirty) {
            Toast.makeText(this, "Restart the game so it picks up the changes.", Toast.LENGTH_LONG).show()
        }
        super.finish()
    }

    private fun restartGame() {
        val intent = Intent(this, LauncherActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        startActivity(intent)
        finishAffinity()
        Runtime.getRuntime().exit(0)
    }

    private fun button(label: String, onClick: () -> Unit) = Button(this).apply {
        text = label
        textSize = 12f
        isAllCaps = false
        minWidth = 0
        minimumWidth = 0
        setPadding(dp(10), dp(4), dp(10), dp(4))
        setOnClickListener { onClick() }
    }

    private fun dp(value: Int) = (value * resources.displayMetrics.density).toInt()

    private companion object {
        val BACKGROUND = Color.rgb(16, 28, 20)
        val SUBTLE = Color.rgb(150, 165, 155)
        val ROW_EVEN = Color.argb(24, 255, 255, 255)
    }
}
