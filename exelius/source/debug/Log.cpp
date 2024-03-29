#include "EXEPCH.h"

#include "source/debug/Log.h"
#include "source/debug/LogManager.h"

/// <summary>
/// Engine namespace. Everything owned by the engine will be inside this namespace.
/// </summary>
namespace Exelius
{
	/// <summary>
	/// Instantiate a log handle with an optional name.
	/// If no name is given, the default log will be retrieved.
	/// 
	/// The log will be retrieved from the LogManager here, and if the
	/// log does not exist, it will be created.
	/// </summary>
	/// <param name="logName">- The optional name of the log to instantiate. Default is "Exelius".</param>
	Log::Log(StringIntern logName /* = "Exelius" */)
		: m_logName(logName)
		, m_pLog(GetOrCreateLog())
	{
		EXE_ASSERT(m_logName.IsValid());
		EXE_ASSERT(m_pLog);
	}

	/// <summary>
	/// Obtains the log from the LogManager.
	/// If the log does not yet exist, it will create one.
	/// </summary>
	/// <returns>- A scoped pointer to the requested log. This should NEVER return nullptr.</returns>
	const std::shared_ptr<spdlog::logger> Log::GetOrCreateLog() const
	{
		EXE_ASSERT(m_logName.IsValid());
		auto pLogManager = LogManager::GetInstance();
		EXE_ASSERT(pLogManager);
		auto log = pLogManager->GetLog(m_logName);

		if (!log)
		{
			// If we get here, that means the log has not been created yet.
			// We create it here, and we will not likely hit this branch again
			// for this log name.
			// 
			// This is here because if a log is not defined prior to the LogManager
			// Initialization, via engine_config.ini, then we create a log with
			// default settings here as it means that the log is defined in code
			// somewhere.
			//
			// TODO: Maybe this entire branch should be a Error on release
			// builds, as we prefer the use of logs to be in the engine_config.ini.
			// SEE NOTE IN LogManager::GetLog in LogManager.cpp
			pLogManager->CreateLog(m_logName, LogLocation::kConsole, LogLevel::kTrace);
			log = pLogManager->GetLog(m_logName);
		}

		EXE_ASSERT(log);
		return log;
	}
}