#include "whisper_engine.h"
#include "logger.h"
#include "whisper.h"

namespace autocaption {

WhisperEngine::WhisperEngine() = default;

WhisperEngine::~WhisperEngine() {
    shutdown();
}

bool WhisperEngine::initialize(const WhisperConfig& config) {
    config_ = config;
    
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
    
    // Load model
    LOG_INFO("Loading Whisper model: " + config.model_path);
    
    whisper_context_params ctx_params = whisper_context_default_params();
    ctx_params.use_gpu = config.use_gpu;
    
    ctx_ = whisper_init_from_file_with_params(config.model_path.c_str(), ctx_params);
    if (!ctx_) {
        LOG_ERROR("Failed to load Whisper model");
        return false;
    }
    
    LOG_INFO("Whisper model loaded successfully");
    LOG_INFO("Using " + std::to_string(config.n_threads) + " threads");
    
    return true;
}

std::vector<WhisperEngine::Transcription> WhisperEngine::transcribe(
    const float* audio, size_t samples, uint64_t /*timestamp_offset_ms*/) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Transcription> results;
    
    if (!ctx_) {
        LOG_ERROR("Whisper not initialized");
        return results;
    }
    
    // Skip if audio is too short (< 1 second)
    if (samples < (size_t)SAMPLE_RATE) {
        LOG_DEBUG("Audio too short for transcription: " + std::to_string(samples) + " samples");
        return results;
    }
    
    // Set up parameters
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.translate = config_.translate;
    params.single_segment = false;
    params.max_tokens = 0;
    params.language = config_.language == "auto" ? nullptr : config_.language.c_str();
    params.n_threads = config_.n_threads;
    params.beam_search.beam_size = config_.beam_size;
    
    // Process audio
    int ret = whisper_full(ctx_, params, audio, static_cast<int>(samples));
    if (ret != 0) {
        LOG_ERROR("Whisper processing failed: " + std::to_string(ret));
        return results;
    }
    
    // Extract results
    int n_segments = whisper_full_n_segments(ctx_);
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx_, i);
        int64_t t0 = whisper_full_get_segment_t0(ctx_, i);
        int64_t t1 = whisper_full_get_segment_t1(ctx_, i);
        
        if (text && strlen(text) > 0) {
            Transcription trans;
            trans.start_time = static_cast<float>(t0) / 100.0f; // Convert from centiseconds
            trans.end_time = static_cast<float>(t1) / 100.0f;
            trans.text = text;
            trans.confidence = 1.0f; // Could extract from whisper
            trans.language = config_.language;
            
            results.push_back(trans);
        }
    }
    
    return results;
}

std::vector<WhisperEngine::Transcription> WhisperEngine::transcribeStreaming(
    const float* audio, size_t samples, bool /*use_previous_context*/) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Transcription> results;
    
    if (!ctx_) {
        LOG_ERROR("Whisper not initialized");
        return results;
    }
    
    // Append to buffer
    audio_buffer_.insert(audio_buffer_.end(), audio, audio + samples);
    
    // Process if we have enough data (30 seconds max)
    const size_t max_samples = SAMPLE_RATE * 30; // 30 seconds
    if (audio_buffer_.size() > max_samples) {
        // Remove old audio, keep last 5 seconds for context
        size_t keep_samples = SAMPLE_RATE * 5;
        audio_buffer_.erase(audio_buffer_.begin(), 
                           audio_buffer_.end() - keep_samples);
    }
    
    // Only process if we have at least 1 second of audio
    if (audio_buffer_.size() < static_cast<size_t>(SAMPLE_RATE)) {
        return results;
    }
    
    // Set up parameters
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.translate = config_.translate;
    params.single_segment = false;
    params.max_tokens = 0;
    params.language = config_.language == "auto" ? nullptr : config_.language.c_str();
    params.n_threads = config_.n_threads;
    params.no_context = true;  // Don't use context for streaming
    
    // Process audio
    int ret = whisper_full(ctx_, params, audio_buffer_.data(), 
                           static_cast<int>(audio_buffer_.size()));
    if (ret != 0) {
        LOG_ERROR("Whisper streaming processing failed: " + std::to_string(ret));
        return results;
    }
    
    // Extract results
    int n_segments = whisper_full_n_segments(ctx_);
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx_, i);
        int64_t t0 = whisper_full_get_segment_t0(ctx_, i);
        int64_t t1 = whisper_full_get_segment_t1(ctx_, i);
        
        if (text && strlen(text) > 0) {
            Transcription trans;
            trans.start_time = static_cast<float>(t0) / 100.0f;
            trans.end_time = static_cast<float>(t1) / 100.0f;
            trans.text = text;
            trans.confidence = 1.0f;
            trans.language = config_.language;
            
            results.push_back(trans);
        }
    }
    
    return results;
}

std::vector<std::pair<std::string, std::string>> WhisperEngine::getSupportedLanguages() {
    std::vector<std::pair<std::string, std::string>> languages;
    
    // Common Whisper languages
    languages = {
        {"auto", "Auto-detect"},
        {"en", "English"},
        {"zh", "Chinese"},
        {"de", "German"},
        {"es", "Spanish"},
        {"ru", "Russian"},
        {"ko", "Korean"},
        {"fr", "French"},
        {"ja", "Japanese"},
        {"pt", "Portuguese"},
        {"tr", "Turkish"},
        {"pl", "Polish"},
        {"ca", "Catalan"},
        {"nl", "Dutch"},
        {"ar", "Arabic"},
        {"sv", "Swedish"},
        {"it", "Italian"},
        {"id", "Indonesian"},
        {"hi", "Hindi"},
        {"fi", "Finnish"},
        {"vi", "Vietnamese"},
        {"he", "Hebrew"},
        {"uk", "Ukrainian"},
        {"el", "Greek"},
        {"ms", "Malay"},
        {"cs", "Czech"},
        {"ro", "Romanian"},
        {"da", "Danish"},
        {"hu", "Hungarian"},
        {"ta", "Tamil"},
        {"no", "Norwegian"},
        {"th", "Thai"},
        {"ur", "Urdu"},
        {"hr", "Croatian"},
        {"bg", "Bulgarian"},
        {"lt", "Lithuanian"},
        {"la", "Latin"},
        {"mi", "Maori"},
        {"ml", "Malayalam"},
        {"cy", "Welsh"},
        {"sk", "Slovak"},
        {"te", "Telugu"},
        {"fa", "Persian"},
        {"lv", "Latvian"},
        {"bn", "Bengali"},
        {"sr", "Serbian"},
        {"az", "Azerbaijani"},
        {"sl", "Slovenian"},
        {"kn", "Kannada"},
        {"et", "Estonian"},
        {"mk", "Macedonian"},
        {"br", "Breton"},
        {"eu", "Basque"},
        {"is", "Icelandic"},
        {"hy", "Armenian"},
        {"ne", "Nepali"},
        {"mn", "Mongolian"},
        {"bs", "Bosnian"},
        {"kk", "Kazakh"},
        {"sq", "Albanian"},
        {"sw", "Swahili"},
        {"gl", "Galician"},
        {"mr", "Marathi"},
        {"pa", "Punjabi"},
        {"si", "Sinhala"},
        {"km", "Khmer"},
        {"sn", "Shona"},
        {"yo", "Yoruba"},
        {"so", "Somali"},
        {"af", "Afrikaans"},
        {"oc", "Occitan"},
        {"ka", "Georgian"},
        {"be", "Belarusian"},
        {"tg", "Tajik"},
        {"sd", "Sindhi"},
        {"gu", "Gujarati"},
        {"am", "Amharic"},
        {"yi", "Yiddish"},
        {"lo", "Lao"},
        {"uz", "Uzbek"},
        {"fo", "Faroese"},
        {"ht", "Haitian Creole"},
        {"ps", "Pashto"},
        {"tk", "Turkmen"},
        {"nn", "Nynorsk"},
        {"mt", "Maltese"},
        {"sa", "Sanskrit"},
        {"lb", "Luxembourgish"},
        {"my", "Myanmar"},
        {"bo", "Tibetan"},
        {"tl", "Tagalog"},
        {"mg", "Malagasy"},
        {"as", "Assamese"},
        {"tt", "Tatar"},
        {"haw", "Hawaiian"},
        {"ln", "Lingala"},
        {"ha", "Hausa"},
        {"ba", "Bashkir"},
        {"jw", "Javanese"},
        {"su", "Sundanese"},
    };
    
    return languages;
}

void WhisperEngine::shutdown() {
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
    audio_buffer_.clear();
}

// ASRPipeline implementation
ASRPipeline::ASRPipeline() = default;

ASRPipeline::~ASRPipeline() {
    shutdown();
}

bool ASRPipeline::initialize(const ASRConfig& config) {
    config_ = config;
    
    // Initialize whisper
    whisper_ = std::make_unique<WhisperEngine>();
    if (!whisper_->initialize(config.whisper_config)) {
        LOG_ERROR("Failed to initialize Whisper engine");
        return false;
    }
    
    // Initialize VAD
    vad_ = std::make_unique<VAD>(config.vad_config);
    
    LOG_INFO("ASR Pipeline initialized");
    return true;
}

void ASRPipeline::shutdown() {
    running_ = false;
    if (whisper_) {
        whisper_->shutdown();
    }
}

void ASRPipeline::processLoop(AudioSegmentQueue& queue) {
    running_ = true;
    
    // Configuration
    const size_t MIN_PROCESS_MS = 1500;      // Minimum audio to process (1.5s to be safe)
    const size_t STEP_MS = config_.stream_window_ms;  // How often to output (3000ms)
    const size_t MAX_BUFFER_MS = 10000;      // Max buffer size (10s)
    
    const size_t min_samples = SAMPLE_RATE * MIN_PROCESS_MS / 1000;
    const size_t step_samples = SAMPLE_RATE * STEP_MS / 1000;
    const size_t max_samples = SAMPLE_RATE * MAX_BUFFER_MS / 1000;
    
    std::vector<float> audio_buffer;
    audio_buffer.reserve(max_samples);
    
    uint64_t buffer_start_ms = 0;
    size_t last_output_sample_count = 0;
    bool has_speech = false;
    
    while (running_) {
        auto segment_opt = queue.tryPop(std::chrono::milliseconds(100));
        
        if (!segment_opt) {
            // No new data - flush remaining buffer if we have enough
            if (has_speech && audio_buffer.size() >= min_samples) {
                uint64_t buffer_end_ms = buffer_start_ms + 
                    (audio_buffer.size() * 1000 / SAMPLE_RATE);
                
                LOG_INFO("Flushing remaining buffer: " + std::to_string(audio_buffer.size()) + " samples");
                processSpeechSegment(audio_buffer.data(), audio_buffer.size(),
                                    buffer_start_ms, buffer_end_ms, "");
                has_speech = false;
                audio_buffer.clear();
                last_output_sample_count = 0;
            }
            continue;
        }
        
        const auto& segment = *segment_opt;
        
        // Run VAD on segment
        auto vad_result = vad_->process(segment.data.data(), segment.data.size(), SAMPLE_RATE);
        
        if (vad_result.is_speech) {
            if (!has_speech) {
                has_speech = true;
                buffer_start_ms = segment.timestamp_ms;
                last_output_sample_count = 0;
                // Don't clear buffer if we're continuing from previous speech
            }
            
            // Add to buffer
            audio_buffer.insert(audio_buffer.end(), 
                               segment.data.begin(), 
                               segment.data.end());
            
            // Keep buffer within max size (sliding window)
            if (audio_buffer.size() > max_samples) {
                size_t excess = audio_buffer.size() - max_samples;
                audio_buffer.erase(audio_buffer.begin(), audio_buffer.begin() + excess);
                buffer_start_ms += (excess * 1000 / SAMPLE_RATE);
                // Adjust last_output count
                if (last_output_sample_count > excess) {
                    last_output_sample_count -= excess;
                } else {
                    last_output_sample_count = 0;
                }
            }
            
            // Calculate new audio since last output
            size_t new_samples = (audio_buffer.size() > last_output_sample_count) 
                ? (audio_buffer.size() - last_output_sample_count) 
                : 0;
            
            // Process when we have enough NEW audio (step size) AND total is at least min
            if (new_samples >= step_samples && audio_buffer.size() >= min_samples) {
                uint64_t buffer_end_ms = segment.timestamp_ms + 
                    (segment.data.size() * 1000 / SAMPLE_RATE);
                
                LOG_INFO("Processing window: " + std::to_string(audio_buffer.size()) + 
                         " samples, new since last: " + std::to_string(new_samples));
                
                auto results = processSpeechSegment(audio_buffer.data(), audio_buffer.size(),
                                                   buffer_start_ms, buffer_end_ms, "");
                
                // Mark position after this output
                // Keep last 500ms for context overlap
                size_t overlap_samples = SAMPLE_RATE / 2;
                if (audio_buffer.size() > overlap_samples) {
                    last_output_sample_count = audio_buffer.size() - overlap_samples;
                } else {
                    last_output_sample_count = 0;
                }
            }
        } else {
            if (has_speech) {
                // Speech ended - process remaining audio if enough
                if (audio_buffer.size() >= min_samples) {
                    uint64_t buffer_end_ms = segment.timestamp_ms;
                    
                    LOG_INFO("Processing final segment: " + std::to_string(audio_buffer.size()) + " samples");
                    processSpeechSegment(audio_buffer.data(), audio_buffer.size(),
                                        buffer_start_ms, buffer_end_ms, "");
                }
                
                // Reset for next speech segment
                has_speech = false;
                audio_buffer.clear();
                last_output_sample_count = 0;
            }
        }
    }
}

std::vector<WhisperEngine::Transcription> ASRPipeline::processSpeechSegment(
    const float* audio, size_t samples,
    uint64_t start_ms, uint64_t end_ms,
    const std::string& /*context*/) {
    
    LOG_INFO("Processing speech segment: " + std::to_string(samples) + 
             " samples (" + std::to_string((end_ms - start_ms) / 1000.0f) + "s)");
    
    // Skip if too short
    if (samples < (size_t)SAMPLE_RATE) {
        LOG_WARN("Audio too short, skipping: " + std::to_string(samples) + " samples");
        return {};
    }
    
    auto transcriptions = whisper_->transcribe(audio, samples, start_ms);
    
    for (const auto& trans : transcriptions) {
        CaptionSegment segment;
        segment.id = segment_id_++;
        segment.start_time = trans.start_time + start_ms / 1000.0f;
        segment.end_time = trans.end_time + start_ms / 1000.0f;
        segment.source_text = trans.text;
        segment.translated_text = "";
        segment.language = trans.language;
        
        LOG_INFO("Transcription [" + std::to_string(segment.id) + "]: " + trans.text);
        
        if (transcription_callback_) {
            transcription_callback_(segment);
        }
    }
    
    return transcriptions;
}

} // namespace autocaption
