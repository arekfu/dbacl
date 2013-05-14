/* 
 * Copyright (C) 2005 Laird Breyer
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
 * 
 * Author:   Laird Breyer <laird@lbreyer.com>
 */

#ifndef HYPEX_H
#define HYPEX_H

typedef struct {
  hash_value_t id;
  weight_t lam[2];
  weight_t ref;
  token_type_t typ;
} PACK_STRUCTS cp_item_t;

typedef struct {
  category_t cat[2];
  hash_bit_count_t max_hash_bits;
  hash_count_t max_tokens;
  token_count_t unique_token_count;
  cp_item_t *hash;
} category_pair_t;

#endif
