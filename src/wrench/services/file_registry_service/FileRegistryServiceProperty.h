/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */


#ifndef WRENCH_FILEREGISTRYPROPERTY_H
#define WRENCH_FILEREGISTRYPROPERTY_H

#include <services/ServiceProperty.h>

namespace wrench {

    class FileRegistryServiceProperty: public ServiceProperty {

    public:
        /** The number of bytes in a request control message sent to the daemon to request a list of file locations **/
        DECLARE_PROPERTY_NAME(REQUEST_MESSAGE_PAYLOAD);
        /** The number of bytes per file location returned in an answer sent by the daemon to answer a file location request **/
        DECLARE_PROPERTY_NAME(ANSWER_MESSAGE_PAYLOAD);
        /** The number of bytes in the control message sent to the daemon to cause it to remove an entry **/
        DECLARE_PROPERTY_NAME(REMOVE_ENTRY_PAYLOAD);
        /** The overhead, in seconds, of looking up entries for a file **/
        DECLARE_PROPERTY_NAME(LOOKUP_OVERHEAD);
        
    };

};


#endif //WRENCH_FILEREGISTRYPROPERTY_H
