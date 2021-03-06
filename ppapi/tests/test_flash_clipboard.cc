// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_flash_clipboard.h"

#include <algorithm>
#include <vector>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/private/flash_clipboard.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FlashClipboard);

// WriteData() sends an async request to the browser process. As a result, the
// string written may not be reflected by IsFormatAvailable() or ReadPlainText()
// immediately. We need to wait and retry.
const int kIntervalMs = 250;
const int kMaxIntervals = kActionTimeoutMs / kIntervalMs;

TestFlashClipboard::TestFlashClipboard(TestingInstance* instance)
    : TestCase(instance) {
}

void TestFlashClipboard::RunTests(const std::string& filter) {
  RUN_TEST(ReadWritePlainText, filter);
  RUN_TEST(ReadWriteHTML, filter);
  RUN_TEST(ReadWriteRTF, filter);
  RUN_TEST(ReadWriteMultipleFormats, filter);
  RUN_TEST(Clear, filter);
  RUN_TEST(InvalidFormat, filter);
}

bool TestFlashClipboard::ReadStringVar(
    PP_Flash_Clipboard_Format format,
    std::string* result) {
  pp::Var text;
  bool success = pp::flash::Clipboard::ReadData(
      instance_,
      PP_FLASH_CLIPBOARD_TYPE_STANDARD,
      format,
      &text);
  if (success && text.is_string()) {
    *result = text.AsString();
    return true;
  }
  return false;
}

bool TestFlashClipboard::WriteStringVar(PP_Flash_Clipboard_Format format,
                                        const std::string& text) {
  std::vector<PP_Flash_Clipboard_Format> formats_vector(1, format);
  std::vector<pp::Var> data_vector(1, pp::Var(text));
  bool success = pp::flash::Clipboard::WriteData(
      instance_,
      PP_FLASH_CLIPBOARD_TYPE_STANDARD,
      formats_vector,
      data_vector);
  return success;
}

bool TestFlashClipboard::IsFormatAvailableMatches(
    PP_Flash_Clipboard_Format format,
    bool expected) {
  for (int i = 0; i < kMaxIntervals; ++i) {
    bool is_available = pp::flash::Clipboard::IsFormatAvailable(
        instance_,
        PP_FLASH_CLIPBOARD_TYPE_STANDARD,
        format);
    if (is_available == expected)
      return true;

    PlatformSleep(kIntervalMs);
  }
  return false;
}

bool TestFlashClipboard::ReadPlainTextMatches(const std::string& expected) {
  for (int i = 0; i < kMaxIntervals; ++i) {
    std::string result;
    bool success = ReadStringVar(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT, &result);
    if (success && result == expected)
      return true;

    PlatformSleep(kIntervalMs);
  }
  return false;
}

bool TestFlashClipboard::ReadHTMLMatches(const std::string& expected) {
  for (int i = 0; i < kMaxIntervals; ++i) {
    std::string result;
    bool success = ReadStringVar(PP_FLASH_CLIPBOARD_FORMAT_HTML, &result);
    // Markup is inserted around the copied html, so just check that
    // the pasted string contains the copied string.
    if (success && result.find(expected) != std::string::npos)
      return true;

    PlatformSleep(kIntervalMs);
  }
  return false;
}

std::string TestFlashClipboard::TestReadWritePlainText() {
  std::string input = "Hello world plain text!";
  ASSERT_TRUE(WriteStringVar(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT, input));
  ASSERT_TRUE(IsFormatAvailableMatches(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT,
                                       true));
  ASSERT_TRUE(ReadPlainTextMatches(input));

  PASS();
}

std::string TestFlashClipboard::TestReadWriteHTML() {
  std::string input = "Hello world html!";
  ASSERT_TRUE(WriteStringVar(PP_FLASH_CLIPBOARD_FORMAT_HTML, input));
  ASSERT_TRUE(IsFormatAvailableMatches(PP_FLASH_CLIPBOARD_FORMAT_HTML, true));
  ASSERT_TRUE(ReadHTMLMatches(input));

  PASS();
}

std::string TestFlashClipboard::TestReadWriteRTF() {
  std::string rtf_string =
        "{\\rtf1\\ansi{\\fonttbl\\f0\\fswiss Helvetica;}\\f0\\pard\n"
        "This is some {\\b bold} text.\\par\n"
        "}";
  pp::VarArrayBuffer array_buffer(rtf_string.size());
  char *bytes = static_cast<char*>(array_buffer.Map());
  std::copy(rtf_string.data(), rtf_string.data() + rtf_string.size(), bytes);
  std::vector<PP_Flash_Clipboard_Format> formats_vector(1,
      PP_FLASH_CLIPBOARD_FORMAT_RTF);
  std::vector<pp::Var> data_vector(1, array_buffer);
  ASSERT_TRUE(pp::flash::Clipboard::WriteData(
      instance_,
      PP_FLASH_CLIPBOARD_TYPE_STANDARD,
      formats_vector,
      data_vector));

  ASSERT_TRUE(IsFormatAvailableMatches(PP_FLASH_CLIPBOARD_FORMAT_RTF, true));

  pp::Var rtf_result;
  ASSERT_TRUE(pp::flash::Clipboard::ReadData(
        instance_,
        PP_FLASH_CLIPBOARD_TYPE_STANDARD,
        PP_FLASH_CLIPBOARD_FORMAT_RTF,
        &rtf_result));
  ASSERT_TRUE(rtf_result.is_array_buffer());
  pp::VarArrayBuffer array_buffer_result(rtf_result);
  ASSERT_TRUE(array_buffer_result.ByteLength() == array_buffer.ByteLength());
  char *bytes_result = static_cast<char*>(array_buffer_result.Map());
  ASSERT_TRUE(std::equal(bytes, bytes + array_buffer.ByteLength(),
      bytes_result));

  PASS();
}

std::string TestFlashClipboard::TestReadWriteMultipleFormats() {
  std::vector<PP_Flash_Clipboard_Format> formats;
  std::vector<pp::Var> data;
  formats.push_back(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT);
  data.push_back(pp::Var("plain text"));
  formats.push_back(PP_FLASH_CLIPBOARD_FORMAT_HTML);
  data.push_back(pp::Var("html"));
  bool success = pp::flash::Clipboard::WriteData(
      instance_,
      PP_FLASH_CLIPBOARD_TYPE_STANDARD,
      formats,
      data);
  ASSERT_TRUE(success);
  ASSERT_TRUE(IsFormatAvailableMatches(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT,
                                       true));
  ASSERT_TRUE(IsFormatAvailableMatches(PP_FLASH_CLIPBOARD_FORMAT_HTML, true));
  ASSERT_TRUE(ReadPlainTextMatches(data[0].AsString()));
  ASSERT_TRUE(ReadHTMLMatches(data[1].AsString()));

  PASS();
}

std::string TestFlashClipboard::TestClear() {
  std::string input = "Hello world plain text!";
  ASSERT_TRUE(WriteStringVar(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT, input));
  ASSERT_TRUE(IsFormatAvailableMatches(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT,
                                       true));
  bool success = pp::flash::Clipboard::WriteData(
      instance_,
      PP_FLASH_CLIPBOARD_TYPE_STANDARD,
      std::vector<PP_Flash_Clipboard_Format>(),
      std::vector<pp::Var>());
  ASSERT_TRUE(success);
  ASSERT_TRUE(IsFormatAvailableMatches(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT,
                                       false));

  PASS();
}

std::string TestFlashClipboard::TestInvalidFormat() {
  PP_Flash_Clipboard_Format invalid_format =
      static_cast<PP_Flash_Clipboard_Format>(-1);
  ASSERT_FALSE(WriteStringVar(invalid_format, "text"));
  ASSERT_TRUE(IsFormatAvailableMatches(invalid_format, false));
  std::string unused;
  ASSERT_FALSE(ReadStringVar(invalid_format, &unused));

  PASS();
}
