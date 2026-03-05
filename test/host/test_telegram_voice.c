/*
 * Host tests for Telegram voice ASR helpers.
 */

#include <stdio.h>
#include <string.h>

#include "telegram_voice.h"

#define TEST(name) static int test_##name(void)
#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)

TEST(multipart_body_construction)
{
    char preamble[512];
    char epilogue[64];

    int plen = telegram_voice_build_multipart_preamble(
        preamble, sizeof(preamble),
        "whisper-1", "zh",
        "以下是繁體中文語音轉錄。",
        "----zclaw_asr_boundary");
    ASSERT(plen > 0);

    int elen = telegram_voice_build_multipart_epilogue(
        epilogue, sizeof(epilogue),
        "----zclaw_asr_boundary");
    ASSERT(elen > 0);

    /* Preamble must contain model field */
    ASSERT(strstr(preamble, "name=\"model\"") != NULL);
    ASSERT(strstr(preamble, "whisper-1") != NULL);

    /* Preamble must contain language field */
    ASSERT(strstr(preamble, "name=\"language\"") != NULL);
    ASSERT(strstr(preamble, "zh") != NULL);

    /* Preamble must contain file part header */
    ASSERT(strstr(preamble, "name=\"file\"") != NULL);
    ASSERT(strstr(preamble, "filename=\"voice.ogg\"") != NULL);
    ASSERT(strstr(preamble, "audio/ogg") != NULL);

    /* Epilogue must contain closing boundary */
    ASSERT(strstr(epilogue, "----zclaw_asr_boundary--") != NULL);

    /* Lengths must match actual string lengths */
    ASSERT((size_t)plen == strlen(preamble));
    ASSERT((size_t)elen == strlen(epilogue));

    return 0;
}

TEST(getfile_response_parsing)
{
    const char *json = "{\"ok\":true,\"result\":{\"file_id\":\"abc\","
                       "\"file_unique_id\":\"xyz\","
                       "\"file_size\":12345,"
                       "\"file_path\":\"voice/file_42.oga\"}}";
    char path[128];
    size_t file_size = 0;
    ASSERT(telegram_voice_parse_file_path(json, path, sizeof(path), &file_size));
    ASSERT(strcmp(path, "voice/file_42.oga") == 0);
    ASSERT(file_size == 12345);
    return 0;
}

TEST(asr_response_parsing)
{
    const char *json = "{\"text\":\"你好世界\"}";
    char text[128];
    ASSERT(telegram_voice_parse_transcription(json, text, sizeof(text)));
    ASSERT(strcmp(text, "你好世界") == 0);
    return 0;
}

TEST(asr_response_empty)
{
    const char *json = "{\"text\":\"\"}";
    char text[128];
    text[0] = 'x';
    /* Should succeed (parse ok) but return empty string */
    ASSERT(telegram_voice_parse_transcription(json, text, sizeof(text)));
    ASSERT(text[0] == '\0');
    return 0;
}

TEST(voice_not_configured)
{
    /* Before init, is_configured should be false */
    /* Note: s_asr_configured starts as false in a fresh process */
    ASSERT(!telegram_voice_is_configured());
    return 0;
}

#define RUN(name) do { \
    printf("  %-40s", #name); \
    int r = test_##name(); \
    if (r) { failures += r; printf("\n"); } \
    else { printf("PASS\n"); } \
} while(0)

int test_telegram_voice_all(void)
{
    int failures = 0;
    printf("\n--- Telegram Voice Tests ---\n");

    RUN(multipart_body_construction);
    RUN(getfile_response_parsing);
    RUN(asr_response_parsing);
    RUN(asr_response_empty);
    RUN(voice_not_configured);

    return failures;
}
