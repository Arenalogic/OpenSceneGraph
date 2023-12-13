# Locate ASIO-headers (http://think-async.com/Asio)
# Created by Stephan Maximilian Huber
FIND_PACKAGE( Boost 1.37 )
IF(Boost_FOUND)
    SET(ASIO_FOUND "YES")
ENDIF()
