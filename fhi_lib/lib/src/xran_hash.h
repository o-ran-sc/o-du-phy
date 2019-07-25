/******************************************************************************
*
*   Copyright (c) 2019 Intel.
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*
*       http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*   See the License for the specific language governing permissions and
*   limitations under the License.
*
*******************************************************************************/


/**
 * @brief Definitions and support functions to process XRAN packet
 * @file xran_hash.h
 * @ingroup group_source_xran
 * @author Intel Corporation
 **/

#ifndef _XRAN_HASH_H
#define _XRAN_HASH_H

#include <rte_common.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>

#define DEFAULT_HASH_FUNC       rte_hash_crc


struct rte_hash *xran_section_init_hash(uint16_t dir, uint16_t cid, int max_num_sections);
void xran_section_free_hash(struct rte_hash *hash);

void xran_section_reset_hash(struct rte_hash *hash);
int xran_section_add_hash(const struct rte_hash *hash, uint16_t section_id, int index);
int xran_section_lookup(const struct rte_hash *hash, uint16_t section_id);
int xran_section_iterate(const struct rte_hash *hash, uint32_t *next);

#endif
