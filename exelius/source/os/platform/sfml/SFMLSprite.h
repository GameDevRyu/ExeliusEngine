#pragma once
#include "source/os/platform/PlatformForwardDeclarations.h"
#include "source/utility/math/Rectangle.h"
#include "source/utility/generic/Color.h"

#include <cstddef>

/// <summary>
/// SFML namespace, used only in SFML specific code within Exelius.
/// </summary>
namespace sf
{
	class Sprite;
}

/// <summary>
/// Engine namespace. Everything owned by the engine will be inside this namespace.
/// </summary>
namespace Exelius
{
	FORWARD_DECLARE(Texture);

	class SFMLSprite
	{
		sf::Sprite* m_pSFMLSprite;
	public:
		/// <summary>
		/// Default Constructor - Creates an empty Sprite with no texture.
		/// </summary>
		SFMLSprite();

		SFMLSprite(Texture& texture);
		SFMLSprite(Texture& texture, const IRectangle& rectangle);

		// TODO:
		// Rule of 5
		~SFMLSprite();

		IRectangle GetTextureRectangle() const;
		void SetTextureRect(const IRectangle& rectangle);
		void SetTexture(Texture& texture, bool resetRectangle = false);

		Color GetColor() const;
		void SetColor(const Color& color);

		FRectangle GetLocalBounds() const;
		FRectangle GetGlobalBounds() const;

		Vector2f GetPosition() const;
		void SetPosition(float x, float y);
		void SetPosition(const Vector2f& position);
		void Move(float xOffset, float yOffset);
		void Move(const Vector2f& offset);

		Vector2f GetScale() const;
		void SetScale(float xFactor, float yFactor);
		void SetScale(const Vector2f& factors);
		void Scale(float xFactorOffset, float yFactorOffset);
		void Scale(const Vector2f& factorOffsets);

		Vector2f GetOrigin() const;
		void SetOrigin(float x, float y);
		void SetOrigin(const Vector2f& origin);

		float GetRotation() const;
		void SetRotation(float angle);
		void Rotate(float angleOffset);

		void Render();

		sf::Sprite* GetSFMLSprite() { return m_pSFMLSprite; }
	};
}