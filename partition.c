/*
 * Partition trigger test
 * This trigger is just a test, during inserts it will skip your child table triggers
 *
 * The trigger should be used this way:
  CREATE OR REPLACE FUNCTION quotes_insert_trigger() RETURNS trigger
      AS 'pgsql_partition3'
          LANGUAGE C;
  DROP TRIGGER IF EXISTS quotes_insert_trigger on quotes;
    CREATE TRIGGER quotes_insert_trigger
      BEFORE INSERT ON quotes
      FOR EACH ROW EXECUTE PROCEDURE partition_insert_trigger();
 
  // Master Table Example  
  CREATE TABLE public.quotes
  (
     id integer, 
     received_time timestamp without time zone, 
     price real, 
     PRIMARY KEY ( id , received_time )
  );
  // Child Table Example:
  CREATE TABLE quotes_2012_09_11 (
     PRIMARY KEY (id, received_time),
     CHECK (received_time >= '2012-09-11 00:00:00' AND received_time < '2012-09-12 00:00:00')
  ) INHERITS (quotes)
   
         
To Compile you must have postgresql-devel packages
gcc -O2 -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -fpic -DREFINT_VERBOSE -I. -I. -I"/usr/pgsql-9.2/include/server/" -D_GNU_SOURCE   -c -o partition.o partition.c
 
gcc -O2 -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -fpic -Wl,--as-needed -Wl,-rpath,'/usr/local/pgsql/lib',--enable-new-dtags -L/usr/pgsql-9.2/lib -lpgport  -shared -o /usr/pgsql-9.2/lib/partition.so
 
You can customize with your own PARTITION_COLUMN or TABLE if you want
 
 */
 
#include "postgres.h"
#include "executor/spi.h"       /* this is what you need to work with SPI */
#include "commands/trigger.h"   /* ... and triggers */
 
#include "utils/rel.h"          /* ... and relations */
 
#include "catalog/namespace.h"
#include "executor/executor.h"
#include "executor/tuptable.h"
 
#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif
 
#define PARTITION_COLUMN "received_time"
#define TABLE "bond_quotes_"
#define SEPARATOR '_'
 
//#define DEBUG
 
extern Datum partition_insert_trigger(PG_FUNCTION_ARGS);
 
PG_FUNCTION_INFO_V1(partition_insert_trigger);
 
Datum partition_insert_trigger(PG_FUNCTION_ARGS)
{
    TriggerData *trigdata = (TriggerData *) fcinfo->context;
    char       *date_time;
    char        child_table[sizeof(TABLE)+sizeof("2012_09_10")] = TABLE;
    int         partition_field;
    Relation    child_table_id;
    Oid         child_table_oid;
    BulkInsertState bistate = GetBulkInsertState();
    TupleTableSlot  *slot;
    EState          *estate = CreateExecutorState();
    ResultRelInfo   *resultRelInfo = makeNode(ResultRelInfo);
    List       *recheckIndexes = NIL;
 
    /* make sure it's called as a trigger at all */
    if (!CALLED_AS_TRIGGER(fcinfo))
        elog(ERROR, "partition_insert_trigger: not called by trigger manager");
    /* Sanity checks */
    if (!TRIGGER_FIRED_BY_INSERT(trigdata->tg_event) || !TRIGGER_FIRED_BEFORE(trigdata->tg_event))
        elog(ERROR, "partition_insert_trigger: not called on insert before");
    #ifdef DEBUG
    elog(INFO, "Trigger Called for: %s", SPI_getrelname(trigdata->tg_relation));
    #endif
    // Get the field number for the partition
    partition_field = SPI_fnumber(trigdata->tg_relation->rd_att, PARTITION_COLUMN);
    // Get the value for the partition_field
    date_time = SPI_getvalue(trigdata->tg_trigtuple, trigdata->tg_relation->rd_att, partition_field);
    //make sure date was specified
    if (!date_time)
        elog(ERROR, "You cannot insert data without specifying a value for the column %s", PARTITION_COLUMN);
    #ifdef DEBUG
    elog(INFO, "Trying to insert date_time=%s", date_time);
    #endif
    //add date_time,  child_table_2012_01_23
    strncpy(child_table + sizeof(TABLE) -1  , date_time, 4);         //2012
    child_table[sizeof(TABLE) + 3] = SEPARATOR;                      //2012_
    strncpy(child_table + sizeof(TABLE) + 4  , date_time + 5, 2);    //2012_01
    child_table[sizeof(TABLE) + 6] =  SEPARATOR;                     //2012_01_
    strncpy(child_table + sizeof(TABLE) + 7  , date_time + 8, 2);    //2012_01_23
    #ifdef DEBUG
    elog(INFO, "New table will be %s", child_table);    
    #endif
    pfree(date_time);
 
    //if you care about triggers on the child tables, call ExecBRInsertTriggers
    //don't care, continue
     
    //get the OID of the table we are looking for to insert
    child_table_oid = RelnameGetRelid(child_table); //Look for child child_table
    if (child_table_oid == InvalidOid){
        elog(INFO, "partition_insert_trigger: Invalid child table %s, inserting data to main table %s", child_table, TABLE);
        return PointerGetDatum(trigdata->tg_trigtuple);
    }
    //get the descriptor of the table we are looking for
    child_table_id = RelationIdGetRelation(child_table_oid); //Get the child relation descriptor
    if (child_table_id == NULL){
        elog(ERROR, "partition_insert_trigger: Failed to locate relation for child table %s, inserting data to main table %s", child_table, TABLE);
        return PointerGetDatum(trigdata->tg_trigtuple);
    }
    //set the resultRelInfo
    resultRelInfo->ri_RangeTableIndex = 1;      /* dummy */
    resultRelInfo->ri_RelationDesc = child_table_id;
    //setup the estate, not sure why
    estate->es_result_relations = resultRelInfo;
    estate->es_num_result_relations = 1;
    estate->es_result_relation_info = resultRelInfo;
 
    /* Set up a tuple slot not sure why yet */
    slot = MakeSingleTupleTableSlot(trigdata->tg_relation->rd_att);
    ExecStoreTuple(trigdata->tg_trigtuple, slot, InvalidBuffer, false);
 
    //heap_insert(child_table_id, trigdata->tg_trigtuple, GetCurrentCommandId(true), use_wal, bistate); 
    simple_heap_insert(child_table_id, trigdata->tg_trigtuple); 
    if (resultRelInfo->ri_NumIndices > 0)
        recheckIndexes = ExecInsertIndexTuples(slot, &(trigdata->tg_trigtuple->t_self), estate);
    // not sure if this would work CatalogUpdateIndexes(child_table_id, trigdata->tg_trigtuple)
 
    //free the used memory
    list_free(recheckIndexes);
    ExecDropSingleTupleTableSlot(slot); //saw this somewhere :P
    RelationClose(child_table_id); //Must be called to free the relation memory page
    FreeBulkInsertState(bistate);  // ? not sure if needed ?
    //If return next line will add to the regular table
    //return PointerGetDatum(trigdata->tg_trigtuple); 
     
    //Return Null data, still have to figure out to return a proper X rows affected
    return PointerGetDatum(NULL);
}
