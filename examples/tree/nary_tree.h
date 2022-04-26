/*
 * nary_tree.h
 *
 *  Created on: 26/04/2022
 *      Author: Nicolas
 */

#ifndef NARY_TREE_H_INCLUDED
#define NARY_TREE_H_INCLUDED

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
typedef struct tree_node_st tree_node_st;

struct tree_node_st
{
    int id;
    struct tree_node_st *sibling, *child;
};

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/** \brief
 *
 * \param
 * \param
 * \return
 *
 */
void init_nodes(tree_node_st *n[], int num_nodes);

/** \brief
 *
 * \param
 * \param
 * \return
 *
 */
tree_node_st * new_node(int id);

/** \brief
 *
 * \param
 * \param
 * \return
 *
 */
tree_node_st * add_sibling(tree_node_st * n, tree_node_st * new_n);

/** \brief
 *
 * \param
 * \param
 * \return
 *
 */
tree_node_st * add_child(tree_node_st * n, tree_node_st * new_n);

/** \brief
 *
 * \param
 * \param
 * \return
 *
 */
void serialize(tree_node_st * n, char*buf);

/** \brief
 *
 * \param
 * \param
 * \return
 *
 */
int deserialize(tree_node_st *n, char *buf);

/** \brief
 *
 * \param
 * \param
 * \return
 *
 */
int search_forwarder(tree_node_st * n, int id);  //int

/** \brief
 *
 * \param
 * \param
 * \return
 *
 */
void print_node_decendents(tree_node_st * n, int level);

/** \brief
 *
 * \param
 * \param
 * \return
 *
 */
void print_tabs(int count);

#endif // NARYTREE_H_INCLUDED
