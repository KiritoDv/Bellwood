#ifdef __ANDROID__

#include <jni.h>
#include <android/log.h>

#include <atomic>
#include <exception>
#include <filesystem>
#include <string>

#include "port/Engine.h"
#include "port/GameExtractor.h"
#include "port/ShipCompat.h"
#include "ship/Context.h"
#include "ship/window/Window.h"
#include "ship/window/gui/Gui.h"

namespace {

constexpr const char* kLogTag = PORT_NAME;

std::string ToStdString(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    std::string result = chars != nullptr ? chars : "";
    if (chars != nullptr) {
        env->ReleaseStringUTFChars(value, chars);
    }
    return result;
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL Java_dev_net64_bellwood_MainActivity_isMenuOpen(JNIEnv*, jobject) {
    auto window = ShipCompat::GetWindow();
    if (window == nullptr) {
        return JNI_FALSE;
    }
    auto gui = window->GetGui();
    if (gui == nullptr) {
        return JNI_FALSE;
    }
    return gui->GetMenuOrMenubarVisible() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_dev_net64_bellwood_GameAssets_nativeDetectDisc(JNIEnv* env, jobject, jstring jDiscPath) {
    const std::string discPath = ToStdString(env, jDiscPath);
    GameExtractor::sLastError.clear();
    const auto version = GameExtractor::DetectVersion(discPath);
    if (!version.has_value()) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "Unsupported disc at %s: %s", discPath.c_str(),
                            GameExtractor::sLastError.c_str());
        return nullptr;
    }
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Recognised %s as %s", discPath.c_str(), version->c_str());
    return env->NewStringUTF(version->c_str());
}

JNIEXPORT jstring JNICALL Java_dev_net64_bellwood_GameAssets_nativeLastError(JNIEnv* env, jobject) {
    return env->NewStringUTF(GameExtractor::sLastError.c_str());
}

JNIEXPORT jboolean JNICALL Java_dev_net64_bellwood_GameAssets_nativeIsArchiveCurrent(JNIEnv* env, jobject,
                                                                                    jstring jArchive) {
    return GameEngine_IsGameArchiveCurrent(ToStdString(env, jArchive)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_dev_net64_bellwood_GameAssets_nativeGenerateGameArchive(JNIEnv* env, jobject,
                                                                                      jstring jDiscPath,
                                                                                      jstring jSourceDir,
                                                                                      jstring jDestDir) {
    const std::string discPath = ToStdString(env, jDiscPath);
    const std::string sourceDir = ToStdString(env, jSourceDir);
    const std::string destDir = ToStdString(env, jDestDir);

    const auto fail = [env](const std::string& message) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Extraction failed: %s", message.c_str());
        return env->NewStringUTF(message.c_str());
    };

    GameExtractor extractor;
    if (!extractor.RunStandalone(discPath)) {
        return fail(GameExtractor::sLastError.empty() ? "That is not a supported Animal Crossing disc."
                                                      : GameExtractor::sLastError);
    }

    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Extracting %s into %s", discPath.c_str(), destDir.c_str());

    std::string extractError;
    std::atomic<size_t> assetCount{ 0 };
    try {
        if (!extractor.GenerateOTRTo(assetCount, sourceDir, destDir)) {
            extractError = GameExtractor::sLastError.empty() ? "Torch could not extract the disc."
                                                             : "Torch could not extract the disc: " +
                                                                   GameExtractor::sLastError;
        }
    } catch (const std::exception& error) {
        extractError = std::string("Torch could not extract the disc: ") + error.what();
    } catch (...) { extractError = "Torch could not extract the disc."; }

    if (!extractError.empty()) {
        return fail(extractError);
    }

    std::error_code error;
    const std::filesystem::path archive = std::filesystem::path(destDir) / GAME_ARCHIVE;
    if (!std::filesystem::exists(archive, error) || std::filesystem::file_size(archive, error) == 0) {
        return fail(std::string("Torch finished without producing ") + GAME_ARCHIVE + ".");
    }

    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Wrote %s", archive.c_str());
    return nullptr;
}

}

#endif
