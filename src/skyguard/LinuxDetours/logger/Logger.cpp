#include <skyguard/LinuxDetours/logger/Logger.h>
#include <log4cxx/helpers/properties.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/fileappender.h>
#include <log4cxx/level.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/patternlayout.h>

SGLD_BEGIN

void Logger::init(const LoggerParameter& param)
{
    using namespace log4cxx;
    using namespace log4cxx::helpers;

    Properties props;
    std::string log_value = param.level;
    std::string log_pattern = "%d [%t] <" + param.processor_tag + ">: %-5p %.32c - %m%n";
    std::string log_file = param.file_path;

    // init file
    if (param.file_flag)
    {
        log_value += ", file";
        props.put("log4j.appender.file", "org.apache.log4j.RollingFileAppender");
        props.put("log4j.appender.file.layout", "org.apache.log4j.Patternlayout");
        props.put("log4j.appender.file.layout.ConversionPattern", log_pattern.c_str());
        props.put("log4j.appender.file.File", log_file.c_str());
        props.put("log4j.appender.file.MaxFileSize", "10MB");
        props.put("log4j.appender.file.MaxBackupIndex", "10");
    }

    // init console
    if (param.console_flag)
    {
        log_value += ", console";
        props.put("log4j.appender.console", "org.apache.log4j.ConsoleAppender");
        props.put("log4j.appender.console.layout", "org.apache.log4j.PatternLayout");
        props.put("log4j.appender.console.layout.ConversionPattern", log_pattern.c_str());
    }

    props.put("log4j.logger." + param.module_tag, log_value.c_str());
    PropertyConfigurator::configure(props);

    mLogger = log4cxx::Logger::getLogger(param.module_tag);
    mParam = param;
}

void Logger::finalize()
{
    if (mLogger)
        mLogger->removeAllAppenders();
}

void Logger::updateLevel(const std::string& level)
{
    mLogger->setLevel(log4cxx::Level::toLevelLS(level));
}

log4cxx::LoggerPtr Logger::getRawLogger() const
{
    return mLogger;
}

log4cxx::LoggerPtr Logger::getRawLogger(const std::string& tag) const
{
    std::string new_tag = mParam.module_tag + "." + tag;
    return mLogger->getLogger(new_tag.c_str());
}

SGLD_END
