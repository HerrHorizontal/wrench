/**
 * Copyright (c) 2017-2018. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef WRENCH_CLOUDSERVICEMESSAGE_H
#define WRENCH_CLOUDSERVICEMESSAGE_H

#include <vector>

#include "wrench/services/compute/ComputeServiceMessage.h"

namespace wrench {

    class ComputeService;

    /***********************/
    /** \cond INTERNAL     */
    /***********************/

    /**
     * @brief Top-level CloudServiceMessage class
     */
    class CloudServiceMessage : public ComputeServiceMessage {
    protected:
        CloudServiceMessage(const std::string &name, double payload);
    };

    /**
     * @brief CloudServiceGetExecutionHostsRequestMessage class
     */
    class CloudServiceGetExecutionHostsRequestMessage : public CloudServiceMessage {
    public:
        CloudServiceGetExecutionHostsRequestMessage(const std::string &answer_mailbox, double payload);

        /** @brief The mailbox to which a reply should be sent */
        std::string answer_mailbox;
    };

    /**
     * @brief CloudServiceGetExecutionHostsAnswerMessage class
     */
    class CloudServiceGetExecutionHostsAnswerMessage : public CloudServiceMessage {
    public:
        CloudServiceGetExecutionHostsAnswerMessage(std::vector<std::string> &execution_hosts, double payload);

        /** @brief The list of execution hosts */
        std::vector<std::string> execution_hosts;
    };

    /**
     * @brief CloudServiceCreateVMRequestMessage class
     */
    class CloudServiceCreateVMRequestMessage : public CloudServiceMessage {
    public:
        CloudServiceCreateVMRequestMessage(const std::string &answer_mailbox,
                                           const std::string &pm_hostname,
                                           const std::string &vm_hostname,
                                           bool supports_standard_jobs,
                                           bool supports_pilot_jobs,
                                           unsigned long num_cores,
                                           double ram_memory,
                                           std::map<std::string, std::string> &plist,
                                           double payload);

        std::string pm_hostname;
        std::string vm_hostname;
        bool supports_standard_jobs;
        bool supports_pilot_jobs;
        unsigned long num_cores;
        double ram_memory;
        std::map<std::string, std::string> plist;
        std::string answer_mailbox;
    };

    /**
     * @brief CloudServiceCreateVMAnswerMessage class
     */
    class CloudServiceCreateVMAnswerMessage : public CloudServiceMessage {
    public:
        CloudServiceCreateVMAnswerMessage(bool success, double payload);
        /** @brief Whether the VM creation was successful or not */
        bool success;
    };

    /***********************/
    /** \endcond           */
    /***********************/
}

#endif //WRENCH_CLOUDSERVICEMESSAGE_H
