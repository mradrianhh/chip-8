#ifndef CORE_LOADER_H
#define CORE_LOADER_H

#include "logger/logger.h"

/// @brief Initializes the loader.
/// @details Sets up logging and configures static variables.
void core_InitializeLoader(LogLevel log_level);

/// @brief Destroys memory held by the loader.
/// @details Destroys the logger and other variables that are manually allocated.
void core_DestroyLoader();

/// @brief  Loads ELF32 file into memory region.
/// @details Loads an ELF32 file and writes its data into region pointed to by 'region'.
/// If the size of the data loaded from the file exceeds the size of the region, an error is thrown.
/// @param filename name of file to load.
/// @param region memory region to load data into.
/// @param region_size used to prevent segmentation fault.
/// @return address of entry point if applicable.
uint32_t core_LoadElf32File(const char *filename, uint8_t *region, size_t region_size);

/// @brief Loads binary file into memory region.
/// @details Loads a binary file and writes it data into region pointed to by 'region'.
/// @param filename name of file to load.
/// @param region memory region to load data into.
/// @param offset offset into region from where we will start loading data.
/// @param region_size used to prevent segmentation fault.
/// @return end of memory range we loaded into.
uint16_t core_LoadBinary16File(const char *filename, uint8_t *region, uint16_t offset, size_t region_size);

/// @brief Loads binary data into memory region.
/// @details Loads binary data provided in 'data' into region pointed to by 'region'.
/// @param region memory region to load data into.
/// @param offset offset into region from where we will start loading data.
/// @param region_size used to prevent segmentation fault.
/// @param data data to load into memory region.
/// @param data_size size of data to load.
/// @return end of memory range we loaded into.
uint16_t core_LoadBinary16Data(uint8_t *region, uint16_t offset, size_t region_size, uint8_t *data, size_t data_size);

#endif
