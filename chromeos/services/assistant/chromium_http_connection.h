// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_CHROMIUM_HTTP_CONNECTION_H_
#define CHROMEOS_SERVICES_ASSISTANT_CHROMIUM_HTTP_CONNECTION_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "libassistant/shared/internal_api/http_connection.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace chromeos {
namespace assistant {

// Implements libassistant's HttpConnection.
class ChromiumHttpConnection
    : public assistant_client::HttpConnection,
      public network::mojom::ChunkedDataPipeGetter,
      public network::SimpleURLLoaderStreamConsumer,
      public base::RefCountedThreadSafe<ChromiumHttpConnection> {
 public:
  ChromiumHttpConnection(std::unique_ptr<network::SharedURLLoaderFactoryInfo>
                             url_loader_factory_info,
                         Delegate* delegate);

  // assistant_client::HttpConnection implementation:
  void SetRequest(const std::string& url, Method method) override;
  void AddHeader(const std::string& name, const std::string& value) override;
  void SetUploadContent(const std::string& content,
                        const std::string& content_type) override;
  void SetChunkedUploadContentType(const std::string& content_type) override;
  void EnableHeaderResponse() override;
  void EnablePartialResults() override;
  void Start() override;
  void Pause() override;
  void Resume() override;
  void Close() override;
  void UploadData(const std::string& data, bool is_last_chunk) override;

  // network::SimpleURLLoaderStreamConsumer implementation:
  void OnDataReceived(base::StringPiece string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

  // network::mojom::ChunkedDataPipeGetter implementation:
  void GetSize(GetSizeCallback get_size_callback) override;
  void StartReading(mojo::ScopedDataPipeProducerHandle pipe) override;

 protected:
  ~ChromiumHttpConnection() override;

 private:
  friend class base::RefCountedThreadSafe<ChromiumHttpConnection>;

  enum class State {
    NEW,
    STARTED,
    COMPLETED,
    DESTROYED,
  };

  // HttpConnection methods, re-scheduled on |task_runner|:
  void SetRequestOnTaskRunner(const std::string& url, Method method);
  void AddHeaderOnTaskRunner(const std::string& name, const std::string& value);
  void SetUploadContentOnTaskRunner(const std::string& content,
                                    const std::string& content_type);
  void SetChunkedUploadContentTypeOnTaskRunner(const std::string& content_type);
  void EnablePartialResultsOnTaskRunner();
  void StartOnTaskRunner();
  void CloseOnTaskRunner();
  void UploadDataOnTaskRunner(const std::string& data, bool is_last_chunk);

  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Send more chunked upload data.
  void SendData();

  // |upload_pipe_| can now receive more data.
  void OnUploadPipeWriteable(MojoResult unused);

  Delegate* const delegate_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  State state_ = State::NEW;
  bool has_last_chunk_ = false;
  uint64_t upload_body_size_ = 0;
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory_info_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  // The portion of the body not yet uploaded when doing chunked uploads.
  std::string upload_body_;
  // Current pipe being used to send the |upload_body_| to |url_loader_|.
  mojo::ScopedDataPipeProducerHandle upload_pipe_;
  // Watches |upload_pipe_| for writeability.
  std::unique_ptr<mojo::SimpleWatcher> upload_pipe_watcher_;
  // If non-null, invoked once the size of the upload is known.
  network::mojom::ChunkedDataPipeGetter::GetSizeCallback get_size_callback_;
  mojo::BindingSet<network::mojom::ChunkedDataPipeGetter> binding_set_;

  // Parameters to be set before Start() call.
  GURL url_;
  Method method_ = Method::GET;
  ::net::HttpRequestHeaders headers_;
  std::string upload_content_;
  std::string upload_content_type_;
  std::string chunked_upload_content_type_;
  bool handle_partial_response_ = false;

  DISALLOW_COPY_AND_ASSIGN(ChromiumHttpConnection);
};

class ChromiumHttpConnectionFactory
    : public assistant_client::HttpConnectionFactory {
 public:
  explicit ChromiumHttpConnectionFactory(
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info);
  ~ChromiumHttpConnectionFactory() override;

  // assistant_client::HttpConnectionFactory implementation:
  assistant_client::HttpConnection* Create(
      assistant_client::HttpConnection::Delegate* delegate) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumHttpConnectionFactory);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_CHROMIUM_HTTP_CONNECTION_H_
