/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */


#include <gtest/gtest.h>
#include <wrench-dev.h>
#include <algorithm>

#include "../../include/TestWithFork.h"
#include "../../include/UniqueTmpPathPrefix.h"

#include "../failure_test_util/ResourceRandomRepeatSwitcher.h"


XBT_LOG_NEW_DEFAULT_CATEGORY(bare_metal_compute_service_link_failure_test, "Log category for BareMetalComputeServiceLinkFailureTest");


class BareMetalComputeServiceLinkFailureTest : public ::testing::Test {

public:

    std::shared_ptr<wrench::ComputeService> cs = nullptr;

    void do_ResourceInformationLinkFailure_test();

protected:
    BareMetalComputeServiceLinkFailureTest() {

        // Create the simplest workflow
        workflow = new wrench::Workflow();

        // Create a one-host platform file
        std::string xml = "<?xml version='1.0'?>"
                          "<!DOCTYPE platform SYSTEM \"http://simgrid.gforge.inria.fr/simgrid/simgrid.dtd\">"
                          "<platform version=\"4.1\"> "
                          "   <zone id=\"AS0\" routing=\"Full\"> "
                          "       <host id=\"Host1\" speed=\"1f\" core=\"10\"/> "
                          "       <host id=\"Host2\" speed=\"1f\" core=\"10\"/> "
                          "       <host id=\"Host3\" speed=\"1f\" core=\"10\"/> "
                          "       <link id=\"link1\" bandwidth=\"1Bps\" latency=\"0us\"/>"
                          "       <link id=\"link2\" bandwidth=\"1Bps\" latency=\"0us\"/>"
                          "       <route src=\"Host1\" dst=\"Host2\"> <link_ctn id=\"link1\""
                          "       /> </route>"
                          "       <route src=\"Host1\" dst=\"Host3\"> <link_ctn id=\"link1\""
                          "       /> </route>"
                          "       <route src=\"Host2\" dst=\"Host3\"> <link_ctn id=\"link2\""
                          "       /> </route>"
                          "   </zone> "
                          "</platform>";

        FILE *platform_file = fopen(platform_file_path.c_str(), "w");
        fprintf(platform_file, "%s", xml.c_str());
        fclose(platform_file);
    }

    std::string platform_file_path = UNIQUE_TMP_PATH_PREFIX + "platform.xml";
    wrench::Workflow *workflow;

};

/**********************************************************************/
/**  LINK FAILURE TEST DURING RESOURCE INFORMATION                   **/
/**********************************************************************/

class BareMetalComputeServiceResourceInformationTestWMS : public wrench::WMS {

public:
    BareMetalComputeServiceResourceInformationTestWMS(BareMetalComputeServiceLinkFailureTest *test,
                                                      std::string hostname) :
            wrench::WMS(nullptr, nullptr,  {}, {}, {}, nullptr, hostname, "test") {
        this->test = test;
    }

private:

    BareMetalComputeServiceLinkFailureTest *test;

    int main() {

        // Create a link switcher on/off er
        auto switcher = std::shared_ptr<wrench::ResourceRandomRepeatSwitcher>(
                new wrench::ResourceRandomRepeatSwitcher("Host1", 123, 1, 50, 1, 10,
                                                         "link1", wrench::ResourceRandomRepeatSwitcher::ResourceType::LINK));
        switcher->simulation = this->simulation;
        switcher->start(switcher, true, false); // Daemonized, no auto-restart

        // Do a bunch of resource requests
        unsigned long num_failures = 0;
        unsigned long num_trials = 1000;
        for (unsigned int i=0; i < num_trials; i++) {
            try {
//                WRENCH_INFO("Sleeping for 25 seconds..");
                wrench::Simulation::sleep(25);
                WRENCH_INFO("Requesting resource information..");
                this->test->cs->getPerHostNumCores();
//                WRENCH_INFO("Got it!");
            } catch (wrench::WorkflowExecutionException &e) {
//                WRENCH_INFO("Got an exception");
                num_failures++;
                if (not std::dynamic_pointer_cast<wrench::NetworkError>(e.getCause())) {
                    throw std::runtime_error("Invalid failure cause: " + e.getCause()->toString() + " (was expecting NetworkError");
                }
            }
        }

        WRENCH_INFO("FAILURES %lu / %lu", num_failures, num_trials);

        return 0;
    }
};

TEST_F(BareMetalComputeServiceLinkFailureTest, ResourceInformationTest) {
    DO_TEST_WITH_FORK(do_ResourceInformationLinkFailure_test);
}

void BareMetalComputeServiceLinkFailureTest::do_ResourceInformationLinkFailure_test() {

    // Create and initialize a simulation
    auto simulation = new wrench::Simulation();
    int argc = 1;
    char **argv = (char **) calloc(1, sizeof(char *));
    argv[0] = strdup("storage_service_test");

    simulation->init(&argc, argv);

    // Setting up the platform
    ASSERT_NO_THROW(simulation->instantiatePlatform(platform_file_path));


    this->cs = simulation->add(new wrench::BareMetalComputeService(
            "Host2",
            (std::map<std::string, std::tuple<unsigned long, double>>){
                    std::make_pair("Host2", std::make_tuple(wrench::ComputeService::ALL_CORES, wrench::ComputeService::ALL_RAM)),
            },
            100.0,
            {},
            {
                    {wrench::BareMetalComputeServiceMessagePayload::RESOURCE_DESCRIPTION_ANSWER_MESSAGE_PAYLOAD, 1},
                    {wrench::BareMetalComputeServiceMessagePayload::RESOURCE_DESCRIPTION_REQUEST_MESSAGE_PAYLOAD, 1},
            }));

    auto wms = simulation->add(
            new BareMetalComputeServiceResourceInformationTestWMS(
                    this, "Host1"));

    wms->addWorkflow(workflow);

    simulation->launch();

    delete simulation;

    free(argv[0]);
    free(argv);
}
