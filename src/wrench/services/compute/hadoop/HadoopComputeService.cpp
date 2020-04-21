/**
 * Copyright (c) 2017-2019. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <wrench/services/compute/hadoop/MRJob.h>
#include "wrench/services/compute/hadoop/HadoopComputeService.h"
#include "HadoopComputeServiceMessage.h"

#include "wrench/simgrid_S4U_util/S4U_Simulation.h"

#include "wrench/services/compute/ComputeService.h"

#include "wrench/logging/TerminalOutput.h"
#include "HadoopComputeServiceMessage.h"
#include "wrench/simgrid_S4U_util/S4U_Mailbox.h"

WRENCH_LOG_NEW_DEFAULT_CATEGORY(hadoop_compute_servivce, "Log category for Hadoop Compute Service");

namespace wrench {

    /**
     * @brief Constructor
     *
     * @param hostname: the name of the host on which the service should be started
     * @param compute_resources: a set of hostnames
     * @param property_list: a property list ({} means "use all defaults")
     * @param messagepayload_list: a message payload list ({} means "use all defaults")
     */
    HadoopComputeService::HadoopComputeService (
            const std::string &hostname,
            const std::set<std::string> compute_resources,
            std::map<std::string, std::string> property_list,
            std::map<std::string, double> messagepayload_list
    ) :
            Service(hostname,
                    "hadoop",
                    "hadoop") {

        // Set default and specified properties
        this->setProperties(this->default_property_values, std::move(property_list));

        // Set default and specified message payloads
        this->setMessagePayloads(this->default_messagepayload_values, std::move(messagepayload_list));

    }

    /**
     * @brief Stop the compute service - must be called by the stop()
     *        method of derived classes
     */
    void HadoopComputeService::stop() {
        Service::stop();
    }

    /**
     * @brief Synchronously submit a MR job to the service (blocks until the job is finished)
     *
     * @throw std::runtime_error
     */
    void HadoopComputeService::runMRJob(MRJob *job) {

        assertServiceIsUp();

        std::string answer_mailbox = S4U_Mailbox::generateUniqueMailboxName("submit_mr_job");

        //  send a "run a MR job" message to the daemon's mailbox_name
        try {
            S4U_Mailbox::putMessage(this->mailbox_name,
                                    new HadoopComputeServiceRunMRJobRequestMessage(
                                            answer_mailbox,
                                            job,
                                            this->getMessagePayloadValue(
                                                    HadoopComputeServiceMessagePayload::RUN_MR_JOB_REQUEST_MESSAGE_PAYLOAD)));
        } catch (std::shared_ptr<NetworkError> &cause) {
            throw std::runtime_error("HadoopComputeService::runMRJob(): UNEXPECTED NETWORK ERROR");
        }

        // Get the answer
        std::shared_ptr<SimulationMessage> message = nullptr;
        try {
            message = S4U_Mailbox::getMessage(answer_mailbox, this->network_timeout);
        } catch (std::shared_ptr<NetworkError> &cause) {
            throw std::runtime_error("HadoopComputeService::runMRJob(): UNEXPECTED NETWORK ERROR");
        }

        if (auto msg = std::dynamic_pointer_cast<HadoopComputeServiceRunMRJobAnswerMessage>(message)) {
            // If no success, throw an exception
            if (not msg->success) {
                throw std::runtime_error("HadoopComputeService::runMRJob(): UNEXPECTED JOB FAILURE");
            }
        } else {
            throw std::runtime_error(
                    "HadoopComputeService::runMRJob(): Received an unexpected [" + message->getName() + "] message!");
        }

    }

    /**
     * @brief Main method of the daemon
     *
     * @return 0 on termination
     */
    int HadoopComputeService::main() {
        this->state = Service::UP;

        TerminalOutput::setThisProcessLoggingColor(TerminalOutput::COLOR_YELLOW);

        WRENCH_INFO("New HadoopComputeService starting (%s) on %ld hosts",
                    this->mailbox_name.c_str(), this->compute_resources.size());

        /** Main loop **/
        while (this->processNextMessage()) {

        }

        WRENCH_INFO("HadoopComputeService on host %s terminating cleanly!", S4U_Simulation::getHostName().c_str());
        return 0;
    }


    /**
     * @brief Wait for and react to any incoming message
     *
     * @return false if the daemon should terminate, true otherwise
     *
     * @throw std::runtime_error
     */
    bool HadoopComputeService::processNextMessage() {

        S4U_Simulation::computeZeroFlop();

        // Wait for a message
        std::shared_ptr<SimulationMessage> message;
        try {
            message = S4U_Mailbox::getMessage(this->mailbox_name);
        } catch (std::shared_ptr<NetworkError> &error) { WRENCH_INFO(
                    "Got a network error while getting some message... ignoring");
            return true;
        }

        WRENCH_INFO("Got a [%s] message", message->getName().c_str());
        if (auto msg = std::dynamic_pointer_cast<ServiceStopDaemonMessage>(message)) {

            // TODO: Forward the stop request to the executor

            // This is Synchronous
            try {
                S4U_Mailbox::putMessage(msg->ack_mailbox,
                                        new ServiceDaemonStoppedMessage(this->getMessagePayloadValue(
                                                HadoopComputeServiceMessagePayload::DAEMON_STOPPED_MESSAGE_PAYLOAD)));
            } catch (std::shared_ptr<NetworkError> &cause) {
                return false;
            }
            return false;

        } else if (auto msg = std::dynamic_pointer_cast<HadoopComputeServiceRunMRJobRequestMessage>(message)) {

            // If an executor is already running, reject the request
            bool success;

            if (not this->pending_jobs.empty()) {
                success = false;

            } else {

                // TODO: Double check that the job can run (e.g., do we have enough compute resources?)

                // Start a MRJobExecutor service
                this->executor = std::shared_ptr<MRJobExecutor>(new MRJobExecutor(this->hostname,
                                                                                  msg->job,
                                                                                  this->compute_resources,  // Right now, run the job on all resources
                                                                                  this->mailbox_name,
                                                                                  {},
                                                                                  {}));
                this->executor->simulation = this->simulation;
                this->executor->start(this->hostname, true, false);

                this->pending_jobs[msg->job] =
                        std::unique_ptr<PendingJob>(new PendingJob(job, executor, msg->answer_mailbox)));
            }


            if (not success) {
                try {
                    S4U_Mailbox::putMessage(msg->answer_mailbox,
                                            new HadoopComputeServiceRunMRJobAnswerMessage(success,
                                                                                          this->getMessagePayloadValue(
                                                                                                  HadoopComputeServiceMessagePayload::RUN_MR_JOB_ANSWER_MESSAGE_PAYLOAD)));
                } catch (std::shared_ptr<NetworkError> &cause) {
                    return true;
                }
            }

            return true;

        } else if (auto msg = std::dynamic_pointer_cast<MRJobExecutorNotificationMessage>(message)) {

            if (this->pending_jobs.find(msg->job) == this->pending_jobs.end()) {
                throw std::runtime_error("Couldn't find MR Job in pending job list!");
            }

            this->pending_jobs.erase(msg->job);

            try {
                S4U_Mailbox::putMessage(msg->answer_mailbox,
                                        new HadoopComputeServiceRunMRJobAnswerMessage(msg->success,
                                                                                      this->getMessagePayloadValue(
                                                                                              MRJobExecutorMessagePayload::XXX)));
            } catch (std::shared_ptr<NetworkError> &cause) {
                return true;
            }


        } else {

            throw std::runtime_error(
                    "HadoopComputeService::processNextMessage(): Received an unexpected [" + message->getName() + "] message!");
        }
    }



};
