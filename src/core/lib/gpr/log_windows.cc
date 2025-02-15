/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <grpc/support/port_platform.h>

#ifdef GPR_WINDOWS_LOG

#include <stdarg.h>
#include <stdio.h>

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>
#include <grpc/support/log_windows.h>
#include <grpc/support/string_util.h>
#include <grpc/support/time.h>

#include "src/core/lib/gpr/log_internal.h"
#include "src/core/lib/gpr/string.h"
#include "src/core/lib/gpr/string_windows.h"
#include "src/core/lib/gprpp/examine_stack.h"

int gpr_should_log_stacktrace(gpr_log_severity severity);

void gpr_log(const char* file, int line, gpr_log_severity severity,
             const char* format, ...) {
  /* Avoid message construction if gpr_log_message won't log */
  if (gpr_should_log(severity) == 0) {
    return;
  }
  va_list args;
  va_start(args, format);
  gpr_vlog(file, line, severity, format, args);
  va_end(args);
}

void gpr_vlog(const char* file, int line, gpr_log_severity severity,
              const char* format, va_list args) {
  char* message;
  int ret = _vscprintf(format, args);
  if (ret < 0) {
    message = NULL;
  } else {
    /* Allocate a new buffer, with space for the NUL terminator. */
    size_t strp_buflen = (size_t)ret + 1;
    message = (char*)gpr_malloc(strp_buflen);

    /* Print to the buffer. */
    ret = vsnprintf_s(message, strp_buflen, _TRUNCATE, format, args);
    if (GPR_UNLIKELY((size_t)ret != strp_buflen - 1)) {
      /* This should never happen. */
      gpr_free(message);
      message = NULL;
    }
  }

  gpr_log_message(file, line, severity, message);
  gpr_free(message);
}

/* Simple starter implementation */
void gpr_default_log(gpr_log_func_args* args) {
  const char* final_slash;
  const char* display_file;
  char time_buffer[64];
  time_t timer;
  gpr_timespec now = gpr_now(GPR_CLOCK_REALTIME);
  struct tm tm;

  timer = (time_t)now.tv_sec;
  final_slash = strrchr(args->file, '\\');
  if (final_slash == NULL)
    display_file = args->file;
  else
    display_file = final_slash + 1;

  if (localtime_s(&tm, &timer)) {
    strcpy(time_buffer, "error:localtime");
  } else if (0 ==
             strftime(time_buffer, sizeof(time_buffer), "%m%d %H:%M:%S", &tm)) {
    strcpy(time_buffer, "error:strftime");
  }

  absl::optional<std::string> stack_trace =
      gpr_should_log_stacktrace(args->severity)
          ? grpc_core::GetCurrentStackTrace()
          : absl::nullopt;
  if (stack_trace) {
    fprintf(stderr, "%s%s.%09u %5lu %s:%d] %s\n%s\n",
            gpr_log_severity_string(args->severity), time_buffer,
            (int)(now.tv_nsec), GetCurrentThreadId(), display_file, args->line,
            args->message, stack_trace->c_str());
  } else {
    fprintf(stderr, "%s%s.%09u %5lu %s:%d] %s\n",
            gpr_log_severity_string(args->severity), time_buffer,
            (int)(now.tv_nsec), GetCurrentThreadId(), display_file, args->line,
            args->message);
  }
  fflush(stderr);
}

#endif /* GPR_WINDOWS_LOG */
