/**
 * @file ccnbtreetest.c
 * 
 * Part of ccnr - CCNx Repository Daemon.
 *
 */

/*
 * Copyright (C) 2011 Palo Alto Research Center, Inc.
 *
 * This work is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 * This work is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details. You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
 
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ccn/btree.h>
#include <ccn/charbuf.h>
#include <ccn/hashtb.h>

#define FAILIF(cond) do {} while ((cond) && fatal(__func__, __LINE__))
#define CHKSYS(res) FAILIF((res) == -1)
#define CHKPTR(p)   FAILIF((p) == NULL)

static int
fatal(const char *fn, int lineno)
{
    char buf[80] = {0};
    snprintf(buf, sizeof(buf)-1, "OOPS - function %s, line %d", fn, lineno);
    perror(buf);
    exit(1);
    return(0);
}

/**
 * Use standard mkdtemp() to create a subdirectory of the
 * current working directory, and set the TEST_DIRECTORY environment
 * variable with its name.
 */
static int
test_directory_creation(void)
{
    int res;
    struct ccn_charbuf *dirbuf;
    char *temp;
    
    dirbuf = ccn_charbuf_create();
    CHKPTR(dirbuf);
    res = ccn_charbuf_putf(dirbuf, "./%s", "_bt_XXXXXX");
    CHKSYS(res);
    temp = mkdtemp(ccn_charbuf_as_string(dirbuf));
    CHKPTR(temp);
    res = ccn_charbuf_putf(dirbuf, "/%s", "_test");
    CHKSYS(res);
    res = mkdir(ccn_charbuf_as_string(dirbuf), 0777);
    CHKSYS(res);
    printf("Created directory %s\n", ccn_charbuf_as_string(dirbuf));
    setenv("TEST_DIRECTORY", ccn_charbuf_as_string(dirbuf), 1);
    ccn_charbuf_destroy(&dirbuf);
    return(res);
}

/**
 * Basic tests of ccn_btree_io_from_directory() and its methods.
 *
 * Assumes TEST_DIRECTORY has been set.
 */
static int
test_btree_io(void)
{
    int res;
    struct ccn_btree_node nodespace = {0};
    struct ccn_btree_node *node = &nodespace;
    struct ccn_btree_io *io = NULL;

    /* Open it up. */
    io = ccn_btree_io_from_directory(getenv("TEST_DIRECTORY"));
    CHKPTR(io);
    node->buf = ccn_charbuf_create();
    CHKPTR(node->buf);
    node->nodeid = 12345;
    res = io->btopen(io, node);
    CHKSYS(res);
    FAILIF(node->iodata == NULL);
    ccn_charbuf_putf(node->buf, "smoke");
    res = io->btwrite(io, node);
    CHKSYS(res);
    node->buf->length = 0;
    ccn_charbuf_putf(node->buf, "garbage");
    res = io->btread(io, node, 500000);
    CHKSYS(res);
    FAILIF(node->buf->length != 5);
    FAILIF(node->buf->limit > 10000);
    node->clean = 5;
    ccn_charbuf_putf(node->buf, "r");
    res = io->btwrite(io, node);
    CHKSYS(res);
    node->buf->length--;
    ccn_charbuf_putf(node->buf, "d");
    res = io->btread(io, node, 1000);
    CHKSYS(res);
    FAILIF(0 != strcmp("smoker", ccn_charbuf_as_string(node->buf)));
    node->buf->length--;
    res = io->btwrite(io, node);
    CHKSYS(res);
    node->buf->length = 0;
    ccn_charbuf_putf(node->buf, "garbage");
    node->clean = 0;
    res = io->btread(io, node, 1000);
    CHKSYS(res);
    res = io->btclose(io, node);
    CHKSYS(res);
    FAILIF(node->iodata != NULL);
    FAILIF(0 != strcmp("smoke", ccn_charbuf_as_string(node->buf)));
    res = io->btdestroy(&io);
    CHKSYS(res);
    ccn_charbuf_destroy(&node->buf);
    return(res);
}

/**
 * Helper for test_structure_sizes()
 *
 * Prints out the size of the struct
 */
static void
check_structure_size(const char *what, int sz)
{
    printf("%s size is %d bytes\n", what, sz);
    errno=EINVAL;
    FAILIF(sz % CCN_BT_SIZE_UNITS != 0);
}

/**
 * Helper for test_structure_sizes()
 *
 * Prints the size of important structures, and make sure that
 * they are mutiples of CCN_BT_SIZE_UNITS.
 */
int
test_structure_sizes(void)
{
    check_structure_size("ccn_btree_entry_trailer",
            sizeof(struct ccn_btree_entry_trailer));
    check_structure_size("ccn_btree_internal_entry",
            sizeof(struct ccn_btree_internal_entry));
    return(0);
}

/**
 * Test that the lockfile works.
 */
int
test_btree_lockfile(void)
{
    int res;
    struct ccn_btree_io *io = NULL;
    struct ccn_btree_io *io2 = NULL;

    io = ccn_btree_io_from_directory(getenv("TEST_DIRECTORY"));
    CHKPTR(io);
    /* Make sure the locking works */
    io2 = ccn_btree_io_from_directory(getenv("TEST_DIRECTORY"));
    FAILIF(io2 != NULL || errno != EEXIST);
    errno=EINVAL;
    res = io->btdestroy(&io);
    CHKSYS(res);
    FAILIF(io != NULL);
    return(res);
}

struct entry_example {
    unsigned char p[CCN_BT_SIZE_UNITS];
    struct ccn_btree_entry_trailer t;
};

struct node_example {
    struct ccn_btree_node_header hdr;
    unsigned char ss[CCN_BT_SIZE_UNITS * 2];
    struct entry_example e[3];
} ex1 = {
    {{0x05, 0x3a, 0xde, 0x78}, {1}},
    "goodstuffed",
    {
        {.t={.koff0={0,0,0,3+8}, .ksiz0={0,1}, .entdx={0,0}, .entsz={3}}}, // "d"
        {.t={.koff0={0,0,0,0+8}, .ksiz0={0,9}, .entdx={0,1}, .entsz={3}}}, // "goodstuff"
        {.t={.koff0={0,0,0,2+8}, .ksiz0={0,2}, .entdx={0,2}, .entsz={3},
            .koff1={0,0,0,3+8}, .ksiz1={0,1}}}, // "odd"
    }
};

struct node_example ex2 = {
    {{0x05, 0x3a, 0xde, 0x78}, {1}},
    "struthiomimus",
    {
        {.t={.koff1={0,0,0,2+8}, .ksiz1={0,3}, .entdx={0,0}, .entsz={3}}}, // "rut"
        {.t={.koff0={0,0,0,0+8}, .ksiz0={0,5}, .entdx={0,1}, .entsz={3}}}, // "strut"
        {.t={.koff0={0,0,0,1+8}, .ksiz0={0,5}, .entdx={0,2}, .entsz={3}}}, // "truth"
    }
};

struct root_example {
    struct ccn_btree_node_header hdr;
    unsigned char ss[CCN_BT_SIZE_UNITS];
    struct ccn_btree_internal_entry e[2];
} rootex1 = {
    {{0x05, 0x3a, 0xde, 0x78}, {1}, {'R'}, {1}},
    "ru",
    {
        {   {.magic={0xcc}, .child={0,0,0,2}}, // ex1 at nodeid 2 as 1st child
            {.entdx={0,0}, .level={1}, .entsz={3}}}, 
        {   {.magic={0xcc}, .child={0,0,0,3}}, // ex2 at nodeid 3 as 2nd child
            {.koff1={0,0,0,0+8}, .ksiz1={0,2}, 
                .entdx={0,1}, .level={1}, .entsz={3}}},
    }
};

int
test_btree_chknode(void)
{
    int res;
    struct ccn_btree_node *node = NULL;
    struct node_example *ex = NULL;
    
    node = calloc(1, sizeof(*node));
    CHKPTR(node);
    node->buf = ccn_charbuf_create();
    CHKPTR(node->buf);
    ccn_charbuf_append(node->buf, &ex1, sizeof(ex1));
    res = ccn_btree_chknode(node, 0);
    CHKSYS(res);
    FAILIF(node->corrupt != 0);
    FAILIF(node->freelow != 8 + 9); // header plus goodstuff
    ex = (void *)node->buf->buf;
    ex->e[1].t.ksiz0[2] = 100; /* ding the size in entry 1 */
    res = ccn_btree_chknode(node, 0);
    FAILIF(res != -1);
    FAILIF(node->corrupt == 0);
    ccn_charbuf_destroy(&node->buf);
    free(node);
    return(0);
}

int
test_btree_key_fetch(void)
{
    int i;
    int res;
    struct ccn_charbuf *cb = NULL;
    struct ccn_btree_node *node = NULL;
    struct node_example ex = ex1;
    
    const char *expect[3] = { "d", "goodstuff", "odd" };
    
    node = calloc(1, sizeof(*node));
    CHKPTR(node);
    node->buf = ccn_charbuf_create();
    CHKPTR(node->buf);
    ccn_charbuf_append(node->buf, &ex, sizeof(ex));
    
    cb = ccn_charbuf_create();
    
    for (i = 0; i < 3; i++) {
        res = ccn_btree_key_fetch(cb, node, i);
        CHKSYS(res);
        FAILIF(cb->length != strlen(expect[i]));
        FAILIF(0 != memcmp(cb->buf, expect[i], cb->length));
    }
    
    res = ccn_btree_key_fetch(cb, node, i); /* fetch past end */
    FAILIF(res != -1);
    res = ccn_btree_key_fetch(cb, node, -1); /* fetch before start */
    FAILIF(res != -1);
    FAILIF(node->corrupt); /* Those should not have flagged corruption */
    
    ex.e[1].t.koff0[2] = 1; /* ding the offset in entry 1 */
    node->buf->length = 0;
    ccn_charbuf_append(node->buf, &ex, sizeof(ex));
    
    res = ccn_btree_key_append(cb, node, 0); /* Should still be OK */
    CHKSYS(res);
    
    res = ccn_btree_key_append(cb, node, 1); /* Should fail */
    FAILIF(res != -1);
    FAILIF(!node->corrupt);
    printf("line %d code = %d\n", __LINE__, node->corrupt);
    
    ccn_charbuf_destroy(&cb);
    ccn_charbuf_destroy(&node->buf);
    free(node);
    return(0);
}

int
test_btree_compare(void)
{
    int i, j;
    int res;
    struct ccn_btree_node *node = NULL;
    struct node_example ex = ex1;
    
    const char *expect[3] = { "d", "goodstuff", "odd" };
    
    node = calloc(1, sizeof(*node));
    CHKPTR(node);
    node->buf = ccn_charbuf_create();
    CHKPTR(node->buf);
    ccn_charbuf_append(node->buf, &ex, sizeof(ex));
    
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            res = ccn_btree_compare((const void *)expect[i], strlen(expect[i]),
                node, j);
            FAILIF( (i < j) != (res < 0));
            FAILIF( (i > j) != (res > 0));
            FAILIF( (i == j) != (res == 0));
        }
    }
    ccn_charbuf_destroy(&node->buf);
    free(node);
    return(0);
}

int
test_btree_searchnode(void)
{
    int i;
    int res;
    struct ccn_btree_node *node = NULL;
    struct node_example ex = ex1;
    const int yes = 1;
    const int no = 0;
    
    struct {
        const char *s;
        int expect;
    } testvec[] = {
        {"", CCN_BT_ENCRES(0, no)},
        {"c", CCN_BT_ENCRES(0, no)},
        {"d", CCN_BT_ENCRES(0, yes)},
        {"d1", CCN_BT_ENCRES(1, no)},
        {"goodstuff", CCN_BT_ENCRES(1, yes)},
        {"goodstuff1", CCN_BT_ENCRES(2, no)},
        {"odc++++++", CCN_BT_ENCRES(2, no)},
        {"odd", CCN_BT_ENCRES(2, yes)},
        {"odd1", CCN_BT_ENCRES(3, no)},
        {"ode", CCN_BT_ENCRES(3, no)}
    };
    
    node = calloc(1, sizeof(*node));
    CHKPTR(node);
    node->buf = ccn_charbuf_create();
    CHKPTR(node->buf);
    ccn_charbuf_append(node->buf, &ex, sizeof(ex));
    
    res = ccn_btree_node_nent(node);
    FAILIF(res != 3);
    
    for (i = 0; i < sizeof(testvec)/sizeof(testvec[0]); i++) {
        const char *s = testvec[i].s;
        res = ccn_btree_searchnode((const void *)s, strlen(s), node);
        printf("search %s => %d, expected %d\n", s, res, testvec[i].expect);
        FAILIF(res != testvec[i].expect);
    }
    ccn_charbuf_destroy(&node->buf);
    free(node);
    return(0);
}

int
test_btree_init(void)
{
    struct ccn_btree *btree = NULL;
    int res;
    struct ccn_btree_node *node = NULL;
    struct ccn_btree_node *node0 = NULL;
    struct ccn_btree_node *node1 = NULL;
    
    btree = ccn_btree_create();
    CHKPTR(btree);
    node0 = ccn_btree_getnode(btree, 0);
    CHKPTR(node0);
    node1 = ccn_btree_getnode(btree, 1);
    FAILIF(node0 == node1);
    FAILIF(hashtb_n(btree->resident) != 2);
    node = ccn_btree_rnode(btree, 0);
    FAILIF(node != node0);
    node = ccn_btree_rnode(btree, 1);
    FAILIF(node != node1);
    node = ccn_btree_rnode(btree, 2);
    FAILIF(node != NULL);
    res = ccn_btree_destroy(&btree);
    FAILIF(btree != NULL);
    return(res);
}


int
test_btree_lookup(void)
{
    const int yes = 1;
    const int no = 0;
    struct ccn_btree *btree = NULL;
    struct ccn_btree_node *root = NULL;
    struct ccn_btree_node *leaf = NULL;
    int i;
    int res;
    struct {
        const char *s;
        int expectnode;
        int expectres;
    } testvec[] = {
        {"d", 2, CCN_BT_ENCRES(0, yes)},
        {"goodstuff", 2, CCN_BT_ENCRES(1, yes)},
        {"odd", 2, CCN_BT_ENCRES(2, yes)},
        {"truth", 3, CCN_BT_ENCRES(2, yes)},
        {"tooth", 3, CCN_BT_ENCRES(2, no)},
    };

    btree = ccn_btree_create();
    CHKPTR(btree);
    leaf = ccn_btree_getnode(btree, 2);
    CHKPTR(leaf);
    ccn_charbuf_append(leaf->buf, &ex1, sizeof(ex1));
    res = ccn_btree_chknode(leaf, 0);
    CHKSYS(res);
    leaf = ccn_btree_getnode(btree, 3);
    CHKPTR(leaf);
    ccn_charbuf_append(leaf->buf, &ex2, sizeof(ex2));
    res = ccn_btree_chknode(leaf, 0);
    CHKSYS(res);
    root = ccn_btree_getnode(btree, 1);
    CHKPTR(root);
    ccn_charbuf_append(root->buf, &rootex1, sizeof(rootex1));
    res = ccn_btree_chknode(root, 0);
    CHKSYS(res);
    /* Now we should have a 3-node btree, all resident. Do our lookups. */
    for (i = 0; i < sizeof(testvec)/sizeof(testvec[0]); i++) {
        const char *s = testvec[i].s;
        res = ccn_btree_lookup(btree, (const void *)s, strlen(s), &leaf);
        printf("lookup %s => %d, %d, expected %d, %d\n", s,
            leaf->nodeid,          res,
            testvec[i].expectnode, testvec[i].expectres);
        FAILIF(res != testvec[i].expectres);
        FAILIF(leaf->nodeid != testvec[i].expectnode);
        FAILIF(leaf->parent != 1);
    }
    res = ccn_btree_destroy(&btree);
    FAILIF(btree != NULL);
    return(res);
}

int
main(int argc, char **argv)
{
    int res;

    res = test_directory_creation();
    CHKSYS(res);
    res = test_btree_io();
    CHKSYS(res);
    res = test_btree_lockfile();
    CHKSYS(res);
    res = test_structure_sizes();
    CHKSYS(res);
    res = test_btree_chknode();
    CHKSYS(res);
    res = test_btree_key_fetch();
    CHKSYS(res);
    res = test_btree_compare();
    CHKSYS(res);
    res = test_btree_searchnode();
    CHKSYS(res);
    res = test_btree_init();
    CHKSYS(res);
    res = test_btree_lookup();
    CHKSYS(res);
    return(0);
}
