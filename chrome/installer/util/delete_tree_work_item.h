// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_
#pragma once

#include <windows.h>

#include <string>

#include "base/file_path.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that recursively deletes a file system hierarchy at the
// given root path. The file system hierarchy could be a single file, or a
// directory.
// The file system hierarchy to be deleted can have a key file. If the key file
// is specified, deletion will be performed only if the key file is not in use.
class DeleteTreeWorkItem : public WorkItem {
 public:
  virtual ~DeleteTreeWorkItem();

  virtual bool Do();

  virtual void Rollback();

 private:
  friend class WorkItem;

  // Get a backup path that can keep root_path_ or key_path_
  bool GetBackupPath(const FilePath& for_path, FilePath* backup_path);

  DeleteTreeWorkItem(const std::wstring& root_path,
                     const std::wstring& key_path);

  // Root path to delete.
  FilePath root_path_;

  // Path to the key file. If the key file is specified, deletion will be
  // performed only if the key file is not in use.
  FilePath key_path_;

  // The full path in temporary directory that the original root_path_ has
  // been moved to.
  FilePath backup_path_;

  // The full path in temporary directory that the original key_path_ has
  // been moved to.
  FilePath key_backup_path_;
};

#endif  // CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_
