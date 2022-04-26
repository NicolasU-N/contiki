/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         CONSTRUCCIÓN DE UN ÁRBOL USANDO EL RSSI
 * \author
 *         >
 */

#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include <stdio.h>
#include "tree_lib.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define PARENT_TIMEOUT 20 * CLOCK_SECOND
#define REMOTE 1
#define NEG_INF -9999
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
struct beacon b;
struct node n;
struct preferred_parent *p; // Para recorrer la lista de posibles padres
struct list_unicast_msg *l;

/*******************************************************************************
 *LISTAS
 ******************************************************************************/
MEMB(preferred_parent_mem, struct preferred_parent, 25); // LISTA ENLAZADA
LIST(preferred_parent_list);

MEMB(unicast_msg_mem, struct list_unicast_msg, 200);
LIST(unicast_msg_list);


/*******************************************************************************
 *PROCESOS
 ******************************************************************************/
PROCESS(send_beacon, "Enviar beacons");
PROCESS(select_parent, "Seleccionar padre");
PROCESS(send_unicast, "Descubrir vecinos");
PROCESS(retx_unicast_msg, "Retrasmitir mensaje unicast");
PROCESS(liberar_lista_padre, "Retrasmitir mensaje unicast");

AUTOSTART_PROCESSES(&send_beacon, &select_parent, &send_unicast, &retx_unicast_msg, &liberar_lista_padre);

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
  {
    void *msg = packetbuf_dataptr(); // msg que llego
    struct beacon b_recv = *((struct beacon *)msg);

    // RSSI
    uint16_t last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    uint16_t total_rssi = b_recv.rssi_p + last_rssi;

    struct preferred_parent *in_l; // in to list

    if (linkaddr_node_addr.u8[0] != 1) // Si no es el nodo raiz
    {
      printf("RSSI recived from NODE ID %d = '%d'. TOTAL RSSI=%d\n", b_recv.id.u8[0], b_recv.rssi_p, total_rssi);

      // LISTA
      // Revisar si ya conozco este posible padre
      for (p = list_head(preferred_parent_list); p != NULL; p = list_item_next(p))
      {
        // We break out of the loop if the address of the neighbor matches
        // the address of the neighbor from which we received this
        // broadcast message.
        if (linkaddr_cmp(&p->id, &b_recv.id))//comparar ID del padre y del hijo
        {
          // YA estaba en la lista ACTUALIZAR
          p->rssi_a = b_recv.rssi_p + last_rssi; // Guardo del rssi. El rssi es igual al rssi_path + rssi del broadcast

          printf("beacon updated to list with RSSI_A = %d\n", p->rssi_a);
          break;
        }
      }

      // Si no conocia este posible padre
      if (p == NULL)
      {
        // ADD to the listcall
        in_l = memb_alloc(&preferred_parent_mem);
        if (in_l == NULL)
        { // If we could not allocate a new entry, we give up.
          printf("ERROR: we could not allocate a new entry for <<preferred_parent_list>> in tree_rssi\n");
        }
        else
        {
          // Guardo los campos del mensaje
          in_l->id = b_recv.id; // Guardo el id del nodo
          // rssi_ac es el rssi del padre + el rssi del enlace al padre
          in_l->rssi_a = b_recv.rssi_p + last_rssi; // Guardo del rssi acumulado. El rssi acumulado es el rssi divulgado por el nodo (rssi_path) + el rssi medido del beacon que acaba de llegar (rss)

          list_push(preferred_parent_list, in_l); // Add an item to the start of the list.
          printf("beacon added to list: id = %d rssi_a = %d\n", in_l->id.u8[0], in_l->rssi_a);
        }
      }
    }
    // printf("BEACON RECIBIDO ID =%d rssi_p=%d\n", from->u8[0], b_recv.rssi_p);
    process_post(&select_parent, PROCESS_EVENT_CONTINUE, NULL);
  }
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static void unicast_recv(struct unicast_conn *c, const linkaddr_t *from)
{
  void *msg = packetbuf_dataptr(); // msg que llego
  linkaddr_t uc_recv = *((linkaddr_t *)msg);
  printf("received from id=%d\n", uc_recv.u8[0]);

  if (linkaddr_node_addr.u8[0] != 1) // Si no es el nodo raiz
  {
    l = memb_alloc(&unicast_msg_mem);

    if (l != NULL)
    {
      list_add(unicast_msg_list, l);
      linkaddr_copy(&l->id, &uc_recv); // Set message id
          }
    process_post(&retx_unicast_msg, PROCESS_EVENT_CONTINUE, NULL);
  }
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static const struct broadcast_callbacks broadcast_callbacks = {broadcast_recv};
static struct broadcast_conn broadcast;

static const struct unicast_callbacks unicast_callbacks = {unicast_recv};
static struct unicast_conn unicast;

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(select_parent, ev, data)
{
  PROCESS_BEGIN();

  struct preferred_parent *p; // Para recorrer la lista de posibles padres
  uint16_t lowest_rssi;
  linkaddr_t new_parent;

  while (1)
  {
    PROCESS_WAIT_EVENT();

    if (linkaddr_node_addr.u8[0] != 1) // Si no es el nodo raiz
    {
      if (list_length(preferred_parent_list) >= 1)
      {
        // recorrer la LISTA
        // Find the lowest of the list
        // Assume that the first one is the lowest
        printf("-------\n");
        for (p = list_head(preferred_parent_list), lowest_rssi = p->rssi_a, linkaddr_copy(&new_parent, &p->id);
             p != NULL; p = list_item_next(p))
        {
          printf("LISTA ID=%d RSSI_A = %d \n", p->id.u8[0], p->rssi_a);
          if (p->rssi_a > lowest_rssi)
          {
            lowest_rssi = p->rssi_a;
            printf("lowest_rssi = %d \n", lowest_rssi);
            linkaddr_copy(&new_parent, &p->id);
          }
          printf("-------\n");
        }

        // update_parent
        printf("#L %d 0\n", n.preferred_parent.u8[0]); // 0: old parent
        linkaddr_copy(&n.preferred_parent, &new_parent);
        printf("#L %d 1\n", n.preferred_parent.u8[0]); //: 1: new parent

        n.rssi_p = lowest_rssi;
      }
    }

  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(send_unicast, ev, data)
{
  static struct etimer et;

  PROCESS_EXITHANDLER(unicast_close(&unicast);)

  PROCESS_BEGIN();

  unicast_open(&unicast, 146, &unicast_callbacks);

  while (1)
  {
    etimer_set(&et, CLOCK_SECOND * 2 + random_rand() % (CLOCK_SECOND * 4));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    packetbuf_copyfrom(&b.id, sizeof(linkaddr_t)); // beacon id

    if (!linkaddr_cmp(&n.preferred_parent, &linkaddr_null)) // el padre del nodo
    {
      unicast_send(&unicast, &n.preferred_parent);
    }
  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(liberar_lista_padre, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  while (1)
  {
    etimer_set(&et, CLOCK_SECOND * 60);

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    if(list_length(preferred_parent_list) > 1){
      list_chop(preferred_parent_list);

      //liberar memoria de la lista
    }


  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(retx_unicast_msg, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&unicast);)

  PROCESS_BEGIN();

  unicast_open(&unicast, 146, &unicast_callbacks);

  while (1)
  {
    PROCESS_YIELD();

    if (!linkaddr_cmp(&n.preferred_parent, &linkaddr_null))
    {
      l = list_head(unicast_msg_list);
      packetbuf_copyfrom(&l->id, sizeof(linkaddr_t));
      unicast_send(&unicast, &n.preferred_parent);
      list_remove(unicast_msg_list, l);
      memb_free(&unicast_msg_mem, l);
    }
  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/

PROCESS_THREAD(send_beacon, ev, data)
{
  static struct etimer et;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  broadcast_open(&broadcast, 129, &broadcast_callbacks);

  if (linkaddr_node_addr.u8[0] == 1)
  {
    n.rssi_p = 0;
  }
  else
  {
    n.rssi_p = NEG_INF;
  }

  while (1)
  {
    /* Delay 2-4 seconds */
    etimer_set(&et, CLOCK_SECOND * 2 + random_rand() % (CLOCK_SECOND * 4)); // 2 4

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    llenar_beacon(&b, linkaddr_node_addr, n.rssi_p);

    packetbuf_copyfrom(&b, sizeof(struct beacon));
    broadcast_send(&broadcast);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
