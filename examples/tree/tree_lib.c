/*
 * tree_lib.c
 *
 *  Created on: 26/04/2022
 *      Author: Nicolas
 */

#include "tree_lib.h"

void llenar_beacon(struct beacon *b, linkaddr_t id, uint16_t rssi_p)
{
  b->rssi_p = rssi_p;
  linkaddr_copy(&b->id, &id);
}
