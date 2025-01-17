// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/host_resolver_mojo.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_with_source.h"
#include "net/test/event_waiter.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace proxy_resolver {
namespace {

void Fail(int result) {
  FAIL() << "Unexpected callback called with error " << result;
}

class MockMojoHostResolverRequest {
 public:
  MockMojoHostResolverRequest(mojom::HostResolverRequestClientPtr client,
                              const base::Closure& error_callback);
  void OnConnectionError();

 private:
  mojom::HostResolverRequestClientPtr client_;
  const base::Closure error_callback_;
};

MockMojoHostResolverRequest::MockMojoHostResolverRequest(
    mojom::HostResolverRequestClientPtr client,
    const base::Closure& error_callback)
    : client_(std::move(client)), error_callback_(error_callback) {
  client_.set_connection_error_handler(base::Bind(
      &MockMojoHostResolverRequest::OnConnectionError, base::Unretained(this)));
}

void MockMojoHostResolverRequest::OnConnectionError() {
  error_callback_.Run();
}

struct HostResolverAction {
  enum Action {
    COMPLETE,
    DROP,
    RETAIN,
  };

  static HostResolverAction ReturnError(net::Error error) {
    HostResolverAction result;
    result.error = error;
    return result;
  }

  static HostResolverAction ReturnResult(const net::AddressList& address_list) {
    HostResolverAction result;
    result.addresses = address_list;
    return result;
  }

  static HostResolverAction DropRequest() {
    HostResolverAction result;
    result.action = DROP;
    return result;
  }

  static HostResolverAction RetainRequest() {
    HostResolverAction result;
    result.action = RETAIN;
    return result;
  }

  Action action = COMPLETE;
  net::AddressList addresses;
  net::Error error = net::OK;
};

class MockMojoHostResolver : public HostResolverMojo::Impl {
 public:
  explicit MockMojoHostResolver(
      const base::Closure& request_connection_error_callback);
  ~MockMojoHostResolver() override;

  void AddAction(HostResolverAction action);

  const std::vector<std::string>& requests() { return requests_received_; }

  void ResolveDns(const std::string& hostname,
                  net::ProxyResolveDnsOperation operation,
                  mojom::HostResolverRequestClientPtr) override;

 private:
  std::vector<HostResolverAction> actions_;
  size_t results_returned_ = 0;
  std::vector<std::string> requests_received_;
  const base::Closure request_connection_error_callback_;
  std::vector<std::unique_ptr<MockMojoHostResolverRequest>> requests_;
};

MockMojoHostResolver::MockMojoHostResolver(
    const base::Closure& request_connection_error_callback)
    : request_connection_error_callback_(request_connection_error_callback) {}

MockMojoHostResolver::~MockMojoHostResolver() {
  EXPECT_EQ(results_returned_, actions_.size());
}

void MockMojoHostResolver::AddAction(HostResolverAction action) {
  actions_.push_back(std::move(action));
}

void MockMojoHostResolver::ResolveDns(
    const std::string& hostname,
    net::ProxyResolveDnsOperation operation,
    mojom::HostResolverRequestClientPtr client) {
  requests_received_.push_back(hostname);
  ASSERT_LE(results_returned_, actions_.size());
  switch (actions_[results_returned_].action) {
    case HostResolverAction::COMPLETE:
      client->ReportResult(actions_[results_returned_].error,
                           std::move(actions_[results_returned_].addresses));
      break;
    case HostResolverAction::RETAIN:
      requests_.push_back(std::make_unique<MockMojoHostResolverRequest>(
          std::move(client), request_connection_error_callback_));
      break;
    case HostResolverAction::DROP:
      client.reset();
      break;
  }
  results_returned_++;
}

}  // namespace

class HostResolverMojoTest : public testing::Test {
 protected:
  enum class ConnectionErrorSource {
    REQUEST,
  };
  using Waiter = net::EventWaiter<ConnectionErrorSource>;

  void SetUp() override {
    mock_resolver_.reset(new MockMojoHostResolver(
        base::Bind(&Waiter::NotifyEvent, base::Unretained(&waiter_),
                   ConnectionErrorSource::REQUEST)));
    resolver_.reset(new HostResolverMojo(mock_resolver_.get()));
  }

  int Resolve(const std::string& hostname,
              std::vector<net::IPAddress>* out_addresses) {
    std::unique_ptr<net::ProxyHostResolver::Request> request =
        resolver_->CreateRequest(hostname,
                                 net::ProxyResolveDnsOperation::DNS_RESOLVE_EX);

    net::TestCompletionCallback callback;
    int result = callback.GetResult(request->Start(callback.callback()));

    *out_addresses = request->GetResults();
    return result;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<MockMojoHostResolver> mock_resolver_;

  std::unique_ptr<HostResolverMojo> resolver_;

  std::unique_ptr<net::HostResolver::Request> request_;

  Waiter waiter_;
};

TEST_F(HostResolverMojoTest, Basic) {
  net::AddressList address_list;
  net::IPAddress address(1, 2, 3, 4);
  address_list.push_back(net::IPEndPoint(address, 80));
  address_list.push_back(
      net::IPEndPoint(ConvertIPv4ToIPv4MappedIPv6(address), 80));
  mock_resolver_->AddAction(HostResolverAction::ReturnResult(address_list));

  std::vector<net::IPAddress> result;
  EXPECT_THAT(Resolve("example.com", &result), IsOk());
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(address_list[0].address(), result[0]);
  EXPECT_EQ(address_list[1].address(), result[1]);

  ASSERT_EQ(1u, mock_resolver_->requests().size());
  EXPECT_EQ("example.com", mock_resolver_->requests()[0]);
}

TEST_F(HostResolverMojoTest, ResolveCachedResult) {
  net::AddressList address_list;
  net::IPAddress address(1, 2, 3, 4);
  address_list.push_back(net::IPEndPoint(address, 80));
  address_list.push_back(
      net::IPEndPoint(ConvertIPv4ToIPv4MappedIPv6(address), 80));
  mock_resolver_->AddAction(HostResolverAction::ReturnResult(address_list));

  // Load results into cache.
  std::vector<net::IPAddress> result;
  ASSERT_THAT(Resolve("example.com", &result), IsOk());
  ASSERT_EQ(1u, mock_resolver_->requests().size());

  // Expect results from cache.
  result.clear();
  EXPECT_THAT(Resolve("example.com", &result), IsOk());
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(address_list[0].address(), result[0]);
  EXPECT_EQ(address_list[1].address(), result[1]);
  EXPECT_EQ(1u, mock_resolver_->requests().size());
}

TEST_F(HostResolverMojoTest, Multiple) {
  net::AddressList address_list;
  net::IPAddress address(1, 2, 3, 4);
  address_list.push_back(net::IPEndPoint(address, 80));
  mock_resolver_->AddAction(HostResolverAction::ReturnResult(address_list));
  mock_resolver_->AddAction(
      HostResolverAction::ReturnError(net::ERR_NAME_NOT_RESOLVED));

  std::unique_ptr<net::ProxyHostResolver::Request> request1 =
      resolver_->CreateRequest("example.com",
                               net::ProxyResolveDnsOperation::DNS_RESOLVE_EX);
  std::unique_ptr<net::ProxyHostResolver::Request> request2 =
      resolver_->CreateRequest("example.org",
                               net::ProxyResolveDnsOperation::DNS_RESOLVE_EX);
  net::TestCompletionCallback callback1;
  net::TestCompletionCallback callback2;
  ASSERT_EQ(net::ERR_IO_PENDING, request1->Start(callback1.callback()));
  ASSERT_EQ(net::ERR_IO_PENDING, request2->Start(callback2.callback()));

  EXPECT_THAT(callback1.GetResult(net::ERR_IO_PENDING), IsOk());
  EXPECT_THAT(callback2.GetResult(net::ERR_IO_PENDING),
              IsError(net::ERR_NAME_NOT_RESOLVED));
  ASSERT_EQ(1u, request1->GetResults().size());
  EXPECT_EQ(address_list[0].address(), request1->GetResults()[0]);
  ASSERT_EQ(0u, request2->GetResults().size());

  EXPECT_THAT(mock_resolver_->requests(),
              testing::ElementsAre("example.com", "example.org"));
}

TEST_F(HostResolverMojoTest, Error) {
  mock_resolver_->AddAction(
      HostResolverAction::ReturnError(net::ERR_NAME_NOT_RESOLVED));

  std::vector<net::IPAddress> result;
  EXPECT_THAT(Resolve("example.com", &result),
              IsError(net::ERR_NAME_NOT_RESOLVED));
  EXPECT_TRUE(result.empty());

  ASSERT_EQ(1u, mock_resolver_->requests().size());
  EXPECT_EQ("example.com", mock_resolver_->requests()[0]);
}

TEST_F(HostResolverMojoTest, EmptyResult) {
  mock_resolver_->AddAction(HostResolverAction::ReturnError(net::OK));

  std::vector<net::IPAddress> result;
  EXPECT_THAT(Resolve("example.com", &result), IsOk());
  EXPECT_TRUE(result.empty());

  ASSERT_EQ(1u, mock_resolver_->requests().size());
}

TEST_F(HostResolverMojoTest, Cancel) {
  mock_resolver_->AddAction(HostResolverAction::RetainRequest());

  std::unique_ptr<net::ProxyHostResolver::Request> request =
      resolver_->CreateRequest("example.com",
                               net::ProxyResolveDnsOperation::DNS_RESOLVE_EX);
  request->Start(base::BindOnce(&Fail));

  request.reset();
  waiter_.WaitForEvent(ConnectionErrorSource::REQUEST);

  ASSERT_EQ(1u, mock_resolver_->requests().size());
  EXPECT_EQ("example.com", mock_resolver_->requests()[0]);
}

TEST_F(HostResolverMojoTest, ImplDropsClientConnection) {
  mock_resolver_->AddAction(HostResolverAction::DropRequest());

  std::vector<net::IPAddress> result;
  EXPECT_THAT(Resolve("example.com", &result), IsError(net::ERR_FAILED));
  EXPECT_TRUE(result.empty());

  ASSERT_EQ(1u, mock_resolver_->requests().size());
  EXPECT_EQ("example.com", mock_resolver_->requests()[0]);
}

}  // namespace proxy_resolver
