// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "voice_capture_pipeline.h"

#include "voice_utils.h"

#include <base/log.h>

#include <algorithm>
#include <cmath>

#if defined(CONF_RNNOISE)
#include <rnnoise.h>
#endif

namespace
{

#if defined(CONF_RNNOISE)
	static constexpr int RNNOISE_FRAME_SAMPLES = 480;
#endif

	void ApplyCaptureHpfCompressor(const SRClientVoiceConfigSnapshot &Config, int16_t *pSamples, int Count, float &PrevIn, float &PrevOut, float &Env)
	{
		if(!Config.m_QmVoiceFilterEnable)
			return;

		const float CutoffHz = VOICE_HPF_CUTOFF_HZ;
		const float Rc = 1.0f / (2.0f * 3.14159265f * CutoffHz);
		const float Dt = 1.0f / VOICE_SAMPLE_RATE;
		const float Alpha = Rc / (Rc + Dt);

		const float Threshold = std::clamp(Config.m_QmVoiceCompThreshold / 100.0f, 0.01f, 1.0f);
		const float Ratio = std::max(1.0f, Config.m_QmVoiceCompRatio / 10.0f);
		const float AttackSec = std::max(0.001f, Config.m_QmVoiceCompAttackMs / 1000.0f);
		const float ReleaseSec = std::max(0.001f, Config.m_QmVoiceCompReleaseMs / 1000.0f);
		const float MakeupGain = std::max(0.0f, Config.m_QmVoiceCompMakeup / 100.0f);
		const float NoiseFloor = 0.02f;
		const float Limiter = std::clamp(Config.m_QmVoiceLimiter / 100.0f, 0.05f, 1.0f);
		const float AttackCoeff = 1.0f - std::exp(-1.0f / (AttackSec * VOICE_SAMPLE_RATE));
		const float ReleaseCoeff = 1.0f - std::exp(-1.0f / (ReleaseSec * VOICE_SAMPLE_RATE));

		for(int i = 0; i < Count; i++)
		{
			const float X = pSamples[i] / 32768.0f;
			const float Y = Alpha * (PrevOut + X - PrevIn);
			PrevIn = X;
			PrevOut = VoiceUtils::SanitizeFloat(Y);

			const float AbsY = std::fabs(PrevOut);
			if(AbsY > Env)
				Env += (AbsY - Env) * AttackCoeff;
			else
				Env += (AbsY - Env) * ReleaseCoeff;

			float Gain = 1.0f;
			if(Env > Threshold)
				Gain = (Threshold + (Env - Threshold) / Ratio) / Env;
			if(Env > NoiseFloor)
				Gain *= MakeupGain;

			const float Out = std::clamp(PrevOut * Gain, -Limiter, Limiter);
			const int Sample = (int)std::clamp(Out * 32767.0f, -32768.0f, 32767.0f);
			pSamples[i] = (int16_t)Sample;
		}
	}

	void ApplyNoiseSuppressorSimple(const SRClientVoiceConfigSnapshot &Config, int16_t *pSamples, int Count, float &NoiseFloor, float &Gate)
	{
		const float Strength = std::clamp(Config.m_QmVoiceNoiseSuppressStrength / 100.0f, 0.0f, 1.0f);
		if(Strength <= 0.0f)
			return;

		const float Rms = VoiceUtils::VoiceFrameRms(pSamples, Count);
		if(!std::isfinite(Rms))
			return;

		if(NoiseFloor <= 0.0f)
			NoiseFloor = Rms;

		const float UpdateFast = 0.2f;
		const float UpdateSlow = 0.05f;
		if(Rms < NoiseFloor * 1.2f)
			NoiseFloor += (Rms - NoiseFloor) * UpdateFast;
		else if(Rms < NoiseFloor * 1.5f)
			NoiseFloor += (Rms - NoiseFloor) * UpdateSlow;

		NoiseFloor = std::clamp(NoiseFloor, 1.0f / 32768.0f, 0.5f);

		const float MinGain = 1.0f - Strength * 0.9f;
		const float Low = 1.2f;
		const float High = 2.5f;
		const float Snr = Rms / (NoiseFloor + 1e-6f);

		float Target = 1.0f;
		if(Snr <= Low)
		{
			Target = MinGain;
		}
		else if(Snr >= High)
		{
			Target = 1.0f;
		}
		else
		{
			const float T = (Snr - Low) / (High - Low);
			Target = MinGain + (1.0f - MinGain) * T;
		}

		const float Dt = Count / (float)VOICE_SAMPLE_RATE;
		const float AttackSec = 0.01f;
		const float ReleaseSec = 0.08f;
		const float AttackCoeff = 1.0f - std::exp(-Dt / AttackSec);
		const float ReleaseCoeff = 1.0f - std::exp(-Dt / ReleaseSec);

		if(Target > Gate)
			Gate += (Target - Gate) * AttackCoeff;
		else
			Gate += (Target - Gate) * ReleaseCoeff;

		Gate = std::clamp(Gate, MinGain, 1.0f);

		if(Gate >= 0.999f)
			return;

		for(int i = 0; i < Count; i++)
		{
			const float Out = pSamples[i] * Gate;
			pSamples[i] = (int16_t)std::clamp(Out, -32768.0f, 32767.0f);
		}
	}

	bool EnsureRnnoiseState(DenoiseState *&pState)
	{
#if defined(CONF_RNNOISE)
		if(!pState)
			pState = rnnoise_create(nullptr);
		return pState != nullptr;
#else
		(void)pState;
		return false;
#endif
	}

	void ApplyNoiseSuppressor(const SRClientVoiceConfigSnapshot &Config, int16_t *pSamples, int Count, float &NoiseFloor, float &Gate, DenoiseState *&pState, bool &FallbackLogged)
	{
		if(Config.m_QmVoiceNoiseSuppressEnable == VOICE_NOISE_SUPPRESS_OFF)
		{
			FallbackLogged = false;
			return;
		}

		const float Strength = std::clamp(Config.m_QmVoiceNoiseSuppressStrength / 100.0f, 0.0f, 1.0f);
		if(Strength <= 0.0f)
		{
			FallbackLogged = false;
			return;
		}

		bool FallbackUsed = false;
		const bool WantsRnnoise = Config.m_QmVoiceNoiseSuppressEnable == VOICE_NOISE_SUPPRESS_RNNOISE;
		const int Mode = VoiceUtils::ResolveNoiseSuppressMode(Config.m_QmVoiceNoiseSuppressEnable, WantsRnnoise && EnsureRnnoiseState(pState), &FallbackUsed);
		if(FallbackUsed)
		{
			if(!FallbackLogged)
			{
				log_warn("voice", "RNNoise mode requested but unavailable, falling back to simple noise suppression");
				FallbackLogged = true;
			}
		}
		else
		{
			FallbackLogged = false;
		}

		if(Mode == VOICE_NOISE_SUPPRESS_SIMPLE)
		{
			ApplyNoiseSuppressorSimple(Config, pSamples, Count, NoiseFloor, Gate);
			return;
		}

#if defined(CONF_RNNOISE)
		const int FrameSize = rnnoise_get_frame_size();
		if(FrameSize != RNNOISE_FRAME_SAMPLES || Count < FrameSize)
		{
			ApplyNoiseSuppressorSimple(Config, pSamples, Count, NoiseFloor, Gate);
			return;
		}

		const int Frames = Count / FrameSize;
		for(int f = 0; f < Frames; f++)
		{
			int16_t aDry[RNNOISE_FRAME_SAMPLES];
			float aIn[RNNOISE_FRAME_SAMPLES];
			float aOut[RNNOISE_FRAME_SAMPLES];
			const int Base = f * FrameSize;
			for(int i = 0; i < FrameSize; i++)
			{
				aDry[i] = pSamples[Base + i];
				aIn[i] = (float)pSamples[Base + i];
			}

			rnnoise_process_frame(pState, aOut, aIn);

			for(int i = 0; i < FrameSize; i++)
			{
				const float Y = aOut[i];
				const int Wet = (int)std::clamp(Y, -32768.0f, 32767.0f);
				pSamples[Base + i] = (int16_t)Wet;
			}
			VoiceUtils::BlendDenoisedFrame(aDry, pSamples + Base, FrameSize, Strength);
		}
#else
		ApplyNoiseSuppressorSimple(Config, pSamples, Count, NoiseFloor, Gate);
#endif
	}

} // namespace

namespace VoiceUtils
{

	void ProcessVoiceCaptureFrame(
		const SRClientVoiceConfigSnapshot &Config,
		int16_t *pSamples,
		int Count,
		float &AgcGain,
		float &NoiseFloor,
		float &NoiseGate,
		DenoiseState *&pNoiseState,
		bool &NoiseFallbackLogged,
		float &HpfPrevIn,
		float &HpfPrevOut,
		float &CompEnv)
	{
		TraceVoiceProcessStage(EVoiceProcessStage::AGC_GAIN);
		const float FrameRms = VoiceFrameRms(pSamples, Count);
		AgcGain = ComputeVoiceAutoGain(AgcGain, FrameRms, VoiceAgcConfigFromRuntime(Config.m_QmVoiceAgcEnable != 0));
		TraceVoiceProcessStage(EVoiceProcessStage::MIC_GAIN);
		ApplyMicGain(std::clamp((Config.m_QmVoiceMicVolume / 100.0f) * AgcGain, 0.0f, 3.0f), pSamples, Count);
		TraceVoiceProcessStage(EVoiceProcessStage::DENOISE);
		ApplyNoiseSuppressor(Config, pSamples, Count, NoiseFloor, NoiseGate, pNoiseState, NoiseFallbackLogged);
		TraceVoiceProcessStage(EVoiceProcessStage::HPF_COMPRESSOR);
		ApplyCaptureHpfCompressor(Config, pSamples, Count, HpfPrevIn, HpfPrevOut, CompEnv);
	}

} // namespace VoiceUtils
