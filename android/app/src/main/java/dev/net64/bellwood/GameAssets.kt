package dev.net64.bellwood

import android.content.Context
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.zip.ZipInputStream

object GameAssets {

    private const val TAG = "GameAssets"
    private const val GAME_ARCHIVE = "ac.o2r"
    private const val CONTROLLER_DB = "gamecontrollerdb.txt"
    private const val TORCH_HASHES = "torch.hash.yml"
    private const val GAMEDATA_ZIP = "gamedata.zip"
    private const val GAMEDATA_VERSION = "gamedata.version"
    private const val STAMP = ".gamedata.version"
    private const val COPY_BUFFER_BYTES = 1 shl 17

    fun gameDir(context: Context): File =
        (context.getExternalFilesDir(null) ?: context.filesDir).also { it.mkdirs() }

    fun gameArchive(context: Context) = File(gameDir(context), GAME_ARCHIVE)

    fun modsDir(context: Context) = File(gameDir(context), "mods")

    fun saveDir(context: Context) = File(gameDir(context), "save")

    fun importDir(context: Context) = File(gameDir(context), "import")

    fun isExtracted(context: Context): Boolean {
        val archive = gameArchive(context)
        return archive.length() > 0 && nativeIsArchiveCurrent(archive.absolutePath)
    }

    fun stageBundledAssets(context: Context): String? {
        val target = gameDir(context)
        val stamp = File(target, STAMP)

        return try {
            val expected = context.assets.open(GAMEDATA_VERSION).use { it.readBytes().decodeToString() }
            if (stamp.takeIf { it.isFile }?.readText() == expected) {
                return null
            }

            context.assets.open(GAMEDATA_ZIP).use { input ->
                ZipInputStream(input.buffered()).use { zip ->
                    while (true) {
                        val entry = zip.nextEntry ?: break
                        val destination = File(target, entry.name).canonicalFile
                        if (!destination.path.startsWith(target.canonicalPath + File.separator)) {
                            Log.w(TAG, "Skipping zip entry ${entry.name}")
                            zip.closeEntry()
                            continue
                        }
                        if (entry.isDirectory) {
                            destination.mkdirs()
                        } else {
                            destination.parentFile?.mkdirs()
                            FileOutputStream(destination).use { output -> zip.copyTo(output, COPY_BUFFER_BYTES) }
                        }
                        zip.closeEntry()
                    }
                }
            }

            val controllerDb = File(target, CONTROLLER_DB)
            if (!controllerDb.isFile) {
                copyAsset(context, CONTROLLER_DB, controllerDb)
            }

            val mods = modsDir(context)
            mods.mkdirs()
            if (mods.list().isNullOrEmpty()) {
                try {
                    copyAsset(context, "mods/place_mods_here.txt", File(mods, "place_mods_here.txt"))
                } catch (error: IOException) {
                    Log.w(TAG, "Could not write the mods placeholder", error)
                }
            }

            File(target, TORCH_HASHES).delete()
            stamp.writeText(expected)
            null
        } catch (error: IOException) {
            Log.e(TAG, "Could not unpack the bundled assets", error)
            "Could not unpack the bundled game files: ${error.message}"
        }
    }

    fun identifyDisc(path: String): String? = nativeDetectDisc(path)

    fun lastError(): String = nativeLastError()

    fun generateGameArchive(context: Context, discPath: String): String? {
        val dir = gameDir(context).absolutePath
        return nativeGenerateGameArchive(discPath, dir, dir)
    }

    private fun copyAsset(context: Context, assetPath: String, target: File) {
        target.parentFile?.mkdirs()
        context.assets.open(assetPath).use { input ->
            FileOutputStream(target).use { output ->
                input.copyTo(output)
                output.fd.sync()
            }
        }
    }

    private external fun nativeDetectDisc(disc: String): String?

    private external fun nativeLastError(): String

    private external fun nativeIsArchiveCurrent(archive: String): Boolean

    private external fun nativeGenerateGameArchive(disc: String, sourceDir: String, destDir: String): String?

    init {
        System.loadLibrary("SDL2")
        System.loadLibrary("main")
    }
}
