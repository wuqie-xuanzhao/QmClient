#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_PET_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_PET_H

#include <game/client/component.h>

#include <algorithm>
#include <cmath>

inline void QmTClientPetAdvanceSpring(vec2 &Position, vec2 &Velocity, vec2 Target, float Delta)
{
	Position += Velocity * Delta;
	const vec2 DeltaPosition = Target - Position;
	const float DeltaLength = length(DeltaPosition);
	constexpr float SpringStrength = 50.0f;
	const vec2 DeltaDamped = (Velocity * -2.0f * std::sqrt(SpringStrength) + DeltaPosition * SpringStrength) * Delta;
	constexpr float Friction = 0.01f;
	const vec2 DeltaWizzy = (Velocity + DeltaPosition * Delta * 50.0f) * std::pow(Friction, Delta) - Velocity;
	Velocity += mix(DeltaDamped, DeltaWizzy, std::clamp(DeltaLength / 64.0f, 0.0f, 1.0f));
}

class CPet : public CComponent
{
private:
	vec2 m_Target;
	vec2 m_Position;
	vec2 m_Velocity;
	vec2 m_Dir;
	float m_Alpha = 0.0f;
	int m_FollowedClientId = -1;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnMapLoad() override;
	bool IsVisibleForClient(int ClientId) const { return ClientId >= 0 && m_FollowedClientId == ClientId && m_Alpha > 0.0f; }
	vec2 Position() const { return m_Position; }
};

#endif
