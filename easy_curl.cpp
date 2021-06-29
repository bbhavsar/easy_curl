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

#include "easy_curl.h"
#include "scoped_cleanup.h"

#include <iostream>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <cassert>

namespace {

inline Error TranslateError(CURLcode code, const char* errbuf) {
  if (code == CURLE_OK) {
    return kOk;
  }

  string err_msg = curl_easy_strerror(code);
  if (strlen(errbuf) != 0) {
    err_msg += ": " + string(errbuf);
  }

  if (code == CURLE_OPERATION_TIMEDOUT) {
    return Error(kTimedOut, "curl timeout" + err_msg);
  }
  return Error(kNetworkError, "curl error" + err_msg);
}

extern "C" {
size_t WriteCallback(void* buffer, size_t size, size_t nmemb, void* user_ptr) {
  size_t real_size = size * nmemb;
  string* buf = reinterpret_cast<string*>(user_ptr);
  buf->append(reinterpret_cast<const char*>(buffer), real_size);
  return real_size;
}
} // extern "C"

} // anonymous namespace

#define CURL_RETURN_NOT_OK(expr)  { \
    auto error = TranslateError((expr), errbuf_); \
    if (error.code != kOk) { \
      return error; \
    } \
  }

EasyCurl::EasyCurl() : timeout_secs_(-1) {
  // curl_global_init() is not thread safe and multiple calls have the
  // same effect as one call.
  // See more details: https://curl.haxx.se/libcurl/c/curl_global_init.html
  static std::once_flag once;
  std::call_once(once, []() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT & ~CURL_GLOBAL_SSL) != 0) {
      cerr << "CURL init fatal error";
      exit(1);
    }
  });
  curl_ = curl_easy_init();
  if (curl_ == nullptr) {
    cerr << "Could not init curl";
    exit(1);
  }

  // Set the error buffer to enhance error messages with more details, when
  // available.
  static_assert(kErrBufSize >= CURL_ERROR_SIZE, "kErrBufSize is too small");
  const auto code = curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, errbuf_);
  if (code != CURLE_OK) {
    cerr << "Failed configuring CURL error buffer";
    exit(1);
  }
}

EasyCurl::~EasyCurl() {
  curl_easy_cleanup(curl_);
}

Error EasyCurl::FetchURL(const string& url, string* dst,
                         const vector<string>& headers) {
  return DoRequest(url, nullptr, dst, headers);
}

Error EasyCurl::PostToURL(const string& url,
                          const string& post_data,
                          string* dst,
                          const vector<string>& headers) {
  return DoRequest(url, &post_data, dst, headers);
}

Error EasyCurl::DoRequest(const string& url,
                          const string* post_data,
                          string* dst,
                          const vector<string>& headers) {
  assert(dst != nullptr);
  dst->clear();
  // Mark the error buffer as cleared.
  errbuf_[0] = 0;

  switch (auth_type_) {
    case CurlAuthType::BASIC:
      CURL_RETURN_NOT_OK(curl_easy_setopt(
          curl_, CURLOPT_HTTPAUTH, CURLAUTH_BASIC));
      break;
    case CurlAuthType::NONE:
      break;
    default:
      CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_HTTPAUTH, CURLAUTH_ANY));
      break;
  }

  if (auth_type_ != CurlAuthType::NONE) {
    CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_USERNAME, username_.c_str()));
    CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_PASSWORD, password_.c_str()));
  }

  if (verbose_) {
    CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1));
  }
  if (fail_on_http_error_) {
    CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_FAILONERROR, 1));
  }

  // Add headers if specified.
  struct curl_slist* curl_headers = nullptr;
  auto clean_up_curl_slist = MakeScopedCleanup([&]() {
    curl_slist_free_all(curl_headers);
  });

  for (const auto& header : headers) {
    curl_headers = curl_slist_append(curl_headers, header.c_str());
  }
  CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, curl_headers));

  CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_URL, url.c_str()));
  if (return_headers_) {
    CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_HEADER, 1));
  }
  CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback));
  CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_WRITEDATA, static_cast<void *>(dst)));
  if (post_data) {
    CURL_RETURN_NOT_OK(curl_easy_setopt(
        curl_, CURLOPT_POSTFIELDS, post_data->c_str()));
  }

  if (timeout_secs_ > 0) {
    CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1));
    CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout_secs_));
  }

  if (!dns_servers_.empty()) {
    CURL_RETURN_NOT_OK(curl_easy_setopt(curl_, CURLOPT_DNS_SERVERS, dns_servers_.c_str()));
  }

  CURL_RETURN_NOT_OK(curl_easy_perform(curl_));
  long val; // NOLINT(*) curl wants a long
  CURL_RETURN_NOT_OK(curl_easy_getinfo(curl_, CURLINFO_NUM_CONNECTS, &val));
  num_connects_ = static_cast<int>(val);

  CURL_RETURN_NOT_OK(curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &val));
  if (val < 200 || val >= 300) {
    return Error(kRemoteError, "HTTP " + val);
  }
  return kOk;
}

