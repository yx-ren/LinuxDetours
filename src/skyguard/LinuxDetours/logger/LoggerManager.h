#ifndef __COMMON_BASE_LOGGER_MANAGER_LOGGER_H__
#define __COMMON_BASE_LOGGER_MANAGER_LOGGER_H__

#include <memory>
#include <vector>
#include <log4cxx/logger.h>
#include <skyguard/LinuxDetours/common.h>


#define CB_TRACE(fmt) CB_LOG_SYS_TRACE(fmt)
#define CB_DEBUG(fmt) CB_LOG_SYS_DEBUG(fmt)
#define CB_INFO(fmt) CB_LOG_SYS_INFO(fmt)
#define CB_WARN(fmt) CB_LOG_SYS_WARN(fmt)
#define CB_ERROR(fmt) CB_LOG_SYS_ERROR(fmt)
#define CB_FATAL(fmt) CB_LOG_SYS_FATAL(fmt)

#define CBT_TRACE(tag, fmt) CB_LOGT_SYS_TRACE(tag, fmt)
#define CBT_DEBUG(tag, fmt) CB_LOGT_SYS_DEBUG(tag, fmt)
#define CBT_INFO(tag, fmt) CB_LOGT_SYS_INFO(tag, fmt)
#define CBT_WARN(tag, fmt) CB_LOGT_SYS_WARN(tag, fmt)
#define CBT_ERROR(tag, fmt) CB_LOGT_SYS_ERROR(tag, fmt)
#define CBT_FATAL(tag, fmt) CB_LOGT_SYS_FATAL(tag, fmt)

#define CB_LOG_SYS_TRACE(fmt) LOG4CXX_TRACE(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog"), fmt)
#define CB_LOGT_SYS_TRACE(tag, fmt) do {\
        LOG4CXX_TRACE(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog", tag), fmt);\
     } while (0)

#define CB_LOG_SYS_DEBUG(fmt) LOG4CXX_DEBUG(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog"), fmt)
#define CB_LOGT_SYS_DEBUG(tag, fmt) do {\
        LOG4CXX_DEBUG(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog", tag), fmt);\
     } while (0)

#define CB_LOG_SYS_INFO(fmt) LOG4CXX_INFO(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog"), fmt)
#define CB_LOGT_SYS_INFO(tag, fmt) do {\
        LOG4CXX_INFO(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog", tag), fmt);\
     } while (0)

#define CB_LOG_SYS_WARN(fmt) LOG4CXX_WARN(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog"), fmt)
#define CB_LOGT_SYS_WARN(tag, fmt) do {\
        LOG4CXX_WARN(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog", tag), fmt);\
     } while (0)

#define CB_LOG_SYS_ERROR(fmt) LOG4CXX_ERROR(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog"), fmt)
#define CB_LOGT_SYS_ERROR(tag, fmt) do {\
        LOG4CXX_ERROR(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog", tag), fmt);\
     } while (0)

#define CB_LOG_SYS_FATAL(fmt) LOG4CXX_FATAL(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog"), fmt)
#define CB_LOGT_SYS_FATAL(tag, fmt) do {\
        LOG4CXX_FATAL(SGLD_NAMESPACE::LoggerManager::getLogger("SysLog", tag), fmt);\
     } while (0)

SGLD_BEGIN

class LoggerManager
{
public:
    virtual ~LoggerManager();

    static void addLogger(const log4cxx::LoggerPtr logger);

    static log4cxx::LoggerPtr getLogger(const std::string& loggerName);
    static log4cxx::LoggerPtr getLogger(const std::string& loggerName, const std::string& tag);

    static log4cxx::LoggerPtr getLoggers();

    static void removeLogger(const log4cxx::LoggerPtr logger);

private:
    static std::vector<log4cxx::LoggerPtr> mLoggers;
};

SGLD_END

#endif
