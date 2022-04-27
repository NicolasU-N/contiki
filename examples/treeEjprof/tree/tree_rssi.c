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
 *         Testing the broadcast layer in Rime
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"

#include "dev/button-sensor.h"

#include "dev/leds.h"

#include <stdio.h>

#include "tree_lib.h"


//Tabla de padre preferido


MEMB(preferred_parent_mem, struct preferred_parent, 10);
LIST(preferred_parent_list);

/*---------------------------------------------------------------------------*/
PROCESS(example_broadcast_process, "Broadcast example");
PROCESS( proceso_timer , "Proceso_corre_timer" );
PROCESS( proceso_posteamos , "proceso_posteamos" );
AUTOSTART_PROCESSES(&example_broadcast_process, &proceso_timer, &proceso_posteamos);

/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  uint16_t last_rssi;

  last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

  void *msg = packetbuf_dataptr(); //msg que llego

  struct beacon b_recv = *( (struct beacon*) msg );



  struct preferred_parent *p; // Para recorrer la lista de posibles padres
  struct preferred_parent *in_l; // in to list

  //Revisar si ya conozco este posible padre
  for(p = list_head(preferred_parent_list); p != NULL; p = list_item_next(p))
  {
     // We break out of the loop if the address of the neighbor matches
    //    the address of the neighbor from which we received this
    //    broadcast message.
     if(linkaddr_cmp(&p->id, &b_recv.id)) {
       break;
     }
  }


  //Si no conocia este posible padre
  if(p == NULL)
  {
    //ADD to the list
    in_l = memb_alloc(&preferred_parent_mem);
    if(in_l == NULL) {            // If we could not allocate a new entry, we give up.
      printf("ERROR: we could not allocate a new entry for <<preferred_parent_list>> in tree_rssi\n");
    }else
    {
        //Guardo los campos del mensaje
        in_l->id       = b_recv.id; // Guardo el id del nodo
        //rssi_ac es el rssi del padre + el rssi del enlace al padre
        in_l->rssi_a  = b_recv.rssi_p + last_rssi; // Guardo del rssi acumulado. El rssi acumulado es el rssi divulgado por el nodo (rssi_path) + el rssi medido del beacon que acaba de llegar (rss)
        list_push(preferred_parent_list,in_l); // Add an item to the start of the list.
        printf("beacon added to list: id = %d rssi_a = %d\n", in_l->id.u8[0], in_l->rssi_a);

    }
  }else // Si el padre era conocido actualizo su rssi
  {
    p->rssi_a = b_recv.rssi_p + last_rssi; // Guardo del rssi. El rssi es igual al rssi_path + rssi del broadcast
    printf("beacon updated to list with RSSI_A = %d\n", p->rssi_a);

  }


  printf("BEACON_RECV NODE_ID = %d.%d RSSI_A = %d\n", b_recv.id.u8[0], b_recv.id.u8[1], b_recv.rssi_p);

  printf("A_NODE_ID = %d.%d  RSSI = %d \n", from->u8[0], from->u8[1], last_rssi);

  printf("broadcast message received from %d.%d: '%s'\n",
         from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

  process_post(&proceso_posteamos, PROCESS_EVENT_CONTINUE, NULL);
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(proceso_posteamos, ev, data)
{

  PROCESS_BEGIN();

  struct preferred_parent *p; // Para recorrer la lista de posibles padres

  while(1)
  {

    PROCESS_WAIT_EVENT();

    printf("Corriendo proceso_posteamos\n" );


    //recorrer la LISTA

    printf("-------\n");
    for(p = list_head(preferred_parent_list); p != NULL; p = list_item_next(p))
    {
      printf("LISTA ID=%d RSSI_A = %d \n", p->id.u8[0], p->rssi_a);

    }
    printf("-------\n");

  }

  PROCESS_END();


}

/*---------------------------------------------------------------------------*/


PROCESS_THREAD(proceso_timer, ev, data)
{

  static struct etimer et;


  PROCESS_BEGIN();

  while(1)
  {

    etimer_set(&et, CLOCK_SECOND * 2);

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    printf("Corriendo proceso_timer \n" );


  }


  PROCESS_END();


}

/*---------------------------------------------------------------------------*/

PROCESS_THREAD(example_broadcast_process, ev, data)
{
  static struct etimer et;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  struct beacon b;

  broadcast_open(&broadcast, 129, &broadcast_call);

  while(1) {

    /* Delay 2-4 seconds */
    etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // Llenar el beacon con info
    //b.id.u8[0] = linkaddr_node_addr.u8[0];
    //b.id.u8[1] = linkaddr_node_addr.u8[1];
    //b.rssi_a   = -10;

    llenar_beacon(&b, linkaddr_node_addr, -10);



    packetbuf_copyfrom(&b, sizeof( struct beacon ));
    broadcast_send(&broadcast);
    printf("broadcast message sent\n");

    printf_hello();



    printf("NODE ID = %d.%d\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]  );
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
