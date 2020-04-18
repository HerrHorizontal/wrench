/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "wrench/logging/TerminalOutput.h"

#include <wrench/workflow/failure_causes/ComputeThreadHasDied.h>

WRENCH_LOG_NEW_DEFAULT_CATEGORY(compute_thread_has_died, "Log category for ComputeThreadHasDied");

namespace wrench {

    /** @brief Constructor
     *
     */
    ComputeThreadHasDied::ComputeThreadHasDied() {
    }


    /**
     * @brief Get the human-readable failure message
     * @return the message
     */
    std::string ComputeThreadHasDied::toString() {
        return "A compute thread has died";
    };

};
