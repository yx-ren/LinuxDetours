#include <iostream>
#include <memory>
#include <skyguard/LinuxDetours/logger/Logger.h>
#include <skyguard/LinuxDetours/logger/LoggerManager.h>

using namespace SGLD_NAMESPACE;

#if 1
#define CUSTOMT_TRACE(fmt) CBT_TRACE("custom_tag", fmt)
#define CUSTOMT_DEBUG(fmt) CBT_DEBUG("custom_tag", fmt)
#define CUSTOMT_INFO(fmt) CBT_INFO("custom_tag", fmt)
#define CUSTOMT_WARN(fmt) CBT_WARN("custom_tag", fmt)
#define CUSTOMT_ERROR(fmt) CBT_ERROR("custom_tag", fmt)

#define LOG_TRAFFIC_TRACE(fmt) LOG4CXX_TRACE(SGLD_NAMESPACE::LoggerManager::getLogger("traffic"), fmt)
#define LOG_TRAFFIC_DEBUG(fmt) LOG4CXX_DEBUG(SGLD_NAMESPACE::LoggerManager::getLogger("traffic"), fmt)
#define LOG_TRAFFIC_INFO(fmt) LOG4CXX_INFO(SGLD_NAMESPACE::LoggerManager::getLogger("traffic"), fmt)
#define LOG_TRAFFIC_WARN(fmt) LOG4CXX_WARN(SGLD_NAMESPACE::LoggerManager::getLogger("traffic"), fmt)
#define LOG_TRAFFIC_ERROR(fmt) LOG4CXX_ERROR(SGLD_NAMESPACE::LoggerManager::getLogger("traffic"), fmt)
#define LOG_TRAFFIC_FATAL(fmt) LOG4CXX_FATAL(SGLD_NAMESPACE::LoggerManager::getLogger("traffic"), fmt)

#define LOG_ACCESS_TRACE(fmt) LOG4CXX_TRACE(SGLD_NAMESPACE::LoggerManager::getLogger("access"), fmt)
#define LOGT_ACCESS_TRACE(tag, fmt) do {\
        LOG4CXX_TRACE(SGLD_NAMESPACE::LoggerManager::getLogger("access", tag), fmt);\
     } while (0)

#define LOG_ACCESS_DEBUG(fmt) LOG4CXX_DEBUG(SGLD_NAMESPACE::LoggerManager::getLogger("access"), fmt)
#define LOGT_ACCESS_DEBUG(tag, fmt) do {\
        LOG4CXX_DEBUG(SGLD_NAMESPACE::LoggerManager::getLogger("access", tag), fmt);\
     } while (0)

#define LOG_ACCESS_INFO(fmt) LOG4CXX_INFO(SGLD_NAMESPACE::LoggerManager::getLogger("access"), fmt)
#define LOGT_ACCESS_INFO(tag, fmt) do {\
        LOG4CXX_INFO(SGLD_NAMESPACE::LoggerManager::getLogger("access", tag), fmt);\
     } while (0)

#define LOG_ACCESS_WARN(fmt) LOG4CXX_WARN(SGLD_NAMESPACE::LoggerManager::getLogger("access"), fmt)
#define LOGT_ACCESS_WARN(tag, fmt) do {\
        LOG4CXX_WARN(SGLD_NAMESPACE::LoggerManager::getLogger("access", tag), fmt);\
     } while (0)

#define LOG_ACCESS_ERROR(fmt) LOG4CXX_ERROR(SGLD_NAMESPACE::LoggerManager::getLogger("access"), fmt)
#define LOGT_ACCESS_ERROR(tag, fmt) do {\
        LOG4CXX_ERROR(SGLD_NAMESPACE::LoggerManager::getLogger("access", tag), fmt);\
     } while (0)

#define LOG_ACCESS_FATAL(fmt) LOG4CXX_FATAL(SGLD_NAMESPACE::LoggerManager::getLogger("access"), fmt)
#define LOGT_ACCESS_FATAL(tag, fmt) do {\
        LOG4CXX_FATAL(SGLD_NAMESPACE::LoggerManager::getLogger("access", tag), fmt);\
     } while (0)
#endif

int main(int argc, const char* argv[])
{
#if 1
    LoggerParameter logger_traffic_param;
    logger_traffic_param.module_tag = "traffic";
    logger_traffic_param.file_path = "/var/log/traffic";
    LoggerSPtr logger_traffic = std::make_shared<Logger>();
    logger_traffic->init(logger_traffic_param);

    LoggerParameter logger_access_param;
    logger_access_param.module_tag = "access";
    logger_access_param.file_path = "/var/log/access";
    LoggerSPtr logger_access = std::make_shared<Logger>();
    logger_access->init(logger_access_param);

    LoggerParameter logger_SysLog_param;
    logger_SysLog_param.module_tag = "SysLog";
    logger_SysLog_param.file_path = "/var/log/SysLog";
    logger_SysLog_param.level = "TRACE";
    LoggerSPtr logger_SysLog = std::make_shared<Logger>();
    logger_SysLog->init(logger_SysLog_param);

    LoggerManager::addLogger(logger_traffic->getRawLogger());
    LoggerManager::addLogger(logger_access->getRawLogger());
    LoggerManager::addLogger(logger_SysLog->getRawLogger());
#endif

#if 1
    for (int i = 0; i != 5; i++)
    {
        LOG_TRAFFIC_INFO("this is traffic logger");

        LOG_ACCESS_INFO("this is access logger");
        LOGT_ACCESS_INFO("accsss_tag_1", "this is access logger");
        LOGT_ACCESS_INFO("accsss_tag_2", "this is access logger");

        CB_TRACE("TRACE, this log generate by LoggerManager");
        CBT_TRACE("sys_tag", "[TRACE] level, this log generate by LoggerManager");

        CB_DEBUG("DEBUG, this log generate by LoggerManager");
        CBT_DEBUG("sys_tag", "[DEBUG] level, this log generate by LoggerManager");

        CB_INFO("INFO, this log generate by LoggerManager");
        CBT_INFO("sys_tag", "[INFO] level, this log generate by LoggerManager");

        CB_WARN("WARN, this log generate by LoggerManager");
        CBT_WARN("sys_tag", "[WARN] level, this log generate by LoggerManager");

        CB_ERROR("ERROR, this log generate by LoggerManager");
        CBT_ERROR("sys_tag", "[ERROR] level, this log generate by LoggerManager");

        CUSTOMT_INFO("INFO, syslogger warp a custom tag");
    }
#endif

    return 0;
}
