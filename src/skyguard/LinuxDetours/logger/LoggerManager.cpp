#include <skyguard/LinuxDetours/logger/LoggerManager.h>
#include <algorithm>

SGLD_BEGIN

std::vector<log4cxx::LoggerPtr> LoggerManager::mLoggers =
    {
        log4cxx::Logger::getLogger("SysLog"),
    };

LoggerManager::~LoggerManager()
{
}

void LoggerManager::addLogger(const log4cxx::LoggerPtr logger)
{
    auto logger_it = std::find_if(mLoggers.begin(), mLoggers.end(),
            [&](const log4cxx::LoggerPtr saved_logger) { return saved_logger == logger;});
    if (logger_it == mLoggers.end())
        mLoggers.push_back(logger);
}

log4cxx::LoggerPtr LoggerManager::getLogger(const std::string& loggerName)
{
    auto logger = std::find_if(mLoggers.begin(), mLoggers.end(),
            [&](const log4cxx::LoggerPtr saved_logger) { return saved_logger->getName() == loggerName;});
    return logger != mLoggers.end() ? *logger : log4cxx::LoggerPtr();
}

log4cxx::LoggerPtr LoggerManager::getLogger(const std::string& loggerName, const std::string& tag)
{
    log4cxx::LoggerPtr found_logger = getLogger(loggerName);
    if (found_logger != log4cxx::LoggerPtr())
    {
        std::string new_tag = loggerName + "." + tag;
        return found_logger->getLogger(new_tag.c_str());
    }
    return found_logger;
}

void LoggerManager::removeLogger(const log4cxx::LoggerPtr logger)
{
    mLoggers.erase(std::remove(mLoggers.begin(), mLoggers.end(), logger), mLoggers.end());
}

SGLD_END
