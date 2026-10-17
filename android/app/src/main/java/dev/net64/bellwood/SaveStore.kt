package dev.net64.bellwood

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream

object SaveStore {

    private const val TAG = "SaveStore"
    private const val COPY_BUFFER = 1 shl 16
    private const val BACKUP_DIR = "backup"
    private val GAMECUBE_SAVE_EXTENSIONS = listOf(".gci", ".gcs", ".raw", ".gcp")

    const val BUNDLE_NAME = "bellwood-save.zip"

    data class Entry(val file: File) {
        val size: Long
            get() = if (file.isDirectory) file.walkTopDown().filter(File::isFile).sumOf(File::length) else file.length()
    }

    fun list(context: Context): List<Entry> =
        (GameAssets.saveDir(context).listFiles() ?: emptyArray())
            .filter { it.name != BACKUP_DIR }
            .sortedBy { it.name }
            .map { Entry(it) }

    fun pendingImports(context: Context): List<File> =
        (GameAssets.importDir(context).listFiles() ?: emptyArray())
            .filter { file -> file.isFile && GAMECUBE_SAVE_EXTENSIONS.any { file.name.endsWith(it, true) } }
            .sortedBy { it.name }

    fun exportAll(context: Context, destination: Uri): String? = runOrReport("back up the save") {
        val root = GameAssets.saveDir(context)
        val files = root.walkTopDown()
            .filter { it.isFile && !it.relativeTo(root).invariantSeparatorsPath.startsWith("$BACKUP_DIR/") }
            .toList()
        check(files.isNotEmpty()) { "There is no save to back up yet." }

        context.contentResolver.openOutputStream(destination).use { output ->
            requireNotNull(output) { "The document provider did not open the destination." }
            ZipOutputStream(output.buffered()).use { zip ->
                for (file in files) {
                    zip.putNextEntry(ZipEntry(file.relativeTo(root).invariantSeparatorsPath))
                    file.inputStream().use { input -> input.copyTo(zip, COPY_BUFFER) }
                    zip.closeEntry()
                }
            }
        }
    }

    fun restore(context: Context, uri: Uri): String? = runOrReport("restore the save") {
        val root = GameAssets.saveDir(context)
        val staging = File(context.cacheDir, "save-restore").apply { deleteRecursively(); mkdirs() }
        var restored = 0

        try {
            context.contentResolver.openInputStream(uri).use { input ->
                requireNotNull(input) { "The document provider did not open the archive." }
                ZipInputStream(input.buffered()).use { zip ->
                    while (true) {
                        val entry = zip.nextEntry ?: break
                        val destination = File(staging, entry.name).canonicalFile
                        if (entry.isDirectory ||
                            !destination.path.startsWith(staging.canonicalPath + File.separator)) {
                            zip.closeEntry()
                            continue
                        }
                        destination.parentFile?.mkdirs()
                        FileOutputStream(destination).use { output -> zip.copyTo(output, COPY_BUFFER) }
                        zip.closeEntry()
                        restored++
                    }
                }
            }
            check(File(staging, "town.json").isFile) { "That archive is not a Bellwood save backup." }

            val keep = File(root, "$BACKUP_DIR/before-restore-" + stamp())
            (root.listFiles() ?: emptyArray()).filter { it.name != BACKUP_DIR }.forEach { current ->
                keep.mkdirs()
                check(current.renameTo(File(keep, current.name))) { "Could not move ${current.name} aside." }
            }
            staging.copyRecursively(root, overwrite = true)
        } finally {
            staging.deleteRecursively()
        }
        check(restored > 0) { "That archive has no save files in it." }
    }

    fun importGameCubeSave(context: Context, uri: Uri): String? {
        val name = queryDisplayName(context, uri) ?: return "Could not read the file's name."
        if (GAMECUBE_SAVE_EXTENSIONS.none { name.endsWith(it, true) }) {
            return "$name is not a GameCube save. Use a .gci, .gcs or .raw file."
        }
        return runOrReport("copy $name") {
            val target = File(GameAssets.importDir(context).apply { mkdirs() }, name)
            context.contentResolver.openInputStream(uri).use { input ->
                requireNotNull(input) { "The document provider did not open $name." }
                FileOutputStream(target).use { output -> input.copyTo(output, COPY_BUFFER) }
            }
        }
    }

    fun deletePendingImport(file: File): String? =
        if (file.delete()) null else "Could not delete ${file.name}."

    private fun stamp(): String = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())

    private fun runOrReport(action: String, work: () -> Unit): String? = try {
        work()
        null
    } catch (error: Exception) {
        Log.e(TAG, "Could not $action", error)
        "Could not $action: ${error.message ?: error}"
    }

    private fun queryDisplayName(context: Context, uri: Uri): String? =
        context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
            ?.use { cursor -> if (cursor.moveToFirst()) cursor.getString(0) else null }
}
