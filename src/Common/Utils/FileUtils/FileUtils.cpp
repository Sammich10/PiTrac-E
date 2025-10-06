#include "Common/Utils/FileUtils/FileUtils.h"
#include <cstdlib>
#include <unistd.h>

namespace PiTrac
{
bool FileUtils::fileExists(const std::string &filePath)
{
    return std::filesystem::exists(filePath);
}

bool FileUtils::directoryExists(const std::string &dirPath)
{
    return std::filesystem::is_directory(dirPath);
}

bool FileUtils::createDirectory(const std::string &dirPath)
{
    return std::filesystem::create_directory(dirPath);
}

bool FileUtils::createFile(const std::string &dirPath, const std::string &filename)
{
    std::ofstream file(dirPath + filename);
    file.close();
    return std::filesystem::exists(dirPath + filename);
}

bool FileUtils::removeFile(const std::string &filePath)
{
    return std::filesystem::remove(filePath);
}

bool FileUtils::copyFile(const std::string &sourcePath, const std::string &destPath)
{
    return std::filesystem::copy_file(sourcePath, destPath);
}

bool FileUtils::moveFile(const std::string &sourcePath, const std::string &destPath)
{
    std::error_code ec;
    std::filesystem::rename(sourcePath, destPath, ec);
    return !ec;
}

std::string FileUtils::findExecutableInPath(const std::string &executableName)
{
    // Get PATH directories
    std::vector<std::string> pathDirs = getPathDirectories();

    // Search each directory in PATH
    for (const auto &dir : pathDirs)
    {
        std::filesystem::path execPath = std::filesystem::path(dir) / executableName;

        if (isExecutable(execPath.string()))
        {
            return execPath.string();
        }
    }

    return ""; // Not found
}

bool FileUtils::executableExists(const std::string &executable)
{
    // If it contains a path separator, check the specific path
    if (executable.find('/') != std::string::npos || executable.find('\\') != std::string::npos)
    {
        return isExecutable(executable);
    }

    // Otherwise, search in PATH
    return !findExecutableInPath(executable).empty();
}

std::string FileUtils::resolveExecutablePath(const std::string &executable)
{
    // If it's already an absolute path, check if it exists
    if (std::filesystem::path(executable).is_absolute())
    {
        return isExecutable(executable) ? executable : "";
    }

    // If it contains a path separator (relative path), check relative to
    // current directory
    if (executable.find('/') != std::string::npos || executable.find('\\') != std::string::npos)
    {
        std::filesystem::path absPath = std::filesystem::absolute(executable);
        return isExecutable(absPath.string()) ? absPath.string() : "";
    }

    // Otherwise, search in PATH
    return findExecutableInPath(executable);
}

std::vector<std::string> FileUtils::getPathDirectories()
{
    std::vector<std::string> directories;

    const char *pathEnv = std::getenv("PATH");
    if (!pathEnv)
    {
        return directories; // Empty if PATH not set
    }

    std::string pathStr(pathEnv);
    size_t start = 0;
    size_t end = 0;

    // Split PATH by colon
    char delimiter = ':';

    while ((end = pathStr.find(delimiter, start)) != std::string::npos)
    {
        if (end != start)   // Skip empty entries
        {
            directories.push_back(pathStr.substr(start, end - start));
        }
        start = end + 1;
    }

    // Add the last directory (after the last delimiter)
    if (start < pathStr.length())
    {
        directories.push_back(pathStr.substr(start));
    }

    return directories;
}

bool FileUtils::isExecutable(const std::string &filePath)
{
    if (!std::filesystem::exists(filePath))
    {
        return false;
    }

    // Check if it's a regular file
    if (!std::filesystem::is_regular_file(filePath))
    {
        return false;
    }

    // Check execute permissions (Unix/Linux)
#ifndef _WIN32
    return (access(filePath.c_str(), X_OK) == 0);
#else
    // On Windows, check file extension or just return true if file exists
    // since Windows doesn't use Unix-style execute permissions
    return true;
#endif
}
} // namespace PiTrac