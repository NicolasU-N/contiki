/*
 * nary_tree.c
 *
 *  Created on: 26/04/2022
 *      Author: Nicolas
 */

#include "nary_tree.h"

extern char *ptr;

void init_nodes(tree_node_st *n[], int num_nodes)
{
    int j = 1;
    for (int i = 0; i < num_nodes; i++)
    {
        n[i] = new_node(j++);
    }
}

tree_node_st *new_node(int id)
{
    tree_node_st *new_node = malloc(sizeof(tree_node_st));
    if (new_node)
    {
        new_node->sibling = NULL;
        new_node->child = NULL;
        new_node->id = id;
        return new_node;
    }
    else
        return NULL;
}

tree_node_st *add_child(tree_node_st *n, tree_node_st *new_n)
{
    if (n == NULL || new_n == NULL)
        return NULL;

    if (n->child)
    {
        return add_sibling(n->child, new_n); // n->child
    }
    else
    {
        n->child = new_n;
        return(n->child); // new_node()
    }
}


tree_node_st *add_sibling(tree_node_st *n, tree_node_st *new_n)
{
    if (n == NULL || new_n == NULL)
        return NULL;

    while (n->sibling)
    {
        // Recorre la lista de hermanos
        if (new_n->id == n->sibling->id)
        {
            //printf("SE REEMPLAZA NODO EXISTENTE");
            new_n->sibling = n->sibling->sibling; // Se agrega el hermano actual al nuevo nodo
            return (n->sibling = new_n);		  // Reemplaza el nodo existente por el nuevo
        }
        n = n->sibling;
    }
    return (n->sibling = new_n); // new_node()
}


void serialize(tree_node_st * n, char*buf)
{
    if (n == NULL)
        return;

    tree_node_st *c,*s;
    sprintf (buf, "%c", n->id+58); //ASCII

    if (n->child)
    {
        c= n->child;
        serialize (c, buf + strlen(buf));
    }
    sprintf (buf+strlen(buf), ")");

    if (n->sibling)
    {
        s = n->sibling;
        serialize (s, buf + strlen(buf));
    }
}


int deserialize(tree_node_st *n, char *buf)
{
    if (!ptr)
        ptr = buf;
    //printf("%c",*(ptr));
    //printf("\r\n");
    if (!(*(ptr)) || *(ptr) == ')')
        return 1;

    tree_node_st *c;
    c = add_child(n, new_node(*(ptr)-58));
    while (!deserialize(c, ++ptr)) {}
    return 0;
}


int search_forwarder(tree_node_st *n, int id)
{
    int forwarder = 0;
    //int nodo_init = n->id;

    if(n->id == id)  //Se solicita buscarse a si mismo por lo que se devuelve 0
    {
        return 0;
    }

    if(n->child == NULL)  //Se llega a un nodo hoja sin haber encontrado el nodo
    {
        //printf("No tienes hijos nodo %d, eres un nodo hoja\n", n->id);
        return 0;
    }

    if(n->child->id == id)
    {
        //printf("Tienes suerte, tu, el nodo %d, eres el padre directo del nodo %d\n", n->id, n->child->id);
        return n->child->id;
    }

    n = n->child; //Se analiza desde la perspectiva del hijo directo
    forwarder = search_forwarder(n, id);
    if(forwarder != 0)
    {
        return n->id;
    }
    else
    {
        while(n->sibling)
        {
            if(n->sibling->id == id)
            {
                //printf("Tienes suerte, tu, el nodo %d, eres el padre directo (pero como hermano) del nodo %d\n", nodo_init, n->sibling->id);
                return n->sibling->id;
            }
            forwarder = search_forwarder(n->sibling, id);
            if(forwarder != 0)
            {
                return n->sibling->id;
            }
            n = n->sibling; //Se recorre cada hermano
        }
    }
    return forwarder;
}


void print_node_decendents(tree_node_st *n, int level)
{
    while (n != NULL)
    {
        print_tabs(level);
        printf("tree_node_st: %d\n", n->id);
        if (n->child != NULL)
        {
            // Si tiene hijos
            print_tabs(level);
            printf("Children:\n"); // tree_node_st: %d\n   ,n->child->id
            print_node_decendents(n->child, level + 1);
        }
        n = n->sibling;
    }
}


void print_tabs(int count)
{
    int i;
    for (i = 0; i < count; i++)
    {
        putchar('\t');
    }
}
