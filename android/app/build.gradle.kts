import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import java.security.MessageDigest
import java.util.Properties
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val repositoryRoot: File = rootProject.projectDir.parentFile
val cmakeLists: String = File(repositoryRoot, "CMakeLists.txt").readText()

val projectVersion: String =
    Regex("""project\s*\([^)]*VERSION\s+([0-9.]+)""").find(cmakeLists)?.groupValues?.get(1) ?: "0.0.0"

val portName: String =
    Regex("""set\(PORT_NAME\s+"([^"]+)"\)""").find(cmakeLists)?.groupValues?.get(1) ?: "Bellwood"

val keystoreProperties = Properties().apply {
    val file = rootProject.file("key.properties")
    if (file.isFile) file.inputStream().use { load(it) }
}

val hasReleaseKeystore =
    keystoreProperties.getProperty("storeFile") != null || System.getenv("KEYSTORE_FILE") != null

abstract class PackGameData : DefaultTask() {

    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val recipes: DirectoryProperty

    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val portData: DirectoryProperty

    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val lusShaders: DirectoryProperty

    @get:InputFile
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val config: RegularFileProperty

    @get:Input
    abstract val engineArchive: Property<String>

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun pack() {
        val recipesRoot = recipes.get().asFile
        val target = outputDir.get().asFile
        target.mkdirs()

        val engine = File(target, engineArchive.get())
        writeZip(
            engine,
            filesUnder(portData.get().asFile, "") + filesUnder(lusShaders.get().asFile, "shaders")
        )

        val bundle = File(target, "gamedata.zip")
        writeZip(
            bundle,
            listOf("config.yml" to config.get().asFile, engine.name to engine) +
                filesUnder(recipesRoot, "assets/ymls")
        )
        engine.delete()

        val digest = MessageDigest.getInstance("MD5").digest(bundle.readBytes())
        File(target, "gamedata.version").writeText(digest.joinToString("") { "%02x".format(it) })
    }

    private fun filesUnder(root: File, prefix: String): List<Pair<String, File>> =
        root.walkTopDown().filter(File::isFile).map { file ->
            val relative = file.relativeTo(root).invariantSeparatorsPath
            (if (prefix.isEmpty()) relative else "$prefix/$relative") to file
        }.toList()

    private fun writeZip(destination: File, entries: List<Pair<String, File>>) {
        ZipOutputStream(destination.outputStream().buffered()).use { zip ->
            entries.sortedBy { it.first }.forEach { (name, file) ->
                zip.putNextEntry(ZipEntry(name).apply { time = FIXED_ENTRY_TIME })
                file.inputStream().use { it.copyTo(zip) }
                zip.closeEntry()
            }
        }
    }

    private companion object {
        const val FIXED_ENTRY_TIME = 318211200000L
    }
}

androidComponents {
    onVariants { variant ->
        val packGameData =
            tasks.register<PackGameData>("pack${variant.name.replaceFirstChar(Char::titlecase)}GameData") {
                recipes.set(File(repositoryRoot, "assets/ymls"))
                portData.set(File(repositoryRoot, "port"))
                lusShaders.set(File(repositoryRoot, "libultraship/src/fast/shaders"))
                config.set(File(repositoryRoot, "config.yml"))
                engineArchive.set("$portName.o2r")
            }
        variant.sources.assets?.addGeneratedSourceDirectory(packGameData, PackGameData::outputDir)
    }
}

android {
    namespace = "dev.net64.bellwood"
    compileSdk = 36
    ndkVersion = "30.0.15729638"

    defaultConfig {
        applicationId = "dev.net64.bellwood"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = projectVersion

        buildConfigField("String", "PORT_NAME", "\"$portName\"")

        ndk {
            abiFilters += listOf("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                    "-DUSE_OPENGLES=ON",
                    "-DSDL_SHARED=ON",
                    "-DSDL_STATIC=OFF",
                    "-DHAVE_LD_VERSION_SCRIPT=OFF",
                    "-DCMAKE_BUILD_TYPE=Release"
                )
                targets += portName
            }
        }
    }

    buildFeatures {
        buildConfig = true
    }

    signingConfigs {
        if (hasReleaseKeystore) {
            create("release") {
                val configuredStore = keystoreProperties.getProperty("storeFile")
                if (configuredStore != null) {
                    storeFile = rootProject.file(configuredStore)
                    storePassword = keystoreProperties.getProperty("storePassword")
                    keyAlias = keystoreProperties.getProperty("keyAlias")
                    keyPassword = keystoreProperties.getProperty("keyPassword")
                } else {
                    storeFile = file(System.getenv("KEYSTORE_FILE"))
                    storePassword = System.getenv("KEYSTORE_PASSWORD")
                    keyAlias = System.getenv("KEY_ALIAS")
                    keyPassword = System.getenv("KEY_PASSWORD")
                }
            }
        }
    }

    buildTypes {
        debug {
            applicationIdSuffix = ".debug"
        }
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName(if (hasReleaseKeystore) "release" else "debug")
            ndk {
                debugSymbolLevel = "SYMBOL_TABLE"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = File(repositoryRoot, "CMakeLists.txt")
            version = "3.30.3+"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    lint {
        abortOnError = false
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-ktx:1.9.3")
}
