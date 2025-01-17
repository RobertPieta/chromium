// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;

scoped_refptr<TransportSocketParams> CreateProxyHostParams() {
  return new TransportSocketParams(HostPortPair("proxy", 80), false,
                                   OnHostResolutionCallback());
}

scoped_refptr<SOCKSSocketParams> CreateSOCKSv5Params() {
  return new SOCKSSocketParams(CreateProxyHostParams(), true /* socks_v5 */,
                               HostPortPair("host", 80),
                               TRAFFIC_ANNOTATION_FOR_TESTS);
}

class SOCKSClientSocketPoolTest : public TestWithScopedTaskEnvironment {
 protected:
  class SOCKS5MockData {
   public:
    explicit SOCKS5MockData(IoMode mode) {
      writes_.reset(new MockWrite[3]);
      writes_[0] = MockWrite(mode, kSOCKS5GreetRequest,
                             kSOCKS5GreetRequestLength);
      writes_[1] = MockWrite(mode, kSOCKS5OkRequest, kSOCKS5OkRequestLength);
      writes_[2] = MockWrite(mode, 0);

      reads_.reset(new MockRead[3]);
      reads_[0] = MockRead(mode, kSOCKS5GreetResponse,
                           kSOCKS5GreetResponseLength);
      reads_[1] = MockRead(mode, kSOCKS5OkResponse, kSOCKS5OkResponseLength);
      reads_[2] = MockRead(mode, 0);

      data_.reset(new StaticSocketDataProvider(
          base::make_span(reads_.get(), 3), base::make_span(writes_.get(), 3)));
    }

    SocketDataProvider* data_provider() { return data_.get(); }

   private:
    std::unique_ptr<StaticSocketDataProvider> data_;
    std::unique_ptr<MockWrite[]> writes_;
    std::unique_ptr<MockRead[]> reads_;
  };

  SOCKSClientSocketPoolTest()
      : transport_socket_pool_(kMaxSockets,
                               kMaxSocketsPerGroup,
                               &transport_client_socket_factory_),
        pool_(kMaxSockets,
              kMaxSocketsPerGroup,
              &host_resolver_,
              &transport_socket_pool_,
              NULL,
              NULL) {}

  ~SOCKSClientSocketPoolTest() override = default;

  int StartRequestV5(const std::string& group_name, RequestPriority priority) {
    return test_base_.StartRequestUsingPool(
        &pool_, group_name, priority, ClientSocketPool::RespectLimits::ENABLED,
        CreateSOCKSv5Params());
  }

  int GetOrderOfRequest(size_t index) const {
    return test_base_.GetOrderOfRequest(index);
  }

  std::vector<std::unique_ptr<TestSocketRequest>>* requests() {
    return test_base_.requests();
  }

  MockClientSocketFactory transport_client_socket_factory_;
  MockTransportClientSocketPool transport_socket_pool_;

  MockHostResolver host_resolver_;
  SOCKSClientSocketPool pool_;
  ClientSocketPoolTest test_base_;
};

TEST_F(SOCKSClientSocketPoolTest, Simple) {
  SOCKS5MockData data(SYNCHRONOUS);
  data.data_provider()->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  transport_client_socket_factory_.AddSocketDataProvider(data.data_provider());

  ClientSocketHandle handle;
  int rv = handle.Init("a", CreateSOCKSv5Params(), LOW, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       CompletionOnceCallback(), &pool_, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

// Test that SocketTag passed into SOCKSClientSocketPool is applied to returned
// sockets.
#if defined(OS_ANDROID)
TEST_F(SOCKSClientSocketPoolTest, Tag) {
  MockTaggingClientSocketFactory socket_factory;
  MockTransportClientSocketPool transport_socket_pool(
      kMaxSockets, kMaxSocketsPerGroup, &socket_factory);
  SOCKSClientSocketPool pool(kMaxSockets, kMaxSocketsPerGroup, &host_resolver_,
                             &transport_socket_pool, NULL, NULL);
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
  scoped_refptr<TransportSocketParams> tcp_params(new TransportSocketParams(
      HostPortPair("proxy", 80), false, OnHostResolutionCallback()));
  scoped_refptr<SOCKSSocketParams> params(new SOCKSSocketParams(
      tcp_params, true /* socks_v5 */, HostPortPair("host", 80),
      TRAFFIC_ANNOTATION_FOR_TESTS));

  // Test socket is tagged when created synchronously.
  SOCKS5MockData data_sync(SYNCHRONOUS);
  data_sync.data_provider()->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory.AddSocketDataProvider(data_sync.data_provider());
  ClientSocketHandle handle;
  int rv = handle.Init("a", params, LOW, tag1,
                       ClientSocketPool::RespectLimits::ENABLED,
                       CompletionOnceCallback(), &pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  EXPECT_EQ(socket_factory.GetLastProducedTCPSocket()->tag(), tag1);
  EXPECT_TRUE(
      socket_factory.GetLastProducedTCPSocket()->tagged_before_connected());

  // Test socket is tagged when reused synchronously.
  StreamSocket* socket = handle.socket();
  handle.Reset();
  rv = handle.Init("a", params, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   CompletionOnceCallback(), &pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(handle.socket(), socket);
  EXPECT_EQ(socket_factory.GetLastProducedTCPSocket()->tag(), tag2);
  handle.socket()->Disconnect();
  handle.Reset();

  // Test socket is tagged when created asynchronously.
  SOCKS5MockData data_async(ASYNC);
  socket_factory.AddSocketDataProvider(data_async.data_provider());
  TestCompletionCallback callback;
  rv = handle.Init("a", params, LOW, tag1,
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  EXPECT_EQ(socket_factory.GetLastProducedTCPSocket()->tag(), tag1);
  EXPECT_TRUE(
      socket_factory.GetLastProducedTCPSocket()->tagged_before_connected());

  // Test socket is tagged when reused after being created asynchronously.
  socket = handle.socket();
  handle.Reset();
  rv = handle.Init("a", params, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   CompletionOnceCallback(), &pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(handle.socket(), socket);
  EXPECT_EQ(socket_factory.GetLastProducedTCPSocket()->tag(), tag2);
}
#endif

}  // namespace

}  // namespace net
