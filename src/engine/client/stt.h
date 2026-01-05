/* Speech-to-Text component using whisper.cpp */
#ifndef ENGINE_CLIENT_STT_H
#define ENGINE_CLIENT_STT_H

#include <base/system.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(CONF_WHISPER)
#include <whisper.h>
#endif

class IConsole;
class IStorage;

class CStt
{
public:
	enum EState
	{
		STATE_IDLE,
		STATE_LOADING_MODEL,
		STATE_READY,
		STATE_RECORDING,
		STATE_TRANSCRIBING,
		STATE_ERROR
	};

	using FTranscriptionCallback = std::function<void(const char *pText, bool IsFinal)>;

	CStt();
	~CStt();

	// Initialize STT with model path
	bool Init(IConsole *pConsole, IStorage *pStorage, const char *pModelPath);
	void Shutdown();

	// Recording control (called from main thread via +stt command)
	void StartRecording();
	void StopRecording();
	bool IsRecording() const { return m_State == STATE_RECORDING; }

	// Update - call from main loop to process results
	void Update();

	// Get current state
	EState GetState() const { return m_State; }
	const char *GetStateString() const;

	// Set callback for transcription results
	void SetTranscriptionCallback(FTranscriptionCallback Callback) { m_TranscriptionCallback = std::move(Callback); }

	// Get last transcription
	const char *GetLastTranscription() const { return m_aLastTranscription; }

	// Get last error
	const char *GetError() const { return m_aError; }

	// Check if model is loaded
	bool IsModelLoaded() const { return m_State >= STATE_READY; }

	// Set language for transcription (e.g. "zh", "en", "auto")
	void SetLanguage(const char *pLang);

private:
#if defined(CONF_WHISPER)
	// Whisper context
	whisper_context *m_pWhisperCtx = nullptr;
	whisper_full_params m_WhisperParams;
#endif

	// Audio capture (SDL)
	unsigned int m_AudioDeviceId = 0;
	bool m_AudioDeviceOpened = false;

	// Audio buffer
	std::vector<float> m_AudioBuffer;
	std::mutex m_AudioMutex;

	// Language setting
	char m_aLanguage[16] = "auto";

	// State
	std::atomic<EState> m_State{STATE_IDLE};

	// Result buffer
	char m_aLastTranscription[2048] = {0};
	char m_aError[256] = {0};
	std::mutex m_ResultMutex;
	bool m_HasNewResult = false;

	// Callback
	FTranscriptionCallback m_TranscriptionCallback;

	// Console for logging
	IConsole *m_pConsole = nullptr;
	IStorage *m_pStorage = nullptr;

	// Background thread for transcription
	std::thread m_TranscribeThread;
	std::atomic<bool> m_StopThread{false};

	// Internal methods
	bool LoadModel(const char *pModelPath);
	void UnloadModel();
	bool OpenAudioDevice();
	void CloseAudioDevice();
	void TranscribeAudio();

	// SDL audio callback
	static void AudioCallback(void *pUserData, unsigned char *pStream, int Len);
};

#endif // ENGINE_CLIENT_STT_H
