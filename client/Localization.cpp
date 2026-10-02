#include "Localization.hpp"

#include <cstring>
#include <string>
#include <unordered_map>
#include <SDL3/SDL.h>

namespace
{
    struct Entry
    {
        const char* english;
        const char* text;
    };
    // One table per language. Keep keys byte-identical to the source literals.
    const Entry kZhTW[] = {
#include "LocalizationZhTW.inc"
    };
    const Entry kJa[] = {
#include "LocalizationJa.inc"
    };

    ClientLanguage gRequested = ClientLanguage::Auto;
    ClientLanguage gEffective = ClientLanguage::English;
    std::unordered_map<std::string, const char*> gTable;
    ClientLanguage gTableLanguage = ClientLanguage::English; // Which language gTable holds

    void BuildTable(ClientLanguage language)
    {
        if (language == gTableLanguage && !gTable.empty())
            return;
        gTable.clear();
        gTableLanguage = language;
        const Entry* entries = nullptr;
        size_t count = 0;
        switch (language)
        {
        case ClientLanguage::ChineseTraditional: entries = kZhTW, count = sizeof(kZhTW) / sizeof(kZhTW[0]); break;
        case ClientLanguage::Japanese: entries = kJa, count = sizeof(kJa) / sizeof(kJa[0]); break;
        default: return;
        }
        gTable.reserve(count);
        for (size_t i = 0; i < count; ++i)
            gTable.emplace(entries[i].english, entries[i].text);
    }

    ClientLanguage DetectSystemLanguage()
    {
        int count = 0;
        SDL_Locale** locales = SDL_GetPreferredLocales(&count);
        ClientLanguage detected = ClientLanguage::English;
        if (locales)
        {
            for (int i = 0; i < count; ++i)
            {
                const SDL_Locale* locale = locales[i];
                if (!locale || !locale->language)
                    continue;
                if (SDL_strcasecmp(locale->language, "zh") == 0)
                {
                    detected = ClientLanguage::ChineseTraditional;
                    break;
                }
                if (SDL_strcasecmp(locale->language, "ja") == 0)
                {
                    detected = ClientLanguage::Japanese;
                    break;
                }
            }
            SDL_free(locales);
        }
        return detected;
    }

    // Ring of buffers for icon+text labels. Sized well above the number of labels a frame draws.
    constexpr int kRingSize = 256;
    std::string gRing[kRingSize];
    int gRingIndex = 0;
}

void clientLocalizationSetLanguage(ClientLanguage language)
{
    gRequested = language;
    gEffective = language == ClientLanguage::Auto ? DetectSystemLanguage() : language;
    BuildTable(gEffective);
}

ClientLanguage clientLocalizationEffectiveLanguage()
{
    return gEffective;
}

const char* clientLanguageName(ClientLanguage language)
{
    switch (language)
    {
    case ClientLanguage::English: return "English";
    case ClientLanguage::ChineseTraditional: return "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87"; // 繁體中文
    case ClientLanguage::Japanese: return "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"; // 日本語
    default: return tr("System default");
    }
}

const char* tr(const char* english)
{
    if (!english || gEffective == ClientLanguage::English)
        return english;
    BuildTable(gEffective);
    const auto it = gTable.find(english);
    return it == gTable.end() ? english : it->second;
}

const char* tri(const char* icon, const char* english)
{
    std::string& slot = gRing[gRingIndex];
    gRingIndex = (gRingIndex + 1) % kRingSize;
    slot.assign(icon ? icon : "");
    slot += ' ';
    slot += tr(english);
    return slot.c_str();
}
