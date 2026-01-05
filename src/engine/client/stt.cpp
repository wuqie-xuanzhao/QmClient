/* Speech-to-Text component implementation using whisper.cpp */
#include "stt.h"

#include <base/log.h>
#include <engine/console.h>
#include <engine/storage.h>

#include <SDL.h>

// Audio settings for whisper.cpp (requires 16kHz mono)
static constexpr int STT_SAMPLE_RATE = 16000;
static constexpr int STT_AUDIO_CHANNELS = 1;
static constexpr int STT_AUDIO_SAMPLES = 1024;

CStt::CStt()
{
#if defined(CONF_WHISPER)
	m_WhisperParams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
	m_WhisperParams.print_progress = false;
	m_WhisperParams.print_special = false;
	m_WhisperParams.print_realtime = false;
	m_WhisperParams.print_timestamps = false;
	m_WhisperParams.translate = false;
	m_WhisperParams.single_segment = false; // Allow multiple segments for better accuracy
	m_WhisperParams.no_context = false; // Use context for better coherence
	m_WhisperParams.language = "zh";
	m_WhisperParams.n_threads = 4;
	// Prompt to guide model for simplified Chinese output
	m_WhisperParams.initial_prompt = "以下是普通话的句子，请使用简体中文输出。";
#endif
}

void CStt::SetLanguage(const char *pLang)
{
	if(pLang && pLang[0])
	{
		str_copy(m_aLanguage, pLang);
	}
	else
	{
		str_copy(m_aLanguage, "auto");
	}
}

CStt::~CStt()
{
	Shutdown();
}

bool CStt::Init(IConsole *pConsole, IStorage *pStorage, const char *pModelPath)
{
#if !defined(CONF_WHISPER)
	log_warn("stt", "STT not available - compiled without CONF_WHISPER");
	return false;
#else
	m_pConsole = pConsole;
	m_pStorage = pStorage;

	if(m_State != STATE_IDLE && m_State != STATE_ERROR)
	{
		log_warn("stt", "STT already initialized");
		return false;
	}

	// Load whisper model
	if(!LoadModel(pModelPath))
	{
		return false;
	}

	log_info("stt", "STT initialized successfully");
	return true;
#endif
}

void CStt::Shutdown()
{
	// Stop recording if active
	if(m_State == STATE_RECORDING)
	{
		StopRecording();
	}

	// Stop transcription thread
	m_StopThread = true;
	if(m_TranscribeThread.joinable())
	{
		m_TranscribeThread.join();
	}

	// Close audio device
	CloseAudioDevice();

	// Unload model
	UnloadModel();

	m_State = STATE_IDLE;
	log_info("stt", "STT shutdown complete");
}

bool CStt::LoadModel(const char *pModelPath)
{
#if !defined(CONF_WHISPER)
	return false;
#else
	m_State = STATE_LOADING_MODEL;

	// Get full path - use binary path for model files in data directory
	char aFullPath[IO_MAX_PATH_LENGTH];
	if(m_pStorage)
	{
		// Try to get path relative to executable
		m_pStorage->GetBinaryPath(pModelPath, aFullPath, sizeof(aFullPath));
	}
	else
	{
		str_copy(aFullPath, pModelPath);
	}

	log_info("stt", "Loading whisper model: %s", aFullPath);

	// Initialize whisper
	whisper_context_params cparams = whisper_context_default_params();
	m_pWhisperCtx = whisper_init_from_file_with_params(aFullPath, cparams);

	if(!m_pWhisperCtx)
	{
		str_copy(m_aError, "Failed to load whisper model");
		log_error("stt", "%s: %s", m_aError, aFullPath);
		m_State = STATE_ERROR;
		return false;
	}

	m_State = STATE_READY;
	log_info("stt", "Whisper model loaded successfully");
	return true;
#endif
}

void CStt::UnloadModel()
{
#if defined(CONF_WHISPER)
	if(m_pWhisperCtx)
	{
		whisper_free(m_pWhisperCtx);
		m_pWhisperCtx = nullptr;
	}
#endif
}

bool CStt::OpenAudioDevice()
{
	if(m_AudioDeviceOpened)
	{
		return true;
	}

	SDL_AudioSpec wanted, obtained;
	SDL_zero(wanted);
	wanted.freq = STT_SAMPLE_RATE;
	wanted.format = AUDIO_F32;
	wanted.channels = STT_AUDIO_CHANNELS;
	wanted.samples = STT_AUDIO_SAMPLES;
	wanted.callback = AudioCallback;
	wanted.userdata = this;

	m_AudioDeviceId = SDL_OpenAudioDevice(nullptr, 1, &wanted, &obtained, 0);
	if(m_AudioDeviceId == 0)
	{
		str_format(m_aError, sizeof(m_aError), "Failed to open audio device: %s", SDL_GetError());
		log_error("stt", "%s", m_aError);
		return false;
	}

	if(obtained.freq != STT_SAMPLE_RATE)
	{
		log_warn("stt", "Audio device sample rate %d differs from required %d", obtained.freq, STT_SAMPLE_RATE);
	}

	m_AudioDeviceOpened = true;
	log_info("stt", "Audio capture device opened (freq=%d, channels=%d)", obtained.freq, obtained.channels);
	return true;
}

void CStt::CloseAudioDevice()
{
	if(m_AudioDeviceOpened && m_AudioDeviceId != 0)
	{
		SDL_CloseAudioDevice(m_AudioDeviceId);
		m_AudioDeviceId = 0;
		m_AudioDeviceOpened = false;
		log_info("stt", "Audio capture device closed");
	}
}

void CStt::AudioCallback(void *pUserData, unsigned char *pStream, int Len)
{
	CStt *pStt = static_cast<CStt *>(pUserData);

	// Convert bytes to float samples
	const float *pSamples = reinterpret_cast<const float *>(pStream);
	const int NumSamples = Len / sizeof(float);

	std::lock_guard<std::mutex> Lock(pStt->m_AudioMutex);
	pStt->m_AudioBuffer.insert(pStt->m_AudioBuffer.end(), pSamples, pSamples + NumSamples);
}

void CStt::StartRecording()
{
#if !defined(CONF_WHISPER)
	log_warn("stt", "STT not available");
	return;
#else
	if(m_State != STATE_READY)
	{
		log_warn("stt", "Cannot start recording - not ready (state=%d)", (int)m_State.load());
		return;
	}

	// Open audio device if not already open
	if(!OpenAudioDevice())
	{
		return;
	}

	// Clear audio buffer
	{
		std::lock_guard<std::mutex> Lock(m_AudioMutex);
		m_AudioBuffer.clear();
		m_AudioBuffer.reserve(STT_SAMPLE_RATE * 30); // Reserve 30 seconds
	}

	// Start audio capture
	SDL_PauseAudioDevice(m_AudioDeviceId, 0);

	m_State = STATE_RECORDING;
	log_info("stt", "Recording started");
#endif
}

void CStt::StopRecording()
{
#if !defined(CONF_WHISPER)
	return;
#else
	if(m_State != STATE_RECORDING)
	{
		return;
	}

	// Stop audio capture
	SDL_PauseAudioDevice(m_AudioDeviceId, 1);

	log_info("stt", "Recording stopped, starting transcription...");

	// Start transcription in background thread
	m_State = STATE_TRANSCRIBING;

	// Join previous thread if any
	if(m_TranscribeThread.joinable())
	{
		m_TranscribeThread.join();
	}

	m_StopThread = false;
	m_TranscribeThread = std::thread(&CStt::TranscribeAudio, this);
#endif
}

void CStt::TranscribeAudio()
{
#if !defined(CONF_WHISPER)
	return;
#else
	if(!m_pWhisperCtx)
	{
		m_State = STATE_ERROR;
		return;
	}

	// Copy audio buffer
	std::vector<float> audioData;
	{
		std::lock_guard<std::mutex> Lock(m_AudioMutex);
		audioData = std::move(m_AudioBuffer);
		m_AudioBuffer.clear();
	}

	if(audioData.empty())
	{
		log_warn("stt", "No audio data to transcribe");
		m_State = STATE_READY;
		return;
	}

	log_info("stt", "Transcribing %zu samples (%.1f seconds)...",
		audioData.size(), static_cast<float>(audioData.size()) / STT_SAMPLE_RATE);

	// Run whisper inference with configured language
	m_WhisperParams.language = m_aLanguage;
	int result = whisper_full(m_pWhisperCtx, m_WhisperParams, audioData.data(), audioData.size());

	if(result != 0)
	{
		std::lock_guard<std::mutex> Lock(m_ResultMutex);
		str_copy(m_aError, "Whisper transcription failed");
		log_error("stt", "%s", m_aError);
		m_State = STATE_READY;
		return;
	}

	// Get transcription result
	std::string transcription;
	const int numSegments = whisper_full_n_segments(m_pWhisperCtx);
	for(int i = 0; i < numSegments; i++)
	{
		const char *pText = whisper_full_get_segment_text(m_pWhisperCtx, i);
		if(pText)
		{
			transcription += pText;
		}
	}

	// Store result
	{
		std::lock_guard<std::mutex> Lock(m_ResultMutex);
		str_copy(m_aLastTranscription, transcription.c_str());
		m_HasNewResult = true;
	}

	log_info("stt", "Transcription complete: \"%s\"", transcription.c_str());

	m_State = STATE_READY;
#endif
}

void CStt::Update()
{
	// Check for new transcription results
	bool hasNew = false;
	char aText[2048] = {0};

	{
		std::lock_guard<std::mutex> Lock(m_ResultMutex);
		if(m_HasNewResult)
		{
			str_copy(aText, m_aLastTranscription);
			m_HasNewResult = false;
			hasNew = true;
		}
	}

	// Call callback if we have new result
	if(hasNew && m_TranscriptionCallback && aText[0] != '\0')
	{
		m_TranscriptionCallback(aText, true);
	}
}

const char *CStt::GetStateString() const
{
	switch(m_State)
	{
	case STATE_IDLE: return "Idle";
	case STATE_LOADING_MODEL: return "Loading Model...";
	case STATE_READY: return "Ready";
	case STATE_RECORDING: return "Recording...";
	case STATE_TRANSCRIBING: return "Transcribing...";
	case STATE_ERROR: return "Error";
	default: return "Unknown";
	}
}
