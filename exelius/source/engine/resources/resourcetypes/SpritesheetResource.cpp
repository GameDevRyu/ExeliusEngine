#include "EXEPCH.h"
#include "SpritesheetResource.h"

#include "source/resource/ResourceHandle.h"

#include <rapidjson/document.h>

/// <summary>
/// Engine namespace. Everything owned by the engine will be inside this namespace.
/// </summary>
namespace Exelius
{
    SpritesheetResource::SpritesheetResource(const ResourceID& id)
        : Resource(id)
    {
        //
    }

    Resource::LoadResult SpritesheetResource::Load(eastl::vector<std::byte>&& data)
    {
        // Set the raw byte data to a string value.
        m_text = eastl::string((const char*)data.begin(), (const char*)data.end());
        if (m_text.empty())
        {
            m_resourceManagerLog.Warn("Failed to read data in Spritesheet Resource.");
            return LoadResult::kFailed;
        }

        // Parse the text as JSON data.
        rapidjson::Document jsonDoc;
        if (jsonDoc.Parse(m_text.c_str()).HasParseError())
        {
            m_resourceManagerLog.Error("Failed to Parse JSON.");
            return LoadResult::kFailed;
        }

        EXE_ASSERT(jsonDoc.IsObject());

        // Find and load the Texture linked to this spritesheet.
        auto textureMember = jsonDoc.FindMember("Texture");

        if (textureMember == jsonDoc.MemberEnd())
        {
            m_resourceManagerLog.Warn("No Texture field found. Spritesheets must have a texture.");
            return LoadResult::kFailed;
        }

        // Set and queue the texture for loading.
        EXE_ASSERT(textureMember->value.IsString());
        m_textureResourceID = textureMember->value.GetString();
        EXE_ASSERT(m_textureResourceID.IsValid());

        ResourceHandle textureResource(m_textureResourceID);
        //EXE_ASSERT(textureResource.IsReferenceHeld());

        // TODO:
        //  This might be more efficient if this is false, so that all loading can be queued together.
        //  Although, this will need to be nicely managed by the user in some way... otherwise,
        //  the resource thread will never know if the queue is ready to process. Something to think about...
        textureResource.QueueLoad(true);
        textureResource.LockResource();

        // Gather the data for all the sprites.
        auto spriteMember = jsonDoc.FindMember("Sprites");

        if (spriteMember == jsonDoc.MemberEnd())
        {
            m_resourceManagerLog.Warn("No Sprite field found. Spritesheets must have at least 1 Sprite. TODO: Maybe no sprite means default value?");
            return LoadResult::kFailed;
        }
        else if (spriteMember != jsonDoc.MemberEnd())
        {

            EXE_ASSERT(spriteMember->name.IsString());
            EXE_ASSERT(spriteMember->value.IsObject());
            bool containsSpriteData = false;

            for (auto spriteItr = spriteMember->value.MemberBegin(); spriteItr != spriteMember->value.MemberEnd(); ++spriteItr)
            {
                containsSpriteData = true;

                // Construct the sprite.
                EXE_ASSERT(spriteItr->name.IsString());
                EXE_ASSERT(spriteItr->value.IsObject());

                FRectangle builtRect;

                auto xMember = spriteItr->value.FindMember("sourceX");
                auto yMember = spriteItr->value.FindMember("sourceY");
                auto wMember = spriteItr->value.FindMember("sourceW");
                auto hMember = spriteItr->value.FindMember("sourceH");

                // TODO: More error checking.
                if (xMember != spriteItr->value.MemberEnd())
                    builtRect.m_left = xMember->value.GetFloat();
                if (yMember != spriteItr->value.MemberEnd())
                    builtRect.m_top = yMember->value.GetFloat();
                if (wMember != spriteItr->value.MemberEnd())
                    builtRect.m_width = wMember->value.GetFloat();
                if (hMember != spriteItr->value.MemberEnd())
                    builtRect.m_height = hMember->value.GetFloat();

                m_sprites.try_emplace(spriteItr->name.GetString(), builtRect);
            }

            if (!containsSpriteData)
            {
                m_resourceManagerLog.Warn("No Sprite field found. Spritesheets must have at least 1 Sprite. TODO: Maybe no sprite means default value?");
                return LoadResult::kFailed;
            }
        }

        return LoadResult::kKeptRawData;
    }

    void SpritesheetResource::Unload()
    {
        ResourceHandle textureResource(m_textureResourceID);
        textureResource.UnlockResource();
    }
}