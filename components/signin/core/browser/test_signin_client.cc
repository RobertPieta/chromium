// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/test_signin_client.h"

#include <memory>

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

TestSigninClient::TestSigninClient(PrefService* pref_service)
    : pref_service_(pref_service),
      are_signin_cookies_allowed_(true),
      network_calls_delayed_(false),
      is_signout_allowed_(true) {}

TestSigninClient::~TestSigninClient() {}

void TestSigninClient::DoFinalInit() {}

PrefService* TestSigninClient::GetPrefs() {
  return pref_service_;
}

void TestSigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric) {
  std::move(on_signout_decision_reached)
      .Run(is_signout_allowed_ ? SignoutDecision::ALLOW_SIGNOUT
                               : SignoutDecision::DISALLOW_SIGNOUT);
}

scoped_refptr<network::SharedURLLoaderFactory>
TestSigninClient::GetURLLoaderFactory() {
  return test_url_loader_factory_.GetSafeWeakWrapper();
}

network::mojom::CookieManager* TestSigninClient::GetCookieManager() {
  if (!cookie_manager_)
    cookie_manager_ = std::make_unique<network::TestCookieManager>();
  return cookie_manager_.get();
}

std::string TestSigninClient::GetProductVersion() { return ""; }

void TestSigninClient::SetNetworkCallsDelayed(bool value) {
  network_calls_delayed_ = value;

  if (!network_calls_delayed_) {
    for (base::OnceClosure& call : delayed_network_calls_)
      std::move(call).Run();
    delayed_network_calls_.clear();
  }
}

bool TestSigninClient::IsFirstRun() const {
  return false;
}

base::Time TestSigninClient::GetInstallDate() {
  return base::Time::Now();
}

bool TestSigninClient::AreSigninCookiesAllowed() {
  return are_signin_cookies_allowed_;
}

void TestSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
}

void TestSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
}

void TestSigninClient::DelayNetworkCall(base::OnceClosure callback) {
  if (network_calls_delayed_) {
    delayed_network_calls_.push_back(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

std::unique_ptr<GaiaAuthFetcher> TestSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<GaiaAuthFetcher>(consumer, source,
                                           url_loader_factory);
}

void TestSigninClient::PreGaiaLogout(base::OnceClosure callback) {
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}
