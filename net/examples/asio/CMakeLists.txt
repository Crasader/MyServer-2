add_executable(asio_chat_server chat/server.cpp)
target_link_libraries(asio_chat_server net)

add_executable(asio_chat_client chat/client.cpp)
target_link_libraries(asio_chat_client net)


