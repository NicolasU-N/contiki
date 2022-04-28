/*
 * tree_rssi.c
 *
 *  Created on: 26/04/2022
 *      Author: Nicolas
 */

#include "nary_tree.h"
#include "tree_lib.h"
#include "net/netstack.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define REMOTE 1          // To set the transmission Power
#define MY_TX_POWER_DBM 0 // This number is in dBm e.g., "0" "7"  "-24"

/*******************************************************************************
 * Variables
 ******************************************************************************/
char *ptr;

beacon_st b; // beacon struct
node_st n;   // node struct

tree_node_st *tree_node; // tree node struct

preferred_parent_st *p; // lista de posibles padres
list_unicast_msg_st *l;

//---LINKED LISTS
MEMB(preferred_parent_mem, preferred_parent_st, 30);
LIST(preferred_parent_list);
MEMB(unicast_msg_mem, list_unicast_msg_st, 50);
LIST(unicast_msg_list);
//---

PROCESS(send_beacon, "Enviar beacons");
PROCESS(select_parent, "Seleccionar el mejor padre por RSSI");
PROCESS(send_routing_table, "Enviar tabla de enrutamiento");
PROCESS(update_routing_table, "Actualizar tabla de enrutamiento");
PROCESS(generate_pkg, "Generar paquetes al destino");
PROCESS(routing, "Enrutamiento Upstream/Downstream");

// PROCESS(retx_unicast_msg, "Retrasmitir mensaje unicast");
AUTOSTART_PROCESSES(&send_beacon, &select_parent, &send_routing_table, &update_routing_table, &generate_pkg, &routing); //, &retx_unicast_msg

/*******************************************************************************
 * Callbacks
 ******************************************************************************/
static void remove_parent(void *n)
{
  preferred_parent_st *e = n;
  list_remove(preferred_parent_list, e);
  memb_free(&preferred_parent_mem, e);
}

static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  beacon_st b_recv;

  if (linkaddr_node_addr.u8[0] != 1) // Si no es el nodo raiz
  {
    void *msg = packetbuf_dataptr();
    b_recv = *((beacon_st *)msg);

    // RSSI
    signed int last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    signed int total_rssi = b_recv.rssi_p + last_rssi;
    
    //random_init(last_rssi);

    printf("RSSI from NODE ID %d = %d. T RSSI=%d\n", b_recv.id.u8[0], b_recv.rssi_p, total_rssi);

    // LISTA
    // Revisar si ya conozco este posible padre
    for (p = list_head(preferred_parent_list); p != NULL; p = p->next)
    {
      // We break out of the loop if the address of the neighbor matches
      // the address of the neighbor from which we received this
      // broadcast message.
      if (linkaddr_cmp(from, &p->id))
      {
        // YA estaba en la lista ACTUALIZAR
        p->rssi_a = total_rssi; // Guardo del rssi. El rssi es igual al rssi_path + rssi del broadcast
        // printf("beacon updated to list with RSSI_A = %d\n", p->rssi_a);
        ctimer_set(&p->ctimer, PARENT_TIMEOUT, remove_parent, p);
        process_post(&select_parent, PROCESS_EVENT_CONTINUE, NULL);
        return;
      }
    }

    p = memb_alloc(&preferred_parent_mem);
    // Si no conocia este posible padre
    if (p != NULL)
    {
      list_add(preferred_parent_list, p);
      linkaddr_copy(&p->id, &b_recv.id); // Guardo el id del nodo
      p->rssi_a = total_rssi;            // Guardo del rssi acumulado. El rssi acumulado es el rssi divulgado por el nodo (rssi_path) + el rssi medido del beacon que acaba de llegar (rssi)
      ctimer_set(&p->ctimer, PARENT_TIMEOUT, remove_parent, p);
      process_post(&select_parent, PROCESS_EVENT_CONTINUE, NULL);
    }
  }
}

static void unicast_recv(struct unicast_conn *c, const linkaddr_t *from)
{
  packetbuf_attr_t uc_type = packetbuf_attr(PACKETBUF_ATTR_UNICAST_TYPE); // Obtener el tipo de paquete
  void *msg = packetbuf_dataptr();

  if (uc_type == U_CONTROL_ATTR)
  {
    //printf("PKG TYPE CTRL\n");
    char *rcv = (char *)msg; // leer mensaje  *((char *)msg)

    process_post(&update_routing_table, PROCESS_EVENT_CONTINUE, rcv);
  }
  else if (uc_type == U_DATA_ATTR)
  {
    //printf("PKG TYPE DATA\n");
    list_unicast_msg_st uc_recv_data = *((list_unicast_msg_st *)msg); // leer mensaje de datos como lista enlazada

    l = memb_alloc(&unicast_msg_mem);
    if (l != NULL)
    {
      list_add(unicast_msg_list, l);
      strcpy(l->tree_ch, uc_recv_data.tree_ch); // set message
      printf("PKG UNICAST R=%s", uc_recv_data.tree_ch); //l->tree_ch
    }

    process_post(&routing, PROCESS_EVENT_CONTINUE, NULL);
  }

  // printf("msg from id=%d\n", uc_recv.u8[0]);
}

static const struct broadcast_callbacks broadcast_callbacks = {broadcast_recv};
static struct broadcast_conn broadcast;

static const struct unicast_callbacks unicast_callbacks = {unicast_recv};
static struct unicast_conn unicast;

/*******************************************************************************
 * Processes
 ******************************************************************************/

PROCESS_THREAD(send_beacon, ev, data)
{
  static struct etimer et;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

//-------------------------------- RADIO ------------------------------------
// Set Transmission Power in the Zolertia
#if REMOTE
  static radio_value_t val;
  if (NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, MY_TX_POWER_DBM) ==
      RADIO_RESULT_OK)
  {
    NETSTACK_RADIO.get_value(RADIO_PARAM_TXPOWER, &val);
    printf("Transmission Power Set : %d dBm\n", val);
  }
  else if (NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, MY_TX_POWER_DBM) ==
           RADIO_RESULT_INVALID_VALUE)
  {
    printf("ERROR: RADIO_RESULT_INVALID_VALUE\n");
  }
  else
  {
    printf("ERROR: The TX power could not be set\n");
  }
#endif

  broadcast_open(&broadcast, 129, &broadcast_callbacks);

  // Si es el nodo padre
  if (linkaddr_node_addr.u8[0] == 1)
  {
    n.rssi_p = 0;
  }
  else
  {
    n.rssi_p = NEG_INF;
  }

  tree_node = new_node(linkaddr_node_addr.u8[0]);
  random_init(linkaddr_node_addr.u8[0]+1);
  //printf("rand %d",random_rand()+200);

  while (1)
  {
    /* Delay 2-4 seconds */
    etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4)); // 1 3
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // printf("n.preferred_parent.u8 %d\n", n.preferred_parent.u8[0]);
    // printf("TX n.rssi_p %d\n", n.rssi_p);

    llenar_beacon(&b, linkaddr_node_addr, n.rssi_p);
    packetbuf_copyfrom(&b, sizeof(beacon_st));
    broadcast_send(&broadcast);
  }
  PROCESS_END();
}

PROCESS_THREAD(select_parent, ev, data)
{
  PROCESS_BEGIN();

  preferred_parent_st *p;
  uint16_t lowest_rssi;
  linkaddr_t new_parent;

  while (1)
  {
    PROCESS_WAIT_EVENT();

    if (linkaddr_node_addr.u8[0] != 1) // Si no es el nodo raiz
    {
      if (list_length(preferred_parent_list) >= 1)
      {
        // Find the lowest of the list
        // Assume that the first one is the lowest
        // recorrer la LISTA
        printf("-----LISTA \n");
        for (p = list_head(preferred_parent_list), lowest_rssi = p->rssi_a, linkaddr_copy(&new_parent, &p->id); p != NULL; p = list_item_next(p))
        {
          printf("ID=%d RSSI_A= %d\n", p->id.u8[0], p->rssi_a);
          if (lowest_rssi < p->rssi_a)
          {
            lowest_rssi = p->rssi_a;
            linkaddr_copy(&new_parent, &p->id);
          }
        }
        printf("----\n");

        // update_parent
        if (n.preferred_parent.u8[0] != new_parent.u8[0])
        {
          printf("#L %d 0\n", n.preferred_parent.u8[0]); // 0: old parent
          // printf("lowest_rssi = %d \n", lowest_rssi);

          linkaddr_copy(&n.preferred_parent, &new_parent);
          // printf("new_parent = %d \n", new_parent.u8[0]);
          printf("#L %d 1\n", n.preferred_parent.u8[0]); //: 1: new parent
        }
        // update parent rssi
        n.rssi_p = lowest_rssi; // total_rssi + lowest_rssi

        // Eliminar la lista de padres preferidos
      }
    }
  }
  PROCESS_END();
}

PROCESS_THREAD(send_routing_table, ev, data)
{
  static struct etimer et, wait;

  PROCESS_EXITHANDLER(unicast_close(&unicast);)

  PROCESS_BEGIN();

  unicast_open(&unicast, 146, &unicast_callbacks);

  etimer_set(&wait, CLOCK_SECOND * STABLE_TIME_SEND_ROUTING);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&wait));

  while (1)
  {
    etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    serialize(tree_node, n.tree_ch); // Serializar arbol
    // printf("SERIAL=%s\n", n.tree_ch);

    if (!linkaddr_cmp(&n.preferred_parent, &linkaddr_null))
    {
      packetbuf_copyfrom(&n.tree_ch, strlen(n.tree_ch)); // enviar arbol codificado
      packetbuf_set_attr(PACKETBUF_ATTR_UNICAST_TYPE, U_CONTROL_ATTR);
      /*
      if (unicast_send(&unicast, &n.preferred_parent))
      {
        printf("ENVIO UNICAST=%s\n", n.tree_ch);
      }
      */
      unicast_send(&unicast, &n.preferred_parent);
    }
  }
  PROCESS_END();
}

PROCESS_THREAD(update_routing_table, ev, data)
{
  PROCESS_BEGIN();
  while (1)
  {
    PROCESS_YIELD();

    char *rcv = (char *)data;
    printf("data recv=%s\n", rcv);

    deserialize(tree_node, rcv); // Deserializar arbol
    ptr = 0;                     // reset ptr deserializer
    //------------
    //char serial_test[128]; // Verificar que la deserializacion sea correcta
    serialize(tree_node, n.tree_ch);
    printf("ENRU ACT=%s\n", n.tree_ch);
    //------------
  }
  PROCESS_END();
}

PROCESS_THREAD(generate_pkg, ev, data)
{
  static struct etimer et, wait;

  PROCESS_BEGIN();

  etimer_set(&wait, CLOCK_SECOND * STABLE_TIME_SEND_PKG);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&wait));

  while (1)
  {
    etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    if (linkaddr_node_addr.u8[0] == ORIGIN)
    {
      l = memb_alloc(&unicast_msg_mem);
      if (l != NULL)
      {
        list_add(unicast_msg_list, l);
        strcpy(l->tree_ch, "Datos"); // set message
        // printf("MSG=%s\n", l->tree_ch);
      }
      process_post(&routing, PROCESS_EVENT_CONTINUE, NULL);
      // printf("Paquete generado\n");
    }
  }
  PROCESS_END();
}

PROCESS_THREAD(routing, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&unicast);)

  PROCESS_BEGIN();

  unicast_open(&unicast, 146, &unicast_callbacks);

  int send_to = 0;
  linkaddr_t node_dest;
  node_dest.u8[1] = 0;

  while (1)
  {
    PROCESS_YIELD();

    send_to = search_forwarder(tree_node, DEST);
    printf("ENVIAR A =%d\n", send_to);

    node_dest.u8[0] = (unsigned char)send_to;//
    node_dest.u8[1] = 0;

    printf("#A color=orange\n"); // orange

    for (l = list_head(unicast_msg_list); l != NULL; l = list_item_next(l))
    {
      packetbuf_copyfrom(&l, sizeof(list_unicast_msg_st));
      packetbuf_set_attr(PACKETBUF_ATTR_UNICAST_TYPE, U_DATA_ATTR);

      if (linkaddr_node_addr.u8[0] == DEST)
      {
        printf("MSG Recived =%s\n", l->tree_ch);        
        break;
      }
      else
      {
        if (send_to == 0)
        { // enviar al padre
          unicast_send(&unicast, &n.preferred_parent);
        }
        else
        { // enviar al nodo hijo
          unicast_send(&unicast, &node_dest);
        }
      }
    }
  }
  PROCESS_END();
}