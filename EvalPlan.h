/**
 * @file EvalPlan.h - evaluation plan
 * @Professor: Kevin Lundeen
 * @Students: Wonseok Seo, Amanda Iverson
 * @see "Seattle University, CPSC5300, Summer 2018"
 * Note: no implementation changes from original version
 */

#pragma once

#include "storage_engine.h"

// type definition for evaluation pipeline
typedef std::pair<DbRelation*,Handles*> EvalPipeline;

class EvalPlan {
public:
    enum PlanType {
        ProjectAll,
        Project,
        Select,
        TableScan
    };
    // use for ProjectAll, e.g., EvalPlan(EvalPlan::ProjectAll, table);
    EvalPlan(PlanType type, EvalPlan *relation);
    // use for Project
    EvalPlan(ColumnNames *projection, EvalPlan *relation);
    // use for Select
    EvalPlan(ValueDict* conjunction, EvalPlan *relation);
    // use for TableScan
    EvalPlan(DbRelation &table);
    // use for copying
    EvalPlan(const EvalPlan *other);
    virtual ~EvalPlan();
    // Attempt to get the best equivalent evaluation plan
    EvalPlan *optimize();
    // Evaluate the plan: evaluate gets values, pipeline gets handles
    ValueDicts *evaluate();
    EvalPipeline pipeline();

protected:
    PlanType type;
    // for everything except TableScan
    EvalPlan *relation;
    // for Project
    ColumnNames *projection;
    // for Select
    ValueDict *select_conjunction;
    // for TableScan
    DbRelation &table;
};
