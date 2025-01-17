/*
 *  Copyright (c) 2020 NetEase Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * Project: curve
 * Created Date: Sun Sep 29 2019
 * Author: xuchaojie
 */

#include <gtest/gtest.h>
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <json/json.h>

#include "test/integration/cluster_common/cluster.h"
#include "src/client/libcurve_file.h"
#include "src/snapshotcloneserver/snapshot/snapshot_service_manager.h"
#include "src/snapshotcloneserver/clone/clone_service_manager.h"
#include "test/integration/snapshotcloneserver/test_snapshotcloneserver_helpler.h"
#include "src/common/snapshotclone/snapshotclone_define.h"
#include "src/snapshotcloneserver/common/snapshotclone_meta_store.h"
#include "src/client/source_reader.h"

using curve::CurveCluster;
using curve::client::FileClient;
using curve::client::UserInfo_t;
using curve::client::SourceReader;

const std::string kTestPrefix = "ConSCSTest";  // NOLINT

const uint64_t chunkSize = 16ULL * 1024 * 1024;
const uint64_t segmentSize = 32ULL * 1024 * 1024;
const uint64_t chunkGap = 1;

const char* kEtcdClientIpPort = "127.0.0.1:10011";
const char* kEtcdPeerIpPort = "127.0.0.1:10012";
const char* kMdsIpPort = "127.0.0.1:10013";
const char* kChunkServerIpPort1 = "127.0.0.1:10014";
const char* kChunkServerIpPort2 = "127.0.0.1:10015";
const char* kChunkServerIpPort3 = "127.0.0.1:10016";
const char* kSnapshotCloneServerIpPort = "127.0.0.1:10017";
const int kMdsDummyPort = 10018;

const char* kSnapshotCloneServerDummyServerPort = "12001";
const char* kLeaderCampaginPrefix = "snapshotcloneserverleaderlock2";

const std::string kLogPath = "./runlog/" + kTestPrefix + "Log";  // NOLINT
const std::string kMdsDbName = kTestPrefix + "DB";               // NOLINT
const std::string kMdsConfigPath =                               // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_mds.conf";

const std::string kCSConfigPath =  // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_chunkserver.conf";

const std::string kCsClientConfigPath =  // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_cs_client.conf";

const std::string kSnapClientConfigPath =  // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_snap_client.conf";

const std::string kS3ConfigPath =  // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix + "_s3.conf";

const std::string kSCSConfigPath =  // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_scs.conf";

const std::string kClientConfigPath =  // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_client.conf";

const std::vector<std::string> mdsConfigOptions{
    std::string("mds.listen.addr=") + kMdsIpPort,
    std::string("mds.etcd.endpoint=") + kEtcdClientIpPort,
    std::string("mds.DbName=") + kMdsDbName,
    std::string("mds.file.expiredTimeUs=50000"),
    std::string("mds.file.expiredTimeUs=10000"),
    std::string("mds.snapshotcloneclient.addr=") + kSnapshotCloneServerIpPort,
};

const std::vector<std::string> mdsConf1{
    { " --graceful_quit_on_sigterm" },
    std::string(" --confPath=") + kMdsConfigPath,
    std::string(" --log_dir=") + kLogPath,
    std::string(" --segmentSize=") + std::to_string(segmentSize),
    { " --stderrthreshold=3" },
};

const std::vector<std::string> chunkserverConfigOptions{
    std::string("mds.listen.addr=") + kMdsIpPort,
    std::string("curve.config_path=") + kCsClientConfigPath,
    std::string("s3.config_path=") + kS3ConfigPath,
    "walfilepool.use_chunk_file_pool=false",
    "walfilepool.enable_get_segment_from_pool=false",
    "global.block_size=4096",
    "global.meta_page_size=4096",
};

const std::vector<std::string> csClientConfigOptions{
    std::string("mds.listen.addr=") + kMdsIpPort,
};

const std::vector<std::string> snapClientConfigOptions{
    std::string("mds.listen.addr=") + kMdsIpPort,
};

const std::vector<std::string> s3ConfigOptions{};

const std::vector<std::string> chunkserverConf1{
    { " --graceful_quit_on_sigterm" },
    { " -chunkServerStoreUri=local://./" + kTestPrefix + "1/" },
    { " -chunkServerMetaUri=local://./" + kTestPrefix +
      "1/chunkserver.dat" },  // NOLINT
    { " -copySetUri=local://./" + kTestPrefix + "1/copysets" },
    { " -raftSnapshotUri=curve://./" + kTestPrefix + "1/copysets" },
    { " -recycleUri=local://./" + kTestPrefix + "1/recycler" },
    { " -chunkFilePoolDir=./" + kTestPrefix + "1/chunkfilepool/" },
    { " -chunkFilePoolMetaPath=./" + kTestPrefix +
      "1/chunkfilepool.meta" },  // NOLINT
    std::string(" -conf=") + kCSConfigPath,
    { " -raft_sync_segments=true" },
    std::string(" --log_dir=") + kLogPath,
    { " --stderrthreshold=3" },
    { " -raftLogUri=curve://./" + kTestPrefix + "1/copysets" },
    { " -walFilePoolDir=./" + kTestPrefix + "1/walfilepool/" },
    { " -walFilePoolMetaPath=./" + kTestPrefix +
        "1/walfilepool.meta" },
};

const std::vector<std::string> chunkserverConf2{
    { " --graceful_quit_on_sigterm" },
    { " -chunkServerStoreUri=local://./" + kTestPrefix + "2/" },
    { " -chunkServerMetaUri=local://./" + kTestPrefix +
      "2/chunkserver.dat" },  // NOLINT
    { " -copySetUri=local://./" + kTestPrefix + "2/copysets" },
    { " -raftSnapshotUri=curve://./" + kTestPrefix + "2/copysets" },
    { " -recycleUri=local://./" + kTestPrefix + "2/recycler" },
    { " -chunkFilePoolDir=./" + kTestPrefix + "2/chunkfilepool/" },
    { " -chunkFilePoolMetaPath=./" + kTestPrefix +
      "2/chunkfilepool.meta" },  // NOLINT
    std::string(" -conf=") + kCSConfigPath,
    { " -raft_sync_segments=true" },
    std::string(" --log_dir=") + kLogPath,
    { " --stderrthreshold=3" },
    { " -raftLogUri=curve://./" + kTestPrefix + "2/copysets" },
    { " -walFilePoolDir=./" + kTestPrefix + "2/walfilepool/" },
    { " -walFilePoolMetaPath=./" + kTestPrefix +
        "2/walfilepool.meta" },
};

const std::vector<std::string> chunkserverConf3{
    { " --graceful_quit_on_sigterm" },
    { " -chunkServerStoreUri=local://./" + kTestPrefix + "3/" },
    { " -chunkServerMetaUri=local://./" + kTestPrefix +
      "3/chunkserver.dat" },  // NOLINT
    { " -copySetUri=local://./" + kTestPrefix + "3/copysets" },
    { " -raftSnapshotUri=curve://./" + kTestPrefix + "3/copysets" },
    { " -recycleUri=local://./" + kTestPrefix + "3/recycler" },
    { " -chunkFilePoolDir=./" + kTestPrefix + "3/chunkfilepool/" },
    { " -chunkFilePoolMetaPath=./" + kTestPrefix +
      "3/chunkfilepool.meta" },  // NOLINT
    std::string(" -conf=") + kCSConfigPath,
    { " -raft_sync_segments=true" },
    std::string(" --log_dir=") + kLogPath,
    { " --stderrthreshold=3" },
    { " -raftLogUri=curve://./" + kTestPrefix + "3/copysets" },
    { " -walFilePoolDir=./" + kTestPrefix + "3/walfilepool/" },
    { " -walFilePoolMetaPath=./" + kTestPrefix +
        "3/walfilepool.meta" },
};

const std::vector<std::string> snapshotcloneserverConfigOptions{
    std::string("client.config_path=") + kSnapClientConfigPath,
    std::string("s3.config_path=") + kS3ConfigPath,
    std::string("metastore.db_name=") + kMdsDbName,
    std::string("server.snapshotPoolThreadNum=8"),
    std::string("server.snapshotCoreThreadNum=2"),
    std::string("server.clonePoolThreadNum=8"),
    std::string("server.createCloneChunkConcurrency=2"),
    std::string("server.recoverChunkConcurrency=2"),
    // 最大快照数修改为3，以测试快照达到上限的用例
    std::string("server.maxSnapshotLimit=3"),
    std::string("client.methodRetryTimeSec=1"),
    std::string("server.clientAsyncMethodRetryTimeSec=1"),
    std::string("etcd.endpoint=") + kEtcdClientIpPort,
    std::string("server.dummy.listen.port=") +
        kSnapshotCloneServerDummyServerPort,
    std::string("leader.campagin.prefix=") + kLeaderCampaginPrefix,
    std::string("server.backEndReferenceRecordScanIntervalMs=100"),
    std::string("server.backEndReferenceFuncScanIntervalMs=1000"),
};

const std::vector<std::string> snapshotcloneConf{
    std::string(" --conf=") + kSCSConfigPath,
    std::string(" --log_dir=") + kLogPath,
    { " --stderrthreshold=3" },
};

const std::vector<std::string> clientConfigOptions{
    std::string("mds.listen.addr=") + kMdsIpPort,
    std::string("global.logPath=") + kLogPath,
    std::string("mds.rpcTimeoutMS=4000"),
};

const char* testFile1_ = "/concurrentItUser1/file1";
const char* testFile2_ =
    "/concurrentItUser1/file2";  // 将在TestImage2Clone2Success中删除  //NOLINT
const char* testFile3_ = "/concurrentItUser2/file3";
const char* testFile4_ = "/concurrentItUser1/file3";
const char* testUser1_ = "concurrentItUser1";
const char* testUser2_ = "concurrentItUser2";

namespace curve {
namespace snapshotcloneserver {

class SnapshotCloneServerTest : public ::testing::Test {
 public:
    static void SetUpTestCase() {
        std::string mkLogDirCmd = std::string("mkdir -p ") + kLogPath;
        system(mkLogDirCmd.c_str());

        cluster_ = new CurveCluster();
        ASSERT_NE(nullptr, cluster_);

        // 初始化db
        system(std::string("rm -rf " + kTestPrefix + ".etcd").c_str());
        system(std::string("rm -rf " + kTestPrefix + "1").c_str());
        system(std::string("rm -rf " + kTestPrefix + "2").c_str());
        system(std::string("rm -rf " + kTestPrefix + "3").c_str());

        // 启动etcd
        pid_t pid = cluster_->StartSingleEtcd(
            1, kEtcdClientIpPort, kEtcdPeerIpPort,
            std::vector<std::string>{ " --name " + kTestPrefix });
        LOG(INFO) << "etcd 1 started on " << kEtcdClientIpPort
                  << "::" << kEtcdPeerIpPort << ", pid = " << pid;
        ASSERT_GT(pid, 0);

        cluster_->PrepareConfig<MDSConfigGenerator>(kMdsConfigPath,
                                                    mdsConfigOptions);

        // 启动一个mds
        pid = cluster_->StartSingleMDS(1, kMdsIpPort, kMdsDummyPort, mdsConf1,
                                       true);
        LOG(INFO) << "mds 1 started on " << kMdsIpPort << ", pid = " << pid;
        ASSERT_GT(pid, 0);

        // 创建物理池
        ASSERT_EQ(0, cluster_->PreparePhysicalPool(
                         1,
                         "./test/integration/snapshotcloneserver/"
                         "config/topo2.json"));

        // format chunkfilepool and walfilepool
        std::vector<std::thread> threadpool(3);

        threadpool[0] =
            std::thread(&CurveCluster::FormatFilePool, cluster_,
                        "./" + kTestPrefix + "1/chunkfilepool/",
                        "./" + kTestPrefix + "1/chunkfilepool.meta",
                        "./" + kTestPrefix + "1/chunkfilepool/", 1);
        threadpool[1] =
            std::thread(&CurveCluster::FormatFilePool, cluster_,
                        "./" + kTestPrefix + "2/chunkfilepool/",
                        "./" + kTestPrefix + "2/chunkfilepool.meta",
                        "./" + kTestPrefix + "2/chunkfilepool/", 1);
        threadpool[2] =
            std::thread(&CurveCluster::FormatFilePool, cluster_,
                        "./" + kTestPrefix + "3/chunkfilepool/",
                        "./" + kTestPrefix + "3/chunkfilepool.meta",
                        "./" + kTestPrefix + "3/chunkfilepool/", 1);
        for (int i = 0; i < 3; i++) {
            threadpool[i].join();
        }

        cluster_->PrepareConfig<CSClientConfigGenerator>(kCsClientConfigPath,
                                                         csClientConfigOptions);

        cluster_->PrepareConfig<S3ConfigGenerator>(kS3ConfigPath,
                                                   s3ConfigOptions);

        cluster_->PrepareConfig<CSConfigGenerator>(kCSConfigPath,
                                                   chunkserverConfigOptions);

        // 创建chunkserver
        pid = cluster_->StartSingleChunkServer(1, kChunkServerIpPort1,
                                               chunkserverConf1);
        LOG(INFO) << "chunkserver 1 started on " << kChunkServerIpPort1
                  << ", pid = " << pid;
        ASSERT_GT(pid, 0);
        pid = cluster_->StartSingleChunkServer(2, kChunkServerIpPort2,
                                               chunkserverConf2);
        LOG(INFO) << "chunkserver 2 started on " << kChunkServerIpPort2
                  << ", pid = " << pid;
        ASSERT_GT(pid, 0);
        pid = cluster_->StartSingleChunkServer(3, kChunkServerIpPort3,
                                               chunkserverConf3);
        LOG(INFO) << "chunkserver 3 started on " << kChunkServerIpPort3
                  << ", pid = " << pid;
        ASSERT_GT(pid, 0);

        std::this_thread::sleep_for(std::chrono::seconds(5));

        // 创建逻辑池, 并睡眠一段时间让底层copyset先选主
        ASSERT_EQ(0, cluster_->PrepareLogicalPool(
                         1,
                         "./test/integration/snapshotcloneserver/"
                         "config/topo2.json"));

        cluster_->PrepareConfig<SnapClientConfigGenerator>(
            kSnapClientConfigPath, snapClientConfigOptions);

        cluster_->PrepareConfig<SCSConfigGenerator>(
            kSCSConfigPath, snapshotcloneserverConfigOptions);

        cluster_->StartSnapshotCloneServer(1, kSnapshotCloneServerIpPort,
                                           snapshotcloneConf);

        cluster_->PrepareConfig<ClientConfigGenerator>(kClientConfigPath,
                                                       clientConfigOptions);

        fileClient_ = new FileClient();
        fileClient_->Init(kClientConfigPath);

        UserInfo_t userinfo;
        userinfo.owner = "concurrentItUser1";

        ASSERT_EQ(0, fileClient_->Mkdir("/concurrentItUser1", userinfo));

        std::string fakeData(4096, 'x');
        ASSERT_TRUE(CreateAndWriteFile(testFile1_, testUser1_, fakeData));
        LOG(INFO) << "Write testFile1_ success.";

        ASSERT_TRUE(CreateAndWriteFile(testFile2_, testUser1_, fakeData));
        LOG(INFO) << "Write testFile2_ success.";

        UserInfo_t userinfo2;
        userinfo2.owner = "concurrentItUser2";
        ASSERT_EQ(0, fileClient_->Mkdir("/concurrentItUser2", userinfo2));

        ASSERT_TRUE(CreateAndWriteFile(testFile3_, testUser2_, fakeData));
        LOG(INFO) << "Write testFile3_ success.";

        ASSERT_EQ(0, fileClient_->Create(testFile4_, userinfo,
                                         10ULL * 1024 * 1024 * 1024));
    }

    static bool CreateAndWriteFile(const std::string& fileName,
                                   const std::string& user,
                                   const std::string& dataSample) {
        UserInfo_t userinfo;
        userinfo.owner = user;
        int ret =
            fileClient_->Create(fileName, userinfo, 10ULL * 1024 * 1024 * 1024);
        if (ret < 0) {
            LOG(ERROR) << "Create fail, ret = " << ret;
            return false;
        }
        return WriteFile(fileName, user, dataSample);
    }

    static bool WriteFile(const std::string& fileName, const std::string& user,
                          const std::string& dataSample) {
        int ret = 0;
        UserInfo_t userinfo;
        userinfo.owner = user;
        int testfd1_ = fileClient_->Open(fileName, userinfo);
        if (testfd1_ < 0) {
            LOG(ERROR) << "Open fail, ret = " << testfd1_;
            return false;
        }
        // 每个chunk写前面4k数据, 写两个segment
        uint64_t totalChunk = 2ULL * segmentSize / chunkSize;
        for (uint64_t i = 0; i < totalChunk / chunkGap; i++) {
            ret =
                fileClient_->Write(testfd1_, dataSample.c_str(),
                                   i * chunkSize * chunkGap, dataSample.size());
            if (ret < 0) {
                LOG(ERROR) << "Write Fail, ret = " << ret;
                return false;
            }
        }
        ret = fileClient_->Close(testfd1_);
        if (ret < 0) {
            LOG(ERROR) << "Close fail, ret = " << ret;
            return false;
        }
        return true;
    }

    static bool CheckFileData(const std::string& fileName,
                              const std::string& user,
                              const std::string& dataSample) {
        UserInfo_t userinfo;
        userinfo.owner = user;
        int dstFd = fileClient_->Open(fileName, userinfo);
        if (dstFd < 0) {
            LOG(ERROR) << "Open fail, ret = " << dstFd;
            return false;
        }

        int ret = 0;
        uint64_t totalChunk = 2ULL * segmentSize / chunkSize;
        for (uint64_t i = 0; i < totalChunk / chunkGap; i++) {
            char buf[4096];
            ret = fileClient_->Read(dstFd, buf, i * chunkSize * chunkGap, 4096);
            if (ret < 0) {
                LOG(ERROR) << "Read fail, ret = " << ret;
                return false;
            }
            std::string data(buf, 4096);
            if (data != dataSample) {
                LOG(ERROR) << "CheckFileData not Equal, data = [" << data
                           << "] , expect data = [" << dataSample << "].";
                return false;
            }
        }
        ret = fileClient_->Close(dstFd);
        if (ret < 0) {
            LOG(ERROR) << "Close fail, ret = " << ret;
            return false;
        }
        return true;
    }

    static void TearDownTestCase() {
        fileClient_->UnInit();
        delete fileClient_;
        fileClient_ = nullptr;
        ASSERT_EQ(0, cluster_->StopCluster());
        delete cluster_;
        cluster_ = nullptr;
        system(std::string("rm -rf " + kTestPrefix + ".etcd").c_str());
        system(std::string("rm -rf " + kTestPrefix + "1").c_str());
        system(std::string("rm -rf " + kTestPrefix + "2").c_str());
        system(std::string("rm -rf " + kTestPrefix + "3").c_str());
    }

    void SetUp() {}

    void TearDown() {}

    void PrepareSnapshotForTestFile1(std::string* uuid1) {
        if (!hasSnapshotForTestFile1_) {
            int ret = MakeSnapshot(testUser1_, testFile1_, "snap1", uuid1);
            ASSERT_EQ(0, ret);
            bool success1 =
                CheckSnapshotSuccess(testUser1_, testFile1_, *uuid1);
            ASSERT_TRUE(success1);
            hasSnapshotForTestFile1_ = true;
            snapIdForTestFile1_ = *uuid1;
        }
    }

    void WaitDeleteSnapshotForTestFile1() {
        if (hasSnapshotForTestFile1_) {
            ASSERT_EQ(0, DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_,
                                                       snapIdForTestFile1_));
        }
    }

    static CurveCluster* cluster_;
    static FileClient* fileClient_;

    bool hasSnapshotForTestFile1_ = false;
    std::string snapIdForTestFile1_;
};

CurveCluster* SnapshotCloneServerTest::cluster_ = nullptr;
FileClient* SnapshotCloneServerTest::fileClient_ = nullptr;

// 并发测试用例

// 这个用例测试快照层数，放在最前面
TEST_F(SnapshotCloneServerTest, TestSameFile3Snapshot) {
    std::string uuid1, uuid2, uuid3;
    int ret = MakeSnapshot(testUser1_, testFile1_, "snap1", &uuid1);
    ASSERT_EQ(0, ret);
    ret = MakeSnapshot(testUser1_, testFile1_, "snap2", &uuid2);
    ASSERT_EQ(0, ret);
    ret = MakeSnapshot(testUser1_, testFile1_, "snap3", &uuid3);
    ASSERT_EQ(0, ret);
    bool success1 = CheckSnapshotSuccess(testUser1_, testFile1_, uuid1);
    ASSERT_TRUE(success1);

    bool success2 = CheckSnapshotSuccess(testUser1_, testFile1_, uuid2);
    ASSERT_TRUE(success2);

    bool success3 = CheckSnapshotSuccess(testUser1_, testFile1_, uuid3);
    ASSERT_TRUE(success3);

    // 快照层数设置为3，尝试再打一次快照，超过层数失败
    ret = MakeSnapshot(testUser1_, testFile1_, "snap3", &uuid3);
    ASSERT_EQ(kErrCodeSnapshotCountReachLimit, ret);

    ASSERT_EQ(0, DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_, uuid1));
    ASSERT_EQ(0, DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_, uuid2));
    ASSERT_EQ(0, DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_, uuid3));
}

TEST_F(SnapshotCloneServerTest, TestCancelAndMakeSnaphotConcurrent) {
    std::string uuid1, uuid2;
    int ret = MakeSnapshot(testUser1_, testFile1_, "snapToCancle", &uuid1);
    ASSERT_EQ(0, ret);
    ret = MakeSnapshot(testUser1_, testFile1_, "snap2", &uuid2);
    ASSERT_EQ(0, ret);
    bool success1 = false;
    bool isCancel = false;
    for (int i = 0; i < 600; i++) {
        FileSnapshotInfo info1;
        int retCode = GetSnapshotInfo(testUser1_, testFile1_, uuid1, &info1);
        if (retCode == 0) {
            if (info1.GetSnapshotInfo().GetStatus() == Status::pending) {
                if (!isCancel) {
                    CancelSnapshot(testUser1_, testFile1_, uuid1);
                    isCancel = true;
                }
                std::this_thread::sleep_for(std::chrono::seconds(30));
                continue;
            } else if (info1.GetSnapshotInfo().GetStatus() == Status::done) {
                success1 = false;
                break;
            } else {
                FAIL() << "Snapshot Fail On status = "
                       << static_cast<int>(info1.GetSnapshotInfo().GetStatus());
            }
        } else {
            success1 = true;
        }
    }
    ASSERT_TRUE(success1);

    bool success2 = CheckSnapshotSuccess(testUser1_, testFile1_, uuid2);
    ASSERT_TRUE(success2);

    ASSERT_EQ(0, DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_, uuid2));
}

TEST_F(SnapshotCloneServerTest, Test3File3Snapshot) {
    std::string uuid1, uuid2, uuid3;
    int ret = MakeSnapshot(testUser1_, testFile1_, "snap1", &uuid1);
    ASSERT_EQ(0, ret);
    ret = MakeSnapshot(testUser1_, testFile2_, "snap2", &uuid2);
    ASSERT_EQ(0, ret);
    ret = MakeSnapshot(testUser2_, testFile3_, "snap3", &uuid3);
    ASSERT_EQ(0, ret);
    bool success1 = CheckSnapshotSuccess(testUser1_, testFile1_, uuid1);
    ASSERT_TRUE(success1);

    bool success2 = CheckSnapshotSuccess(testUser1_, testFile2_, uuid2);
    ASSERT_TRUE(success2);

    bool success3 = CheckSnapshotSuccess(testUser2_, testFile3_, uuid3);
    ASSERT_TRUE(success3);
    ASSERT_EQ(0, DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_, uuid1));
    ASSERT_EQ(0, DeleteAndCheckSnapshotSuccess(testUser1_, testFile2_, uuid2));
    ASSERT_EQ(0, DeleteAndCheckSnapshotSuccess(testUser2_, testFile3_, uuid3));
}

TEST_F(SnapshotCloneServerTest, TestSnapSameClone1Success) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);

    std::string dstFile = "/clone1";

    std::string uuid1, uuid2;
    int ret1, ret2;
    ret1 = CloneOrRecover("Clone", testUser1_, snapId, dstFile, true, &uuid1);
    ASSERT_EQ(0, ret1);

    // 幂等
    ret2 = CloneOrRecover("Clone", testUser1_, snapId, dstFile, true, &uuid2);
    ASSERT_EQ(0, ret2);

    // Flatten
    ret1 = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret1);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    TaskCloneInfo info2;
    int retCode = GetCloneTaskInfo(testUser1_, uuid2, &info2);
    ASSERT_EQ(0, retCode);
}

TEST_F(SnapshotCloneServerTest, TestSnap2Clone2Success) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);

    std::string dstFile1 = "/clone1_1";
    std::string dstFile2 = "/clone1_2";
    std::string uuid1, uuid2;
    int ret1, ret2;
    ret1 = CloneOrRecover("Clone", testUser1_, snapId, dstFile1, true, &uuid1);
    ASSERT_EQ(0, ret1);

    ret2 = CloneOrRecover("Clone", testUser1_, snapId, dstFile2, true, &uuid2);
    ASSERT_EQ(0, ret2);

    // Flatten
    ret1 = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret1);
    ret2 = Flatten(testUser1_, uuid2);
    ASSERT_EQ(0, ret2);

    bool success1 = false;
    bool firstDelete = false;
    for (int i = 0; i < 600; i++) {
        TaskCloneInfo info1;
        int retCode = GetCloneTaskInfo(testUser1_, uuid1, &info1);
        if (retCode != 0) {
            break;
        }
        if (info1.GetCloneInfo().GetStatus() == CloneStatus::cloning) {
            if (!firstDelete) {
                int retCode2 = DeleteSnapshot(testUser1_, testFile1_, snapId);
                ASSERT_EQ(kErrCodeSnapshotCannotDeleteCloning, retCode2);
                firstDelete = true;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
            continue;
        } else if (info1.GetCloneInfo().GetStatus() == CloneStatus::done) {
            success1 = true;
            break;
        } else {
            FAIL() << "Clone Fail On status = "
                   << static_cast<int>(info1.GetCloneInfo().GetStatus());
        }
    }
    ASSERT_TRUE(firstDelete);
    ASSERT_TRUE(success1);

    bool success2 = CheckCloneOrRecoverSuccess(testUser1_, uuid2, true);
    ASSERT_TRUE(success2);

    int retCode3 =
        DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_, snapId);
    ASSERT_EQ(0, retCode3);
}

TEST_F(SnapshotCloneServerTest, TestImage2Clone2Success) {
    std::string dstFile1 = "/clone2_1";
    std::string dstFile2 = "/clone2_2";
    std::string uuid1, uuid2;
    int ret1, ret2;
    ret1 =
        CloneOrRecover("Clone", testUser1_, testFile2_, dstFile1, true, &uuid1);
    ASSERT_EQ(0, ret1);

    ret2 =
        CloneOrRecover("Clone", testUser1_, testFile2_, dstFile2, true, &uuid2);
    ASSERT_EQ(0, ret2);

    // Flatten
    ret1 = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret1);
    ret2 = Flatten(testUser1_, uuid2);
    ASSERT_EQ(0, ret2);

    UserInfo_t userinfo;
    userinfo.owner = testUser1_;

    bool success1 = false;
    bool firstDelete = false;
    for (int i = 0; i < 600; i++) {
        TaskCloneInfo info1;
        int retCode = GetCloneTaskInfo(testUser1_, uuid1, &info1);
        if (retCode != 0) {
            break;
        }
        if (info1.GetCloneInfo().GetStatus() == CloneStatus::cloning) {
            if (!firstDelete) {
                int retCode2 = fileClient_->Unlink(testFile2_, userinfo, false);
                ASSERT_EQ(-LIBCURVE_ERROR::DELETE_BEING_CLONED, retCode2);
                firstDelete = true;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
            continue;
        } else if (info1.GetCloneInfo().GetStatus() == CloneStatus::done) {
            success1 = true;
            break;
        } else {
            FAIL() << "Clone Fail On status = "
                   << static_cast<int>(info1.GetCloneInfo().GetStatus());
        }
    }
    ASSERT_TRUE(firstDelete);
    ASSERT_TRUE(success1);

    bool success2 = CheckCloneOrRecoverSuccess(testUser1_, uuid2, true);
    ASSERT_TRUE(success2);

    int retCode3 = fileClient_->Unlink(testFile2_, userinfo, false);
    ASSERT_EQ(0, retCode3);
}

TEST_F(SnapshotCloneServerTest, TestReadWriteWhenLazyCloneSnap) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);

    std::string uuid1;
    std::string dstFile = "/concurrentItUser1/SnapLazyClone4Rw";
    int ret =
        CloneOrRecover("Clone", testUser1_, snapId, dstFile, true, &uuid1);
    ASSERT_EQ(0, ret);

    // Flatten
    ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    std::string fakeData(4096, 'y');
    ASSERT_TRUE(WriteFile(dstFile, testUser1_, fakeData));
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));

    // 判断是否clone成功
    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);
}

TEST_F(SnapshotCloneServerTest, TestReadWriteWhenLazyCloneImage) {
    std::string uuid1;
    std::string dstFile = "/concurrentItUser1/ImageLazyClone4Rw";

    int ret =
        CloneOrRecover("Clone", testUser1_, testFile1_, dstFile, true, &uuid1);
    ASSERT_EQ(0, ret);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, true));

    // clone完成stage1之后即可对外提供服务，测试克隆卷是否能正常读取数据
    std::string fakeData1(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData1));

    // Flatten
    ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    std::string fakeData2(4096, 'y');
    ASSERT_TRUE(WriteFile(dstFile, testUser1_, fakeData2));
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData2));

    // 判断是否clone成功
    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);
}

TEST_F(SnapshotCloneServerTest, TestReadWriteWhenLazyRecoverSnap) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);

    std::string uuid1;
    std::string dstFile = testFile1_;
    int ret =
        CloneOrRecover("Recover", testUser1_, snapId, dstFile, true, &uuid1);
    ASSERT_EQ(0, ret);

    // Flatten
    ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    std::string fakeData(4096, 'y');
    ASSERT_TRUE(WriteFile(dstFile, testUser1_, fakeData));
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));

    // 判断是否clone成功
    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);
}

}  // namespace snapshotcloneserver
}  // namespace curve
