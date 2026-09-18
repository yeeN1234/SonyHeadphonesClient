#pragma once
#include <string>

/**
 * Minimal UI localization. English source strings are the keys; a missing translation
 * falls back to the English text, so untranslated strings never break the UI.
 */
enum class ClientLanguage
{
    Auto = 0,        // Follow the system locale
    English = 1,
    ChineseTraditional = 2,
};

/** Apply a language choice (Auto resolves through the platform locale). */
void clientLocalizationSetLanguage(ClientLanguage language);
/** The language actually in effect after Auto resolution. */
ClientLanguage clientLocalizationEffectiveLanguage();
/** Display name for a language choice, in that language. */
const char* clientLanguageName(ClientLanguage language);

/** Translate an English UI string. The returned pointer is valid for the process lifetime. */
const char* tr(const char* english);
/**
 * Icon glyph + translated text, e.g. tri(PSI_LINK, "Connect") -> "<icon> 連線".
 * Returns a pointer into a ring of temporary buffers; valid until the ring wraps, which
 * is well beyond a single frame of UI. Use only as an immediate ImGui label argument.
 */
const char* tri(const char* icon, const char* english);
