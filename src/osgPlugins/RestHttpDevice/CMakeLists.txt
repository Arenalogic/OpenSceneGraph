INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
    
SET(TARGET_SRC
    connection.cpp
    io_service_pool.cpp
    mime_types.cpp
    reply.cpp
    request_handler.cpp
    request_parser.cpp
    server.cpp
    RestHttpDevice.cpp
    ReaderWriterRestHttpDevice.cpp
)

SET(TARGET_H
    connection.hpp
    header.hpp
    io_service_pool.hpp
    mime_types.hpp
    reply.hpp
    request_handler.hpp
    request_parser.hpp
    request.hpp
    server.hpp
    RestHttpDevice.hpp
)

SET(TARGET_ADDED_LIBRARIES osgPresentation )

#### end var setup  ###
SETUP_PLUGIN(resthttp)
