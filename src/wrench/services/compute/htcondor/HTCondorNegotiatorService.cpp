/**
 * Copyright (c) 2017-2020. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "wrench/logging/TerminalOutput.h"
#include "wrench/services/compute/htcondor/HTCondorCentralManagerServiceMessage.h"
#include "wrench/services/compute/htcondor/HTCondorCentralManagerServiceMessagePayload.h"
#include "wrench/services/compute/htcondor/HTCondorNegotiatorService.h"
#include "wrench/simgrid_S4U_util/S4U_Mailbox.h"
#include "wrench/simgrid_S4U_util/S4U_Simulation.h"
#include "wrench/simulation/Simulation.h"
#include "wrench/workflow/WorkflowTask.h"
#include "wrench/workflow/job/PilotJob.h"
#include "wrench/workflow/job/StandardJob.h"
#include <wrench/workflow/failure_causes/NetworkError.h>
#include <wrench/services/compute/batch/BatchComputeService.h>
#include <wrench/services/compute/bare_metal/BareMetalComputeService.h>

WRENCH_LOG_CATEGORY(wrench_core_htcondor_negotiator, "Log category for HTCondorNegotiator");

namespace wrench {

    /**
     * @brief HELPER method (hack) to get all files at a storage service with last read date
     * @param ss : storage_service
     * @return list of files and their last read date
     */
    std::vector<std::pair<WorkflowFile *, double>> HTCondorNegotiatorService::listAllFilesAtStorageService(std::shared_ptr<StorageService> ss) {
        std::vector<std::pair<WorkflowFile *, double>> list_of_files_at_storage_service;
        for (auto const &fs : ss->file_systems) {
            for (auto const &content : fs.second->content) {
                for (auto const &f : content.second) {
                    list_of_files_at_storage_service.push_back(std::make_pair(f, ss->file_read_dates[f]));
                }
            }
        }
        return list_of_files_at_storage_service;
    }


    /**
     * @brief Constructor
     *
     * @param hostname: the hostname on which to start the service
     * @param startup_overhead: a startup overhead, in seconds
     * @param compute_services: a set of 'child' compute services available to and via the HTCondor pool
     * @param running_jobs: a list of currently running jobs
     * @param pending_jobs: a list of pending jobs
     * @param reply_mailbox: the mailbox to which the "done/failed" message should be sent
     */
    HTCondorNegotiatorService::HTCondorNegotiatorService(
            std::string &hostname,
            double startup_overhead,
            std::set<std::shared_ptr<ComputeService>> &compute_services,
            std::map<std::shared_ptr<WorkflowJob>, std::shared_ptr<ComputeService>> &running_jobs,
            std::vector<std::tuple<std::shared_ptr<WorkflowJob>, std::map<std::string, std::string>>> &pending_jobs,
            std::string &reply_mailbox)
            : Service(hostname, "htcondor_negotiator", "htcondor_negotiator"), reply_mailbox(reply_mailbox),
              compute_services(compute_services), running_jobs(running_jobs), pending_jobs(pending_jobs) {

        this->startup_overhead = startup_overhead;
        this->setMessagePayloads(this->default_messagepayload_values, messagepayload_list);
    }

    /**
     * @brief Destructor
     */
    HTCondorNegotiatorService::~HTCondorNegotiatorService() {
        this->pending_jobs.clear();
    }

    /**
     * @brief Compare the priority between two workflow jobs
     *
     * @param lhs: pointer to a workflow job
     * @param rhs: pointer to a workflow job
     *
     * @return whether the priority of the left-hand-side workflow job is higher
     */
    bool HTCondorNegotiatorService::JobPriorityComparator::operator()(
            std::tuple<std::shared_ptr<WorkflowJob>, std::map<std::string, std::string>> &lhs,
            std::tuple<std::shared_ptr<WorkflowJob>, std::map<std::string, std::string>> &rhs) {
        return std::get<0>(lhs)->getPriority() > std::get<0>(rhs)->getPriority();
    }

    /**
     * @brief Main method of the daemon
     *
     * @return 0 on termination
     */
    int HTCondorNegotiatorService::main() {

        TerminalOutput::setThisProcessLoggingColor(TerminalOutput::COLOR_BLUE);

        WRENCH_INFO("HTCondor Negotiator Service starting on host %s listening on mailbox_name %s",
                    this->hostname.c_str(), this->mailbox_name.c_str());

        std::vector<std::shared_ptr<WorkflowJob>> scheduled_jobs;

        // Simulate some overhead
        S4U_Simulation::sleep(this->startup_overhead);

        // sort jobs by priority
        std::sort(this->pending_jobs.begin(), this->pending_jobs.end(), JobPriorityComparator());

        // Go through the jobs and schedule them if possible
        std::set<wrench::WorkflowFile*> output_files;
        for (auto entry : this->pending_jobs) {

            auto job = std::get<0>(entry);
            auto service_specific_arguments = std::get<1>(entry);
            bool is_standard_job = (std::dynamic_pointer_cast<StandardJob>(job) != nullptr);

            auto target_compute_service = pickTargetComputeService(job, service_specific_arguments);

            if (target_compute_service) {
                job->pushCallbackMailbox(this->reply_mailbox);
                if (auto sjob = std::dynamic_pointer_cast<StandardJob>(job)) {
                    /******************************************************/
                    /** "UGLY" HACK for caching functionality starts here */

                    // identify available storage services on target_compute_service's hosts
                    std::cerr << "Caching hack for job " << sjob->getName().c_str() << std::endl;
                    std::set<std::shared_ptr<wrench::StorageService>> matched_local_storage_services;
                    for (auto const &host : target_compute_service->getHosts()) {
                        for (auto ss: this->simulation->storage_services) {
                            if (host == ss->getHostname()) {
                                matched_local_storage_services.insert(ss);
                                break;
                            }
                        }
                    }
                    // get list of the job's output-files
                    std::cerr << "Identify output-files" << std::endl;
                    
                    output_files.clear();
                    for (auto const &task : sjob->getTasks()) {
                        if (!task->getOutputFiles().empty()) {
                            std::cerr << "Found " << std::to_string(task->getOutputFiles().size()) << " output-files for task " << task->getID().c_str() << std::endl;
                            std::cerr << "Trying to insert into set of " << output_files.size() << "objects" << std::endl;
                            output_files.insert(task->getOutputFiles().begin(), task->getOutputFiles().end()); //! WHY do you segfault???
                            std::cerr << "output-file successfully added" << std::endl;
                        }
                    }
                    
                    std::cerr << "Start file-loop" << std::endl;
                    for (auto &flp : sjob->file_locations) {
                        // make sure to skip output-files
                        bool is_output = false;
                        for (auto const &f : output_files) {
                            if (flp.first == f) {
                                std::cerr << "skip file " << flp.first->getID().c_str() << std::endl;
                                is_output = true;
                            }
                        }
                        if (is_output) continue;
                        // adjust job's file locations map
                        //TODO: try to find file on any storage service
                        for (auto const &ss : matched_local_storage_services) {
                            // Cache input-file, when needed
                            //TODO: cache file on only one storage service
                            if (!ss->lookupFile(flp.first, FileLocation::LOCATION(ss))) {
                                std::cerr << "Couldn't find file " << flp.first->getID().c_str() << " on storage service " << ss->getName().c_str() << std::endl;
                                // Evict input-files as long as there is not enough space left on the cache
                                //TODO: implement also random/FIFO/other eviction policies
                                auto file_list = listAllFilesAtStorageService(ss);
                                std::sort(
                                    file_list.begin(), file_list.end(), 
                                    [](const std::pair<wrench::WorkflowFile*, double> &a, const std::pair<wrench::WorkflowFile*, double> &b){
                                        return a.second < b.second;
                                    }
                                );
                                bool need_evict_file = true;
                                while (need_evict_file) {
                                    for (auto const &fs : ss->file_systems) {
                                        if (fs.second->hasEnoughFreeSpace(flp.first->getSize())) {
                                            need_evict_file = false;
                                            break;
                                        }
                                    }
                                    if (need_evict_file) {
                                        std::cerr << "Need to evict file " << file_list.begin()->first->getID().c_str() << "on storage service " << ss->getName().c_str() << std::endl;
                                        ss->deleteFile(file_list.begin()->first, FileLocation::LOCATION(ss));
                                    }
                                }
                                // Copy input files to have a cached version available for the next task requiring this file
                                bool found_a_location = false;
                                //TODO: identify the most optimal source location
                                for (auto const &source_location : flp.second) {
                                    if (source_location->getStorageService()->lookupFile(flp.first, source_location)) {
                                        StorageService::copyFile(flp.first, source_location, FileLocation::LOCATION(ss));
                                        found_a_location = true;
                                        break;
                                    }
                                }
                                if (!found_a_location) {
                                    throw std::runtime_error("Didn't find the file " + flp.first->getID() + " anywhere");
                                }
                            }
                            std::cerr << "Found file " << flp.first->getID().c_str() << " on storage service " << ss->getName().c_str() << std::endl;
                            flp.second.insert(flp.second.begin(), FileLocation::LOCATION(ss));
                        }
                    }

                    /** HACK ends here                                     */
                    /*******************************************************/
                    target_compute_service->submitStandardJob(sjob, service_specific_arguments);
                } else {
                    auto pjob = std::dynamic_pointer_cast<PilotJob>(job);
                    target_compute_service->submitPilotJob(pjob, service_specific_arguments);
                }
                this->running_jobs.insert(std::make_pair(job, target_compute_service));
                scheduled_jobs.push_back(job);
            }
        }

        // Send the callback to the originator
        try {
            S4U_Mailbox::putMessage(
                    this->reply_mailbox, new NegotiatorCompletionMessage(
                            scheduled_jobs, this->getMessagePayloadValue(
                                    HTCondorCentralManagerServiceMessagePayload::HTCONDOR_NEGOTIATOR_DONE_MESSAGE_PAYLOAD)));
        } catch (std::shared_ptr<NetworkError> &cause) {
            return 1;
        }

        WRENCH_INFO("HTCondorNegotiator Service on host %s cleanly terminating!",
                    S4U_Simulation::getHostName().c_str());
        return 0;

    }

    /**
     * @brief Helper method to pick a target compute service for a job
     * @param job
     * @param service_specific_arguments
     * @return
     */
    std::shared_ptr<ComputeService> HTCondorNegotiatorService::pickTargetComputeService(
            std::shared_ptr<WorkflowJob> job, std::map<std::string,
            std::string> service_specific_arguments) {

        bool is_grid_universe = (service_specific_arguments.find("universe") != service_specific_arguments.end());

        if (is_grid_universe) {
            return pickTargetComputeServiceGridUniverse(job, service_specific_arguments);
        } else {
            return pickTargetComputeServiceNonGridUniverse(job, service_specific_arguments);
        }

    }

    /**
     * @brief Helper method to pick a target compute service for a job for a Grid universe job
     * @param job: job to run
     * @param service_specific_arguments: service-specific arguments
     * @return
     */
    std::shared_ptr<ComputeService> HTCondorNegotiatorService::pickTargetComputeServiceGridUniverse(
            std::shared_ptr<WorkflowJob> job, std::map<std::string,
            std::string> service_specific_arguments) {

        std::set<std::shared_ptr<BatchComputeService>> available_batch_compute_services;

        // Figure out which batch compute services are available
        for (auto const &cs : this->compute_services) {
            if (auto batch_cs = std::dynamic_pointer_cast<BatchComputeService>(cs)) {
                available_batch_compute_services.insert(batch_cs);
            }
        }

        // If none, then grid universe jobs are not allowed
        if (available_batch_compute_services.empty()) {
            throw std::invalid_argument(
                    "HTCondorNegotiatorService::pickTargetComputeServiceGridUniverse(): A grid universe job was submitted, "
                    "but no BatchComputeService is available to the HTCondorComputeService");
        }

        // -N and -c service-specific arguments are required
        if ((service_specific_arguments.find("-N") == service_specific_arguments.end()) or
            (service_specific_arguments.find("-c") == service_specific_arguments.end())) {
            throw std::invalid_argument(
                    "HTCondorNegotiatorService::pickTargetComputeServiceGridUniverse(): A grid universe job must provide -N and -c service-specific arguments");
        }

        // -service service-specific arguments may be required and should point to an existing servuce
        if (service_specific_arguments.find("-service") == service_specific_arguments.end()) {
            if (available_batch_compute_services.size() == 1) {
                service_specific_arguments["-service"] = (*available_batch_compute_services.begin())->getName();
            } else {
                throw std::invalid_argument(
                        "HTCondorNegotiatorService::pickTargetComputeServiceGridUniverse(): a grid universe job must provide a -service service-specific argument since "
                        "the HTCondorComputeService has more than one available BatchComputeService");
            }
        }

        // Find the target batch compute service
        std::shared_ptr<BatchComputeService> target_batch_cs = nullptr;
        for (auto const &batch_cs : available_batch_compute_services) {
            if (batch_cs->getName() == service_specific_arguments["-service"]) {
                target_batch_cs = batch_cs;
                break;
            }
        }
        if (target_batch_cs == nullptr) {
            throw std::invalid_argument("HTCondorNegotiatorService::pickTargetComputeServiceGridUniverse(): "
                                        "-service service-specific argument specifies a batch compute service named '" +
                                        service_specific_arguments["-service"] +
                                        "', but no such service is known to the HTCondorComputeService");
        }

        return target_batch_cs;
    }


    /**
     * @brief Helper method to pick a target compute service for a job for a Non-Grid universe job
     * @param job: job to run
     * @param service_specific_arguments: service-specific arguments
     * @return
     */
    //TODO: coordinate jobs preferably to compute services, where the input-files are already in proximity
    std::shared_ptr<ComputeService> HTCondorNegotiatorService::pickTargetComputeServiceNonGridUniverse(
            std::shared_ptr<WorkflowJob> job, std::map<std::string,
            std::string> service_specific_arguments) {

        std::shared_ptr<BareMetalComputeService> target_cs = nullptr;

        if (std::dynamic_pointer_cast<PilotJob>(job)) {
            throw std::invalid_argument("HTCondorNegotiatorService::pickTargetComputeServiceNonGridUniverse(): "
                                        "Non-Grid universe pilot jobs are currently not supported");
        }
        auto sjob = std::dynamic_pointer_cast<StandardJob>(job);

        // Figure out which batch compute services are available
        //TODO: sort compute services according to a priority map
        for (auto const &cs : this->compute_services) {
            // Only BareMetalComputeServices can be used
            if (not std::dynamic_pointer_cast<BareMetalComputeService>(cs)) {
                continue;
            }
            // If job type is not supported, nevermind (shouldn't happen really)
            if (std::dynamic_pointer_cast<StandardJob>(job) and (not cs->supportsStandardJobs())) {
                continue;
            }

            // If service-specific arguments are provided, for now reject them
            if (not service_specific_arguments.empty()) {
                throw std::invalid_argument("HTCondorNegotiatorService::pickTargetComputeServiceNonGridUniverse(): "
                                            "service-specific arguments for Non-Grid universe jobs are currently not supported");
            }

            bool enough_idle_resources = cs->isThereAtLeastOneHostWithIdleResources(sjob->getMinimumRequiredNumCores(),
                                                                                    sjob->getMinimumRequiredMemory());
#if 0
            // Check on RAM constraints
            auto ram_resources = cs->getPerHostAvailableMemoryCapacity();
            unsigned long max_available_ram_capacity = 0;
            for (auto const &entry: ram_resources) {
                max_available_ram_capacity = std::max<unsigned long>(max_available_ram_capacity, entry.second);
            }
            if (max_available_ram_capacity < sjob->getMinimumRequiredMemory()) {
                continue;
            }

            // Check on idle resources
            auto idle_core_resources = cs->getPerHostNumIdleCores();
            unsigned long max_num_idle_cores = 0;
            for (auto const &entry : idle_core_resources) {
                max_num_idle_cores = std::max<unsigned long>(max_num_idle_cores, entry.second);
            }
            if (max_num_idle_cores < sjob->getMinimumRequiredNumCores()) {
                continue;
            }
#endif
            if (enough_idle_resources) {
                // Return the first appropriate CS we found
                return cs;
            }
        }

        return nullptr;
    }



}



