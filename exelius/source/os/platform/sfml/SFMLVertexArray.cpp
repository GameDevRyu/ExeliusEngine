#include "EXEPCH.h"
#include "SFMLVertexArray.h"
#include "source/os/interface/graphics/Vertex.h"

#include <SFML/Graphics/VertexArray.hpp>

/// <summary>
/// Engine namespace. Everything owned by the engine will be inside this namespace.
/// </summary>
namespace Exelius
{
	SFMLVertexArray::SFMLVertexArray(size_t vertexCount)
		: m_pSFMLVertexArray(new sf::VertexArray(sf::Quads, vertexCount))
	{
		EXE_ASSERT(m_pSFMLVertexArray);
	}

	SFMLVertexArray::~SFMLVertexArray()
	{
		delete m_pSFMLVertexArray;
		m_pSFMLVertexArray = nullptr;
	}

	size_t SFMLVertexArray::GetVertexCount() const
	{
		return m_pSFMLVertexArray->getVertexCount();
	}

	void SFMLVertexArray::Clear()
	{
		m_pSFMLVertexArray->clear();
	}

	void SFMLVertexArray::Resize(size_t vertexCount)
	{
		m_pSFMLVertexArray->resize(vertexCount);
	}

	void SFMLVertexArray::Append(const Vertex& vertex)
	{
		m_pSFMLVertexArray->append(vertex.GetNativeVertex().GetSFMLVertex());
	}
}
