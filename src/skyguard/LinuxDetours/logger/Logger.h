#ifndef __COMMON_BASE_LOGGER_LOGGER_H__
#define __COMMON_BASE_LOGGER_LOGGER_H__

#include <memory>
#include <log4cxx/logger.h>
#include <skyguard/LinuxDetours/common.h>

SGLD_BEGIN

struct LoggerParameter
{
    LoggerParameter()
    {
        processor_tag = "sys";
        module_tag = "SysLog";
        level = "DEBUG";
        console_flag = true;
        file_flag = true;
        file_path = "/var/log/cblog";
        file_size = "1024000";
        file_backup = "10";
    }
    std::string processor_tag;
    std::string module_tag;
    std::string level;
    bool console_flag;
    bool file_flag;
    std::string file_path;
    std::string file_size;
    std::string file_backup;
};
typedef std::shared_ptr<LoggerParameter> LoggerParameterSPtr;

class Logger
{
public:
    void init(const LoggerParameter& param);

    void finalize();

    void updateLevel(const std::string& level);

    log4cxx::LoggerPtr getRawLogger() const;

    log4cxx::LoggerPtr getRawLogger(const std::string& tag) const;

private:
    log4cxx::LoggerPtr mLogger;
    LoggerParameter mParam;
};
typedef std::shared_ptr<Logger> LoggerSPtr;

SGLD_END

#endif
