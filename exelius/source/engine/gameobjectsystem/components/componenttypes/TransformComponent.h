#pragma once
#include "source/engine/gameobjectsystem/components/Component.h"
#include "source/utility/containers/Vector2.h"

/// <summary>
/// Engine namespace. Everything owned by the engine will be inside this namespace.
/// </summary>
namespace Exelius
{
	class TransformComponent
		: public Component
	{
		Vector2f m_position;
		Vector2f m_dimensions;
	public:
		DEFINE_COMPONENT(TransformComponent);

		TransformComponent(GameObject* pOwner)
			: Component(pOwner)
			, m_position({ 0.0f, 0.0f })
			, m_dimensions({ 0.0f, 0.0f })
		{
			//
		}

		virtual bool Initialize() final override;
		virtual bool Initialize(const rapidjson::Value& jsonComponentData) final override;

		float GetX() const { return m_position.x; }
		void SetX(float x) { m_position.x = x; }
		float GetY() const { return m_position.y; }
		void SetY(float y) { m_position.y = y; }
		float GetW() const { return m_dimensions.x; }
		void SetW(float w) { m_dimensions.x = w; }
		float GetH() const { return m_dimensions.y; }
		void SetH(float h) { m_dimensions.y = h; }

		void Move(float x, float y)
		{
			m_position.x = m_position.x + x;
			m_position.y = m_position.y + y;
		}
	};
}