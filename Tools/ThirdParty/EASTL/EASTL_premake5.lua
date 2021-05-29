printf("Including EASTL_premake5.lua - This should only appear once.")
------------------------------------------------------------------------------------------------------
--[[
    Exelius Game Engine - Game Development Tool
    Copyright (c) 2020 Exelius Entertainment
    Created By: Erin "Ryukana" Grover

    This Engine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    License for more details. You should have received a copy of the
    License along with this Engine.
--]]
------------------------------------------------------------------------------------------------------

local dependencies = require("Tools/ThirdParty/DependencyManager")

local eastl = 
{
	location = "EASTL"
}

function eastl.Include(rootDir)
	includedirs
	{
		rootDir .. [[include/]]
	}

	defines 
    {
        "_CRT_SECURE_NO_WARNINGS"
    }

	--------------------------------------------------------------------------------------------------
	-- Windows Library Directories
	--------------------------------------------------------------------------------------------------

	filter{"platforms:x86", "configurations:Debug", "system:windows"}
		libdirs
		{
			rootDir .. [[lib/Win32/Debug/]],
		}

	filter{"platforms:x86", "configurations:Test", "system:windows"}
		libdirs
		{
			rootDir .. [[lib/Win32/RelWithDebInfo/]]
		}

	filter{"platforms:x86", "configurations:Release", "system:windows"}
		libdirs
		{
			rootDir .. [[lib/Win32/MinSizeRel/]]
		}

	filter{"platforms:x64", "configurations:Debug", "system:windows"}
		libdirs
		{
			rootDir .. [[lib/Win64/Debug/]],
		}

	filter{"platforms:x64", "configurations:Test", "system:windows"}
		libdirs
		{
			rootDir .. [[lib/Win64/RelWithDebInfo/]]
		}

	filter{"platforms:x64", "configurations:Release", "system:windows"}
		libdirs
		{
			rootDir .. [[lib/Win64/MinSizeRel/]]
		}
		
	--------------------------------------------------------------------------------------------------
	-- Linux Library Directories
	--------------------------------------------------------------------------------------------------

	filter{"platforms:x86", "configurations:Debug", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Lin32/Debug/]],
		}

	filter{"platforms:x86", "configurations:Test", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Lin32/Release/]]
		}

	filter{"platforms:x86", "configurations:Release", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Lin32/Release/]]
		}

	filter{"platforms:x64", "configurations:Debug", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Lin64/Debug/]],
		}

	filter{"platforms:x64", "configurations:Test", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Lin64/Release/]]
		}

	filter{"platforms:x64", "configurations:Release", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Lin64/Release/]]
		}

	--------------------------------------------------------------------------------------------------
	-- Raspberry Pi Library Directories
	--------------------------------------------------------------------------------------------------

	filter{"platforms:rpi86", "configurations:Debug", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Rpi32/Debug/]],
		}

	filter{"platforms:rpi86", "configurations:Test", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Rpi32/Release/]]
		}

	filter{"platforms:rpi86", "configurations:Release", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Rpi32/Release/]]
		}

	filter{"platforms:rpi64", "configurations:Debug", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Rpi64/Debug/]],
		}

	filter{"platforms:rpi64", "configurations:Test", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Rpi64/Release/]]
		}

	filter{"platforms:rpi64", "configurations:Release", "system:linux"}
		libdirs
		{
			rootDir .. [[lib/Rpi64/Release/]]
		}

	filter{}
end

function eastl.Link(rootdir, exeliusLibDir)

	filter{"system:windows"}
		links
		{
			"EASTL"
		}

	filter{"system:linux"}
		links
		{
			"EASTL"
		}

	filter {}
end

dependencies.Add("EASTL", eastl)