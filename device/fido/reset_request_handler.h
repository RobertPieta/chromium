// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_RESET_REQUEST_HANDLER_H_
#define DEVICE_FIDO_RESET_REQUEST_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace service_manager {
class Connector;
};  // namespace service_manager

namespace device {

class FidoAuthenticator;

namespace pin {
struct EmptyResponse;
}

// ResetRequestHandler is a simple state machine that gets a touch from an
// authenticator and then sends a CTAP2 reset request. This is expected to be
// driven by Settings UI for users to manually reset authenticators.
class COMPONENT_EXPORT(DEVICE_FIDO) ResetRequestHandler
    : public FidoRequestHandlerBase {
 public:
  // ResetSentCallback will be run once an authenticator has been touched and a
  // reset command has been sent to it. This will always occur before
  // |FinishedCallback|.
  using ResetSentCallback = base::OnceCallback<void()>;
  // FinishedCallback will be called once this process has completed. If the
  // status is |kCtap1ErrInvalidCommand| then the user may have selected a non-
  // CTAP2 authenticator, in which case no reset command was ever sent.
  // Otherwise the status is the result of the reset command.
  using FinishedCallback = base::OnceCallback<void(CtapDeviceResponseCode)>;

  ResetRequestHandler(
      service_manager::Connector* connector,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      ResetSentCallback reset_sent_callback,
      FinishedCallback finished_callback);
  ~ResetRequestHandler() override;

 private:
  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;

  void OnTouch(FidoAuthenticator* authenticator);
  void OnResetComplete(CtapDeviceResponseCode status,
                       base::Optional<pin::EmptyResponse> response);

  ResetSentCallback reset_sent_callback_;
  FinishedCallback finished_callback_;
  bool processed_touch_ = false;
  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<ResetRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResetRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_RESET_REQUEST_HANDLER_H_
