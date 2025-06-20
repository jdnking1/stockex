add_executable(bench order_book_bench.cpp)

target_link_libraries(bench PRIVATE libexchange)