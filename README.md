pgsql_partition
===============

Sample postgresql partition C trigger.

 * Partition trigger test
 * This trigger is just a test, during inserts it will skip your child table triggers

For more info check: http://www.charlesrg.com/linux/71-postgresql-partitioning-the-database-the-fastest-way

The trigger should be used this way:

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

Edit the source to customize the table name and partitioning scheme.
