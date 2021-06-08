// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright (C) 2021 Micron Technology, Inc.
//
// This code is derived from and modifies the LevelDB project.

#include "hse_binding/hse_kvs.h"

#include <hse/hse.h>

#include "leveldb/status.h"

namespace leveldb {

static const int MSG_SIZE = 100;

HseKvs::HseKvs(hse_kvs* handle, const std::string& kvs_name,
               size_t get_buffer_size)
    : kvs_handle_(handle),
      kvs_name_(kvs_name),
      get_buffer_size_(get_buffer_size) {
  get_buffer_ = new char[get_buffer_size];
}

Status HseKvs::Close() {
  hse_err_t err;

  if (kvs_handle_ != nullptr) {
    err = hse_kvdb_kvs_close(kvs_handle_);
    kvs_handle_ = nullptr;

    if (err) {
      char msg[MSG_SIZE];
      return Status::IOError(hse_err_to_string(err, msg, sizeof(msg), NULL));
    }
  }

  return Status::OK();
}

Status HseKvs::Put(const Slice& key, const Slice& value) {
  hse_err_t err;

  err = hse_kvs_put(kvs_handle_, NULL, key.data(), key.size(), value.data(),
                    value.size());
  if (err) {
    char msg[MSG_SIZE];
    return Status::IOError(hse_err_to_string(err, msg, sizeof(msg), NULL));
  }

  return Status::OK();
}

Status HseKvs::Get(const Slice& key, std::string* value) {
  size_t value_size;
  Status s = GetInPlace(key, get_buffer_, get_buffer_size_, &value_size);

  if (s.ok()) {
    value->assign((const char*)get_buffer_, value_size);
  }

  return s;
}

Status HseKvs::GetInPlace(const Slice& key, void* dest, size_t dest_size,
                          size_t* value_size) {
  hse_err_t err;
  bool found;

  err = hse_kvs_get(kvs_handle_, NULL, key.data(), key.size(), &found,
                    get_buffer_, get_buffer_size_, value_size);

  if (err) {
    char msg[MSG_SIZE];
    return Status::IOError(hse_err_to_string(err, msg, sizeof(msg), NULL));
  }

  if (!found) {
    return Status::NotFound(std::strerror(ENOENT));
  }

  return Status::OK();
}

Status HseKvs::Delete(const Slice& key) {
  hse_err_t err;

  err = hse_kvs_delete(kvs_handle_, NULL, key.data(), key.size());

  if (err) {
    char msg[MSG_SIZE];
    return Status::IOError(hse_err_to_string(err, msg, sizeof(msg), NULL));
  }

  return Status::OK();
}

HseKvsCursor* HseKvs::NewCursor(bool reverse) {
  hse_kvs_cursor* kvs_cursor_handle;
  hse_kvdb_opspec os;
  hse_err_t err;

  HSE_KVDB_OPSPEC_INIT(&os);
  if (reverse) {
    os.kop_flags |= HSE_KVDB_KOP_FLAG_REVERSE;
  }

  err = hse_kvs_cursor_create(kvs_handle_, &os, NULL, 0, &kvs_cursor_handle);
  if (err) {
    char msg[MSG_SIZE];
    hse_err_to_string(err, msg, sizeof(msg), NULL);
    std::fprintf(stderr, "cursor create error: %s\n", msg);
    return nullptr;
  }

  HseKvsCursor* cursor = new HseKvsCursor(kvs_cursor_handle);

  return cursor;
}

}  // namespace leveldb
