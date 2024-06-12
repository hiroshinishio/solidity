set(JSON_Install OFF CACHE INTERNAL "")
add_subdirectory(
	${CMAKE_SOURCE_DIR}/deps/nlohmann-json
	EXCLUDE_FROM_ALL
)

