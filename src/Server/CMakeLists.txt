set(TARGET_NAME GrinNode)

file(GLOB SOURCE_CODE
	"Main.cpp"
	"JSONFactory.cpp"
	"ShutdownManager.cpp"
	"Node/Node.cpp"
	"Node/NodeRestServer.cpp"
	"Node/API/BlockAPI.cpp"
	"Node/API/ChainAPI.cpp"
	"Node/API/HeaderAPI.cpp"
	"Node/API/PeersAPI.cpp"
	"Node/API/ServerAPI.cpp"
	"Node/API/TxHashSetAPI.cpp"
	"Wallet/WalletDaemon.cpp"
	"Wallet/WalletRestServer.cpp"
	"Wallet/API/OwnerGetAPI.cpp"
	"GrinJoin/GrinJoinController.cpp"
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${TARGET_NAME} ${SOURCE_CODE})

add_dependencies(${TARGET_NAME} Infrastructure P2P BlockChain Wallet PoW civetweb)
target_compile_definitions(${TARGET_NAME} PRIVATE ssize_t=SSIZE_T)
target_link_libraries(${TARGET_NAME} Infrastructure P2P BlockChain Wallet PoW civetweb)