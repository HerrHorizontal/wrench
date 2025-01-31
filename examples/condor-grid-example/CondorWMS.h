/**
 * Copyright (c) 2020. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_CONDORWMS_H
#define WRENCH_CONDORWMS_H

#include <wrench-dev.h>


namespace wrench {

    class Simulation;

    /**
     *  @brief A Workflow Management System (WMS) implementation (inherits from WMS)
     */
    class CondorWMS : public WMS {

    public:
        // Constructor
        CondorWMS(const std::set<std::shared_ptr<wrench::ComputeService>> &compute_services,
                  const std::set<std::shared_ptr<wrench::StorageService>> &storage_services,
                  std::string hostname);

    protected:

        // Overridden method
        void processEventStandardJobCompletion(std::shared_ptr<StandardJobCompletedEvent>) override;
        /**
        void processEventStandardJobFailure(std::shared_ptr<StandardJobFailedEvent>) override;
        **/
    private:
        // main() method of the WMS
        int main() override;

    };
}

#endif //WRENCH_CONDORWMS_H
