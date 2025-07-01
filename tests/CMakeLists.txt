include(Testing)

add_executable(test "order_book_test.cpp" "order_queue_test.cpp")


target_link_libraries(test PRIVATE libexchange)

AddTests(test)