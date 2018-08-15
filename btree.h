#pragma once

#include "BTreeNode.h"

class BTreeIndex : public DbIndex {
public:
    BTreeIndex(DbRelation& relation, Identifier name, ColumnNames key_columns,
               bool unique);
    virtual ~BTreeIndex();

    virtual void create();
    virtual void drop();

    virtual void open();
    virtual void close();

    virtual Handles* lookup(ValueDict* key) const;
    virtual Handles* range(ValueDict* min_key, ValueDict* max_key) const;

    virtual void insert(Handle handle);
    virtual void del(Handle handle);

    // pull out the key values from the ValueDict in order
    virtual KeyValue *tkey(const ValueDict *key) const;

protected:
    static const BlockID STAT = 1;
    bool closed;
    BTreeStat *stat;
    BTreeNode *root;
    HeapFile file;
    KeyProfile key_profile;

    void build_key_profile();
    Handles* _lookup(BTreeNode *node, uint height, const KeyValue* key) const;
    Insertion _insert(BTreeNode *node, uint height, const KeyValue* key,
                      Handle handle);
};

bool test_btree();
