#ifndef AL_ENTRY_POINT_H
#define AL_ENTRY_POINT_H

#include "alfina_engine_application.h"

int main(int argc, const char* argv[])
{
    using namespace al::engine;

    try
    {
        CommandLineParams params{ argv, static_cast<CommandLineParams::size_type>(argc) };
        AlfinaEngineApplication* application = create_application(params);
        application->initialize_components();
        application->run();
        application->terminate_components();
        destroy_application(application);
    }
    catch(std::exception& e)
    {
        // @TODO :  display messge box with exception info
    }

    return 0;
}

#endif
