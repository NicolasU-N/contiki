/*
 * tree_lib.h
 *
 *  Created on: 26/04/2022
 *      Author: Nicolas
 */

#ifndef TREE_LIB_H
#define TREE_LIB_H

#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"
#include "lib/list.h"
#include "lib/memb.h"
//#include "dev/button-sensor.h"
//#include "dev/leds.h"
#include <stdio.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define NEG_INF           -999
#define RSSI_NODO_PERDIDO -400
#define PARENT_TIMEOUT  60 * CLOCK_SECOND

#define U_DATA_ATTR     0x01
#define U_CONTROL_ATTR  0x02

#define ORIGIN  3
#define DEST    20
/*******************************************************************************
 * Variables
 ******************************************************************************/
typedef struct beacon_st beacon_st;

typedef struct node_st node_st;

typedef struct preferred_parent_st preferred_parent_st;

typedef struct list_unicast_msg_st list_unicast_msg_st;


struct beacon_st
{
  linkaddr_t id;   // Node id
  uint16_t rssi_p; // rssi acumulado del padre
};

struct node_st
{
  linkaddr_t preferred_parent_st;   // Node id
  uint16_t rssi_p; // rssi acumulado del padre
};

struct preferred_parent_st
{
  struct preferred_parent_st *next;
  linkaddr_t id;   // Node id
  signed int rssi_a; // rssi acumulado
  struct ctimer ctimer;
};

struct list_unicast_msg_st
{
  struct list *next;
  linkaddr_t id;   // Node id
};

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void llenar_beacon(struct beacon *b, linkaddr_t id, uint16_t rssi_p);

#endif /* TREE_LIB_H */
