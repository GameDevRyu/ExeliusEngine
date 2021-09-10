#include "EXEPCH.h"
#include "Source/Engine/GameObjectSystem/GameObject.h"

#include "Source/Resource/ResourceHandle.h"
#include "Source/Engine/Resources/ResourceTypes/TextFileResource.h"

/// <summary>
/// Engine namespace. Everything owned by the engine will be inside this namespace.
/// </summary>
namespace Exelius
{
	/// <summary>
	/// Constructor - Initializes member variables.
	/// TODO: This should not be publicly accessable!
	/// </summary>
	/// <param name="id">The unique object ID given by the GameObjectSystem.</param>
	GameObject::GameObject(uint32_t id)
		: m_id(id)
		, m_enabled(true)
		, m_name("Invalid")
	{
		EXE_ASSERT(m_id != GameObjectSystem::kInvalidID);
	}

	/// <summary>
	/// Initialize this object using a JSON text file.
	/// This will set any values specified in the file,
	/// and will create (or get from the pool) any necessary
	/// components and intialize those.
	/// </summary>
	/// <param name="pResource">The JSON TextFile resource containing object data.</param>
	/// <returns>True on success, false on failure.</returns>
	bool GameObject::Initialize(const eastl::string& pRawText)
	{
		Log log("GameObjectSystem");

		rapidjson::Document jsonDoc;
		if (jsonDoc.Parse(pRawText.c_str()).HasParseError())
		{
			log.Error("Failed to Parse JSON.");
			return false;
		}

		EXE_ASSERT(jsonDoc.IsObject());

		// Set any game object specific values.
		auto nameMember = jsonDoc.FindMember("Name");

		if (nameMember != jsonDoc.MemberEnd())
		{
			EXE_ASSERT(nameMember->value.IsString());
			m_name = nameMember->value.GetString();
		}
		else
		{
			log.Error("No 'Name' field found. Setting object name to a default value.");
			// Name the object based on it's ID.
			m_name = "New GameObject (";
			//m_name += eastl::to_string(m_id);														<-- Why does this cause errors?!!
			m_name += ")";
		}

		// Create and Initialize any Components.
		ParseComponentArray(jsonDoc);

		return true;
	}

	/// <summary>
	/// Removes all the components attached to this GameObject.
	/// This returns them to their respective pools.
	/// </summary>
	void GameObject::RemoveComponents()
	{
		auto* pGameObjectSystem = GameObjectSystem::GetInstance();
		EXE_ASSERT(pGameObjectSystem);

		for (auto& componentPair : m_components)
		{
			EXE_ASSERT(componentPair.first.IsValid());
			EXE_ASSERT(componentPair.second.IsValid());

			pGameObjectSystem->ReleaseComponent(componentPair.first, componentPair.second);
		}

		m_components.clear();
	}

	/// <summary>
	/// This function is called when a game object file has successfully loaded.
	/// This means we should have the data we need to initialize the object,
	/// so we pass the data to the intialize function.
	/// </summary>
	/// <param name="resourceID">- The ID of the loaded resource.</param>
	/// <returns>True if the resource was flushed here, false if not.</returns>
	bool GameObject::OnResourceLoaded(const ResourceID& resourceID)
	{
		Log log("GameObjectSystem");

		EXE_ASSERT(resourceID.IsValid());
		ResourceHandle textFileResource(resourceID);

		//EXE_ASSERT(textFileResource.IsReferenceHeld()); // This is literally what we are being informed about!

		if (!Initialize(textFileResource.GetAs<TextFileResource>()->GetRawText()))
		{
			log.Error("GameObject failed to initialize.");
		}

		// TODO:
		//	In MOST cases.. the ref count is currently 2 for the gameobject file.
		//	This is due to the initial creation of the resource when CreateGameObject is called,
		//	and the above GetAs call. (Both increment the refcount) So here there would need to
		//	be a call to Release (A second call is made when textFileResource falls out of scope).
		//	The fix would be to change how GameObject creation happens (Maybe on gameobject file load?)
		//textFileResource.Release();

		// We unlock here because the resource was locked for the duration of the load.
		// Now we no longer need it.
		textFileResource.UnlockResource();

		return true;
	}

	/// <summary>
	/// Parses and creates the components in the JSON object document.
	/// This will create (or get from the pool), attach, and initialize these components.
	/// </summary>
	/// <param name="jsonDoc">The JSON document with the Object data.</param>
	void GameObject::ParseComponentArray(const rapidjson::Document& jsonDoc)
	{
		auto* pGameObjectSystem = GameObjectSystem::GetInstance();
		EXE_ASSERT(pGameObjectSystem);
		Log log("GameObjectSystem");

		// Find an Array with name 'Components'
		auto componentArrayMember = jsonDoc.FindMember("Components");

		// If the Array does not exist then bail.
		if (componentArrayMember == jsonDoc.MemberEnd())
		{
			log.Info("No 'Components' field found.");
			return;
		}

		auto& componentArrayValue = componentArrayMember->value;

		// Make sure it's value is an Object (List of).
		EXE_ASSERT(componentArrayValue.IsObject());

		// For each Component in the Components List.
		for (auto componentMember = componentArrayValue.MemberBegin(); componentMember != componentArrayValue.MemberEnd(); ++componentMember)
		{
			Handle newComponentHandle = pGameObjectSystem->CreateComponentFromFactory(componentMember->name.GetString(), this, componentMember->value);
			if (!newComponentHandle.IsValid())
				continue;

			m_components.try_emplace(componentMember->name.GetString(), newComponentHandle);
		}
	}
}