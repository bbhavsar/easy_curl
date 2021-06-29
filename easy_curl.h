// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef EASY_CURL_LIBRARY_H
#define EASY_CURL_LIBRARY_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using namespace std;
typedef void CURL;

enum class CurlAuthType {
  NONE,
  BASIC
};

enum ErrorCode {
  kOk = 0,
  kNotFound = 1,
  kCorruption = 2,
  kNotSupported = 3,
  kInvalidArgument = 4,
  kIOError = 5,
  kAlreadyPresent = 6,
  kRuntimeError = 7,
  kNetworkError = 8,
  kIllegalState = 9,
  kNotAuthorized = 10,
  kAborted = 11,
  kRemoteError = 12,
  kServiceUnavailable = 13,
  kTimedOut = 14,
  kUninitialized = 15,
  kConfigurationError = 16,
  kIncomplete = 17,
  kEndOfFile = 18,
};

struct Error {
  ErrorCode code;
  string msg;
  Error(ErrorCode code) : code(code) {}
  Error(ErrorCode code, string msg) : code(code), msg(std::move(msg)) {}
};

// Simple wrapper around curl's "easy" interface, allowing the user to
// fetch web pages into memory using a blocking API.
//
// This is not thread-safe.
class EasyCurl {
 public:
  EasyCurl();
  ~EasyCurl();

  EasyCurl(const EasyCurl& that) = delete;
  EasyCurl& operator=(const EasyCurl& that) = delete;

  // Fetch the given URL into the provided buffer.
  // Any existing data in the buffer is replaced.
  // The optional param 'headers' holds additional headers.
  // e.g. {"Accept-Encoding: gzip"}
  Error FetchURL(const std::string& url,
                 string* dst,
                 const std::vector<std::string>& headers = {});

  // Issue an HTTP POST to the given URL with the given data.
  // Returns results in 'dst' as above.
  // The optional param 'headers' holds additional headers.
  // e.g. {"Accept-Encoding: gzip"}
  Error PostToURL(const std::string& url,
                  const std::string& post_data,
                  string* dst,
                  const std::vector<std::string>& headers = {});

  void set_return_headers(bool v) {
    return_headers_ = v;
  }

  void set_timeout(int secs) {
    timeout_secs_ = secs;
  }

  // Set the list of DNS servers to be used instead of the system default.
  // The format of the dns servers option is:
  //   host[:port][,host[:port]]...
  void set_dns_servers(std::string dns_servers) {
    dns_servers_ = std::move(dns_servers);
  }

  Error set_auth(CurlAuthType auth_type, std::string username = "", std::string password = "") {
    auth_type_ = auth_type;
    username_ = std::move(username);
    password_ = std::move(password);

    return kOk;
  }

  // Enable verbose mode for curl. This dumps debugging output to stderr, so
  // is only really useful in the context of tests.
  void set_verbose(bool v) {
    verbose_ = v;
  }

  // Whether to return an error if server responds with HTTP code >= 400.
  // By default, curl returns the returned content and the response code
  // since it's handy in case of auth-related HTTP response codes such as
  // 401 and 407. See 'man CURLOPT_FAILONERROR' for details.
  void set_fail_on_http_error(bool fail_on_http_error) {
    fail_on_http_error_ = fail_on_http_error;
  }

  // Returns the number of new connections created to achieve the previous transfer.
  int num_connects() const {
    return num_connects_;
  }

 private:
  static const constexpr size_t kErrBufSize = 256;

  // Do a request. If 'post_data' is non-NULL, does a POST.
  // Otherwise, does a GET.
  Error DoRequest(const std::string& url,
                  const std::string* post_data,
                  string* dst,
                  const std::vector<std::string>& headers = {});

  CURL* curl_;

  // Whether to return the HTTP headers with the response.
  bool return_headers_ = false;

  bool verbose_ = false;

  // The default setting for CURLOPT_FAILONERROR in libcurl is 0 (false).
  bool fail_on_http_error_ = false;

  int timeout_secs_;

  std::string dns_servers_;

  int num_connects_ = 0;

  char errbuf_[kErrBufSize];

  std::string username_;

  std::string password_;

  CurlAuthType auth_type_ = CurlAuthType::NONE;
};


void hello();

#endif //EASY_CURL_LIBRARY_H
