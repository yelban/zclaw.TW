#ifndef TELEGRAM_VOICE_H
#define TELEGRAM_VOICE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

// Load ASR configuration from NVS. Call once during init.
esp_err_t telegram_voice_init(void);

// Returns true if an ASR API key is available.
bool telegram_voice_is_configured(void);

// Download voice file from Telegram and transcribe via ASR API.
// Writes transcribed text into out_text (null-terminated, up to out_text_size).
esp_err_t telegram_voice_transcribe(const char *bot_token, const char *file_id,
                                     char *out_text, size_t out_text_size);

// --- Testable helpers (exposed for host tests) ---

// Parse Telegram getFile JSON response and extract file_path.
// Returns true on success, copies path into out_path.
// If out_file_size is non-NULL, writes the file_size field (0 if absent).
bool telegram_voice_parse_file_path(const char *json, char *out_path, size_t out_path_size,
                                     size_t *out_file_size);

// Parse Whisper-compatible ASR JSON response and extract text.
// Returns true on success, copies transcription into out_text.
bool telegram_voice_parse_transcription(const char *json, char *out_text, size_t out_text_size);

// Build multipart preamble (everything before OGG binary).
// Returns number of bytes written to buf (excluding NUL), or -1 on error.
int telegram_voice_build_multipart_preamble(char *buf, size_t buf_size,
                                             const char *model, const char *language,
                                             const char *prompt, const char *boundary);

// Build multipart epilogue (everything after OGG binary).
// Returns number of bytes written to buf (excluding NUL), or -1 on error.
int telegram_voice_build_multipart_epilogue(char *buf, size_t buf_size, const char *boundary);

#endif // TELEGRAM_VOICE_H
