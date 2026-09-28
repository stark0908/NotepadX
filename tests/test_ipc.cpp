#include <gtest/gtest.h>
#include "ipc/SingleInstance.h"

#include <filesystem>
#include <thread>
#include <vector>

using namespace notepadx;

class SingleInstanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        testSocketPath_ = std::filesystem::temp_directory_path() / ("notepadx_test_ipc_" + std::to_string(getpid()) + ".sock");
        std::error_code ec;
        std::filesystem::remove(testSocketPath_, ec);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(testSocketPath_, ec);
    }

    std::filesystem::path testSocketPath_;
};

TEST_F(SingleInstanceTest, SerializationRoundTrip) {
    const std::vector<std::string> files = {
        "/home/user/document.txt",
        "/tmp/space in filename.cpp",
        "relative/path/test.h"
    };

    const std::string payload = SingleInstance::serializePayload(files);
    EXPECT_FALSE(payload.empty());

    const std::vector<std::string> roundTrip = SingleInstance::deserializePayload(payload);
    EXPECT_EQ(roundTrip.size(), files.size());
    for (size_t i = 0; i < files.size(); ++i) {
        EXPECT_EQ(roundTrip[i], files[i]);
    }
}

TEST_F(SingleInstanceTest, EmptyPayloadRoundTrip) {
    const std::vector<std::string> files;
    const std::string payload = SingleInstance::serializePayload(files);
    const std::vector<std::string> roundTrip = SingleInstance::deserializePayload(payload);
    EXPECT_TRUE(roundTrip.empty());
}

TEST_F(SingleInstanceTest, InvalidPayloadDeserialization) {
    const std::vector<std::string> roundTrip = SingleInstance::deserializePayload("not valid json");
    EXPECT_TRUE(roundTrip.empty());
}

TEST_F(SingleInstanceTest, DefaultSocketPathValid) {
    const auto path = SingleInstance::defaultSocketPath();
    EXPECT_FALSE(path.empty());
    EXPECT_EQ(path.extension(), ".sock");
}

TEST_F(SingleInstanceTest, NotifyNonExistentSocketReturnsFalse) {
    SingleInstance client(testSocketPath_);
    EXPECT_FALSE(client.notifyRunningInstance({"test.txt"}));
}

TEST_F(SingleInstanceTest, FullIpcClientServerRoundTrip) {
    SingleInstance server(testSocketPath_);
    std::vector<std::string> receivedFiles;
    bool received = false;

    ASSERT_TRUE(server.startListening([&](const std::vector<std::string>& files) {
        receivedFiles = files;
        received = true;
    }));

    EXPECT_TRUE(server.isListening());
    EXPECT_TRUE(std::filesystem::exists(testSocketPath_));

    // Secondary instance
    std::thread clientThread([&]() {
        SingleInstance client(testSocketPath_);
        const bool sent = client.notifyRunningInstance({"first.txt", "second.txt"});
        EXPECT_TRUE(sent);
    });

    EXPECT_TRUE(server.processPendingConnection(2000));
    clientThread.join();

    EXPECT_TRUE(received);
    ASSERT_EQ(receivedFiles.size(), 2);
    // Relative paths are converted to absolute paths
    EXPECT_TRUE(receivedFiles[0].find("first.txt") != std::string::npos);
    EXPECT_TRUE(receivedFiles[1].find("second.txt") != std::string::npos);

    server.stop();
    EXPECT_FALSE(server.isListening());
    EXPECT_FALSE(std::filesystem::exists(testSocketPath_));
}
