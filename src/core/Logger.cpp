#include "core/Logger.h"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace obfuscator {

Logger::Logger() {
}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogFile(const std::string& filePath) {
    std::ofstream newFile;
    newFile.open(filePath, std::ios::app);
    if (newFile.is_open()) {
        // 新文件打开成功，替换旧文件
        if (logFile_.is_open()) {
            logFile_.close();
        }
        logFile_.swap(newFile);
        useFile_ = true;
    } else {
        // 新文件打开失败，保持旧文件不变
        LOG_ERROR("Failed to open log file: " + filePath);
        if (!logFile_.is_open()) {
            useFile_ = false;
        }
    }
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel_ = level;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel_) {
        return;
    }
    
    // 获取当前时间
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    std::string timestamp = oss.str();
    
    std::string logMessage = "[" + timestamp + "] [" + levelToString(level) + "] " + message;
    
    // 输出到控制台
    if (level >= LogLevel::WARNING) {
        std::cerr << logMessage << std::endl;
    } else {
        std::cout << logMessage << std::endl;
    }
    
    // 输出到文件
    if (useFile_ && logFile_.is_open()) {
        logFile_ << logMessage << std::endl;
        logFile_.flush();
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

} // namespace obfuscator

