#include "Common/Utils/FileUtils/FileUtils.h"

namespace PiTrac {

bool FileUtils::fileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath);
}

bool FileUtils::directoryExists(const std::string& dirPath) {
    return std::filesystem::is_directory(dirPath);
}

bool FileUtils::createDirectory(const std::string& dirPath) {
    return std::filesystem::create_directory(dirPath);
}

bool FileUtils::createFile(const std::string& dirPath, const std::string& filename) {
    std::ofstream file(dirPath + filename);
    file.close();
    return std::filesystem::exists(dirPath + filename);
}

bool FileUtils::removeFile(const std::string& filePath) {
    return std::filesystem::remove(filePath);
}

bool FileUtils::copyFile(const std::string& sourcePath, const std::string& destPath) {
    return std::filesystem::copy_file(sourcePath, destPath);
}

bool FileUtils::moveFile(const std::string& sourcePath, const std::string& destPath) {
    std::error_code ec;
    std::filesystem::rename(sourcePath, destPath, ec);
    return !ec;
}    

} // namespace PiTrac