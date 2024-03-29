#pragma once
#include "source/engine/gameobjectsystem/components/ComponentFactory.h"

/// <summary>
/// Engine namespace. Everything owned by the engine will be inside this namespace.
/// </summary>
namespace Exelius
{
	class ExeliusComponentFactory
		: public ComponentFactory
	{
	public:
		/// <summary>
		/// Constructor initialized the log.
		/// </summary>
		ExeliusComponentFactory();
		ExeliusComponentFactory(const ExeliusComponentFactory&) = delete;
		ExeliusComponentFactory(ExeliusComponentFactory&&) = delete;
		ExeliusComponentFactory& operator=(const ExeliusComponentFactory&) = delete;
		ExeliusComponentFactory& operator=(ExeliusComponentFactory&&) = delete;
		virtual ~ExeliusComponentFactory() = default;

		virtual bool Initialize() override;
		virtual Handle CreateComponent(const Component::Type& componentName, GameObject* pOwningObject, const rapidjson::Value& componentData) override;
	};
}