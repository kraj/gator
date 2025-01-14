/* Copyright (C) 2021-2022 by Arm Limited. All rights reserved. */

#include "logging/global_log.h"

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <ostream>

namespace logging {
    namespace {
        inline void output_item(bool verbose,
                                char const * level,
                                thread_id_t tid,
                                log_timestamp_t const & timestamp,
                                source_loc_t const & location,
                                std::string_view const & message)
        {
            constexpr double to_ns = 1e-9;
            constexpr int pref_precision = 7;

            if (!verbose) {
                std::cerr << message << std::endl;
            }
            else {
                auto now_ns = double(timestamp.seconds) + (to_ns * double(timestamp.nanos));

                std::cerr << std::fixed << std::setprecision(pref_precision) << "[" << now_ns << "] " << level << " #"
                          << pid_t(tid) << " (" << location.file_name() << ":" << location.line_no() << "): " << message
                          << std::endl;
            }
        }
    }

    global_log_sink_t::global_log_sink_t()
    {
        // disable buffering of output
        ::setvbuf(stdout, nullptr, _IONBF, 0);
        ::setvbuf(stderr, nullptr, _IONBF, 0);
        // make sure that everything goes to output immediately
        std::cout << std::unitbuf;
        std::cerr << std::unitbuf;
    }

    void global_log_sink_t::log_item(thread_id_t tid,
                                     log_level_t level,
                                     log_timestamp_t const & timestamp,
                                     source_loc_t const & location,
                                     std::string_view message)
    {
        // writing to the log must be serialized in a multithreaded environment
        std::lock_guard lock {mutex};

        // special handling for certain log levels
        switch (level) {
            case log_level_t::trace:
                if (output_debug) {
                    output_item(true, "TRACE:", tid, timestamp, location, message);
                }
                break;
            case log_level_t::debug:
                if (output_debug) {
                    output_item(true, "DEBUG:", tid, timestamp, location, message);
                }
                break;
            case log_level_t::info:
                output_item(output_debug, "INFO: ", tid, timestamp, location, message);
                break;
            case log_level_t::warning:
                output_item(output_debug, "WARN: ", tid, timestamp, location, message);
                break;
            case log_level_t::setup:
                // append it to the setup log
                setup_messages.append(message).append("|");
                if (output_debug) {
                    output_item(true, "SETUP:", tid, timestamp, location, message);
                }
                break;
            case log_level_t::error:
                // store the last error message
                last_error = std::string(message);
                output_item(output_debug, "ERROR:", tid, timestamp, location, message);
                break;
            case log_level_t::fatal:
                // store the last error message
                last_error = std::string(message);
                output_item(output_debug, "FATAL:", tid, timestamp, location, message);
                break;
            case log_level_t::child_stdout:
                if (output_debug) {
                    output_item(output_debug, "STDOU:", tid, timestamp, location, message);
                }
                // always output to cout, regardless of whether the cerr log was also output
                std::cout << message;
                break;
            case log_level_t::child_stderr:
                if (output_debug) {
                    output_item(output_debug, "STDER:", tid, timestamp, location, message);
                }
                else {
                    std::cerr << message;
                }
                break;
        }
    }
}
