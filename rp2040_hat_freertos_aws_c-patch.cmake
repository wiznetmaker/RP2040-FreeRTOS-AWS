# CMake minimum required version
cmake_minimum_required(VERSION 3.12)

# Find git
find_package(Git)

if(NOT Git_FOUND)
	message(FATAL_ERROR "Could not find 'git' tool for RP2040-HAT-FREERTOS-AWS-C patching")
endif()

message("RP2040-HAT-FREERTOS-AWS-C patch utils found")

set(RP2040_HAT_FREERTOS_AWS_C_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(AWS_IOT_DEVICE_SDK_EMBEDDED_C_SRC_DIR "${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR}/libraries/aws-iot-device-sdk-embedded-C")
set(AWS_IOT_DEVICE_SDK_EMBEDDED_C_COREHTTP_SRC_DIR "${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR}/libraries/aws-iot-device-sdk-embedded-C/libraries/standard/coreHTTP")
set(AWS_IOT_DEVICE_SDK_EMBEDDED_C_COREMQTT_SRC_DIR "${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR}/libraries/aws-iot-device-sdk-embedded-C/libraries/standard/coreMQTT")
set(AWS_IOT_DEVICE_SDK_EMBEDDED_C_COREHTTP_HTTP_PARSER_SRC_DIR "${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR}/libraries/aws-iot-device-sdk-embedded-C/libraries/standard/coreHTTP/source/dependency/3rdparty/http_parser")
set(IOLIBRARY_DRIVER_SRC_DIR "${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR}/libraries/ioLibrary_Driver")
set(MBEDTLS_SRC_DIR "${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR}/libraries/mbedtls")
set(PICO_EXTRAS_SRC_DIR "${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR}/libraries/pico-extras")
set(PICO_SDK_SRC_DIR "${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR}/libraries/pico-sdk")
set(PICO_SDK_TINYUSB_SRC_DIR "${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR}/libraries/pico-sdk/lib/tinyusb")
set(RP2040_HAT_AWS_C_PATCH_DIR "${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR}/patches")

# Delete untracked files in FreeRTOS-Kernel
if(EXISTS "${FREERTOS_KERNEL_SRC_DIR}/.git")
	message("cleaning FreeRTOS-Kernel...")
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${FREERTOS_KERNEL_SRC_DIR} clean -fdx)
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${FREERTOS_KERNEL_SRC_DIR} reset --hard)
	message("FreeRTOS-Kernel cleaned")
endif()

# Delete untracked files in ioLibrary_Driver
if(EXISTS "${IOLIBRARY_DRIVER_SRC_DIR}/.git")
	message("cleaning ioLibrary_Driver...")
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${IOLIBRARY_DRIVER_SRC_DIR} clean -fdx)
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${IOLIBRARY_DRIVER_SRC_DIR} reset --hard)
	message("ioLibrary_Driver cleaned")
endif()

# Delete untracked files in mbedtls
if(EXISTS "${MBEDTLS_SRC_DIR}/.git")
	message("cleaning mbedtls...")
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${MBEDTLS_SRC_DIR} clean -fdx)
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${MBEDTLS_SRC_DIR} reset --hard)
	message("mbedtls cleaned")
endif()

# Delete untracked files in pico-extras
if(EXISTS "${PICO_EXTRAS_SRC_DIR}/.git")
	message("cleaning pico-extras...")
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${PICO_EXTRAS_SRC_DIR} clean -fdx)
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${PICO_EXTRAS_SRC_DIR} reset --hard)
	message("pico-extras cleaned")
endif()

# Delete untracked files in pico-sdk
if(EXISTS "${PICO_SDK_SRC_DIR}/.git")
	message("cleaning pico-sdk...")
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${PICO_SDK_SRC_DIR} clean -fdx)
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${PICO_SDK_SRC_DIR} reset --hard)
	message("pico-sdk cleaned")
endif()

execute_process(COMMAND ${GIT_EXECUTABLE} -C ${RP2040_HAT_FREERTOS_AWS_C_SRC_DIR} submodule update --init)

# Delete untracked files in tinyusb
if(EXISTS "${PICO_SDK_TINYUSB_SRC_DIR}/.git")
	message("cleaning pico-sdk tinyusb...")
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${PICO_SDK_TINYUSB_SRC_DIR} clean -fdx)
	execute_process(COMMAND ${GIT_EXECUTABLE} -C ${PICO_SDK_TINYUSB_SRC_DIR} reset --hard)
	message("pico-sdk tinyusb cleaned")
endif()

execute_process(COMMAND ${GIT_EXECUTABLE} -C ${PICO_SDK_SRC_DIR} submodule update --init)

