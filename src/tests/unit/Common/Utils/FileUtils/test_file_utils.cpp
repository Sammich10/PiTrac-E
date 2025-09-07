#include "Common/Utils/FileUtils/FileUtils.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

using namespace PiTrac;

class FileUtilsTest : public ::testing::Test
{
  protected:
    std::string tmpDir = "/tmp";
    std::string testDir = tmpDir + "/test_dir";
    std::string testDir2 = tmpDir + "/test_dir2";
    std::string testFile = "/test_file.txt";
    std::string testFileCopy = "/test_file_copy.txt";
    std::string testFileMove = "/test_file_move.txt";

    void SetUp() override
    {
        std::filesystem::remove_all(testDir);
        std::filesystem::remove_all(testDir2);
        std::filesystem::remove(testDir + testFile);
        std::filesystem::remove(testDir + testFileCopy);
        std::filesystem::remove(testDir + testFileMove);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(testDir);
        std::filesystem::remove(testDir + testFile);
        std::filesystem::remove(testDir + testFileCopy);
        std::filesystem::remove(testDir + testFileMove);
    }
};

TEST_F(FileUtilsTest, CreateDirectory) {
    EXPECT_TRUE(FileUtils::createDirectory(testDir));
    EXPECT_TRUE(std::filesystem::exists(testDir));
}

TEST_F(FileUtilsTest, FindDirectory){
    EXPECT_TRUE(FileUtils::createDirectory(testDir));
    EXPECT_TRUE(FileUtils::directoryExists(testDir));
}

TEST_F(FileUtilsTest, CreateFile) {
    EXPECT_TRUE(FileUtils::createDirectory(testDir));
    EXPECT_TRUE(FileUtils::createFile(testDir, testFile));
    EXPECT_TRUE(std::filesystem::exists(testDir + testFile));
}

TEST_F(FileUtilsTest, CopyFile) {
    EXPECT_TRUE(FileUtils::createDirectory(testDir));
    EXPECT_TRUE(FileUtils::createDirectory(testDir2));
    EXPECT_TRUE(FileUtils::createFile(testDir, testFile));
    EXPECT_TRUE(FileUtils::copyFile(testDir + testFile, testDir2 + testFileCopy));
    EXPECT_TRUE(std::filesystem::exists(testDir + testFile));
    EXPECT_TRUE(std::filesystem::exists(testDir2 + testFileCopy));
}

TEST_F(FileUtilsTest, MoveFile) {
    EXPECT_TRUE(FileUtils::createDirectory(testDir));
    EXPECT_TRUE(FileUtils::createDirectory(testDir2));
    EXPECT_TRUE(FileUtils::createFile(testDir, testFile));
    EXPECT_TRUE(FileUtils::moveFile(testDir + testFile, testDir2 + testFileMove));
    EXPECT_TRUE(std::filesystem::exists(testDir2 + testFileMove));
    EXPECT_FALSE(std::filesystem::exists(testDir + testFile));
}

TEST_F(FileUtilsTest, DeleteFile) {
    EXPECT_TRUE(FileUtils::createDirectory(testDir));
    EXPECT_TRUE(FileUtils::createFile(testDir, testFile));
    EXPECT_TRUE(FileUtils::removeFile(testDir + testFile));
    EXPECT_FALSE(std::filesystem::exists(testDir + testFile));
}
