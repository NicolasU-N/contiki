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
//#include "lib/list.h"
//#include "lib/memb.h"
//#include "dev/button-sensor.h"
//#include "dev/leds.h"
#include <stdio.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define NEG_INF           -999
//#define RSSI_NODO_PERDIDO -400
#define PARENT_TIMEOUT  40 * CLOCK_SECOND

#define STABLE_TIME_SEND_ROUTING  20 //sec 12
#define STABLE_TIME_SEND_PKG      30 //sec 12

#define U_DATA_ATTR     0x01
#define U_CONTROL_ATTR  0x02

#define ORIGIN  7
#define DEST    12
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
  linkaddr_t preferred_parent;   // Node id
  signed int rssi_p; // rssi acumulado del padre
  char tree_ch[128]; //arbol codificado
};

struct preferred_parent_st
{
  preferred_parent_st *next;
  linkaddr_t id;   // Node id
  signed int rssi_a; // rssi acumulado
  struct ctimer ctimer;
};

struct list_unicast_msg_st
{
  struct list *next;
  char tree_ch[32];   // Node id
};

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void llenar_beacon(beacon_st *b, linkaddr_t id, uint16_t rssi_p);

#endif /* TREE_LIB_H */
