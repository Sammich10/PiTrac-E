#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

namespace PiTrac
{
class FileUtils
{
/**
 * @class FileUtils
 * @brief Utility class for common file and directory operations.
 *
 * Provides static methods for file and directory manipulation.
 */
  public:

/**
 * @brief Checks if a file exists at the specified path.
 *
 * @param filePath The path to the file to check.
 * @return true if the file exists, false otherwise.
 */
    static bool fileExists
    (
        const std::string &filePath
    );

/**
 * @brief Checks if a directory exists at the specified path.
 *
 * @param dirPath The path to the directory to check.
 * @return true if the directory exists, false otherwise.
 */
    static bool directoryExists
    (
        const std::string &dirPath
    );

/**
 * @brief Creates a directory at the specified path.n.
 *
 * @param dirPath The path of the directory to create.
 * @return true if the directory was successfully created or already exists,
 * false otherwise.
 */
    static bool createDirectory
    (
        const std::string &dirPath
    );

/**
 * @brief Creates a file at the specified path.
 *
 * @param dirPath The directory in which to create the file.
 * @param filename The name of the file to create.
 *
 * @return true if the file was successfully created, false otherwise.
 */
    static bool createFile
    (
        const std::string &dirPath,
        const std::string &filename
    );

/**
 * @brief Attempts to delete the file located at the given filePath.
 *
 * @param filePath The path to the file to be removed.
 * @return true if the file was successfully removed, false otherwise.
 */
    static bool removeFile
    (
        const std::string &filePath
    );

/**
 * @brief Copies a file from the source path to the destination path.
 *
 * @param sourcePath The path to the source file to be copied.
 * @param destPath The path where the copied file should be placed.
 * @return true if the file was copied successfully, false otherwise.
 */
    static bool copyFile
    (
        const std::string &sourcePath,
        const std::string &destPath
    );


/**
 * @brief Moves a file from the source path to the destination path.
 *
 * @param sourcePath The path to the file to be moved.
 * @param destPath The destination path where the file should be moved.
 * @return true if the file was moved successfully, false otherwise.
 */
    static bool moveFile
    (
        const std::string &sourcePath,
        const std::string &destPath
    );

/**
 * @brief Finds an executable in the system PATH.
 *
 * @param executableName The name of the executable to find (e.g., "ls", "gcc").
 * @return The full path to the executable if found, empty string otherwise.
 */
    static std::string findExecutableInPath
    (
        const std::string &executableName
    );

/**
 * @brief Checks if an executable exists, with PATH resolution.
 *
 * If the executable name contains a path separator, checks that specific path.
 * Otherwise, searches in the system PATH.
 *
 * @param executable The executable name or path to check.
 * @return true if the executable exists and is executable, false otherwise.
 */
    static bool executableExists
    (
        const std::string &executable
    );

/**
 * @brief Resolves an executable to its full path.
 *
 * If the executable is already an absolute path, returns it as-is (if it
 * exists).
 * If it's a relative path or just a name, searches in PATH.
 *
 * @param executable The executable name or path to resolve.
 * @return The full path to the executable if found, empty string otherwise.
 */
    static std::string resolveExecutablePath
    (
        const std::string &executable
    );

/**
 * @brief Gets all directories in the system PATH environment variable.
 *
 * @return Vector of directory paths from the PATH environment variable.
 */
    static std::vector<std::string> getPathDirectories();

    static std::string getDirectoryFromPath
    (
        const std::string &filePath
    );

    static bool readFileToString
    (
        const std::string &filePath,
        std::string &outContent
    );

  private:
/**
 * @brief Checks if a file is executable.
 *
 * @param filePath The path to the file to check.
 * @return true if the file exists and is executable, false otherwise.
 */
    static bool isExecutable
    (
        const std::string &filePath
    );
};
} // namespace PiTrac

#endif // FILE_UTILS_H