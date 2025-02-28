/*
** Portions Copyright (C) 2000,2001,2002 Carnegie Mellon University
** Copyright (C) 2001 Jed Pickel <jed@pickel.net>
** Portions Copyright (C) 2001 Andrew R. Baker <andrewb@farm9.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/* $Id: spo_database.c,v 1.70 2003/05/19 18:08:05 chrisgreen Exp $ */

/* Snort Database Output Plug-in
 * 
 *  Maintainer: Roman Danyliw <rdd@cert.org>, <roman@danyliw.com>
 *
 *  Originally written by Jed Pickel <jed@pickel.net> (2000-2001)
 *
 * See the doc/README.database file with this distribution 
 * documentation or the snortdb web site for configuration
 * information
 *
 * Web Site: http://www.andrew.cmu.edu/~rdanyliw/snortdb/snortdb.html
 */

/******** Configuration *************************************************/

/* 
 * If you want extra debugging information for solving database 
 * configuration problems, uncomment the following line. 
 */
/* #define DEBUG */

/* Enable DB transactions */
#define ENABLE_DB_TRANSACTIONS

/******** Headers ******************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "event.h"
#include "decode.h"
#include "rules.h"
#include "plugbase.h"
#include "spo_plugbase.h"
#include "parser.h"
#include "debug.h"
#include "util.h"

#include "snort.h"

#ifdef ENABLE_POSTGRESQL
    #include <libpq-fe.h>
#endif
#ifdef ENABLE_MYSQL
    #if defined(_WIN32) || defined(_WIN64)
        #include <windows.h>
    #endif
    #include <mysql.h>
#endif
#ifdef ENABLE_ODBC
    #include <sql.h>
    #include <sqlext.h>
    #include <sqltypes.h>
#endif
#ifdef ENABLE_ORACLE
    #include <oci.h>
#endif
#ifdef ENABLE_MSSQL
    #define DBNTWIN32
    #include <windows.h>
    #include <sqlfront.h>
    #include <sqldb.h>
#endif

/******** Data Types  **************************************************/

/* enumerate the supported databases */
enum db_types_en
{
   DB_MYSQL      = 1,
   DB_POSTGRESQL = 2,
   DB_MSSQL      = 3,
   DB_ORACLE     = 4,
   DB_ODBC       = 5
};
typedef enum db_types_en dbtype_t;

char * db_type_name[] = { "mysql",
                          "postgresql",
                          "mssql",
                          "oracle",
                          "odbc"};

/* link-list of SQL queries */
typedef struct _SQLQuery
{
    char * val;
    struct _SQLQuery * next;
} SQLQuery;


/* the cid is unique across the dbtype, dbname, host, and sid */
/* therefore, we use these as a lookup key for the cid */
typedef struct _SharedDatabaseData
{
    dbtype_t  dbtype_id;
    char     *dbname;
    char     *host;
    int       sid;
    int       cid;
    int       reference;
} SharedDatabaseData;

typedef struct _DatabaseData
{
    SharedDatabaseData *shared;
    char  *facility;
    char  *password;
    char  *user;
    char  *port;
    char  *sensor_name;
    int    encoding;
    int    detail;
    int    ignore_bpf;
    int    tz;
    int    DBschema_version;
#ifdef ENABLE_POSTGRESQL
    PGconn * p_connection;
    PGresult * p_result;
#endif
#ifdef ENABLE_MYSQL
    MYSQL * m_sock;
    MYSQL_RES * m_result;
    MYSQL_ROW m_row;
#endif
#ifdef ENABLE_ODBC
    SQLHENV u_handle;
    SQLHDBC u_connection;
    SQLHSTMT u_statement;
    SQLINTEGER  u_col;
    SQLINTEGER  u_rows;
#endif
#ifdef ENABLE_ORACLE
    OCIEnv *o_environment;
    OCISvcCtx *o_servicecontext;
    OCIError *o_error;
    OCIStmt *o_statement;
    OCIDefine *o_define;
    text o_errormsg[512];
    sb4 o_errorcode;
#endif
#ifdef ENABLE_MSSQL
    PDBPROCESS  ms_dbproc;
    PLOGINREC   ms_login;
    DBINT       ms_col;
#endif
} DatabaseData;

/* list for lookup of shared data information */
typedef struct _SharedDatabaseDataNode
{
    SharedDatabaseData *data;
    struct _SharedDatabaseDataNode *next;
} SharedDatabaseDataNode;


/******** Constants  ***************************************************/

#define MAX_QUERY_LENGTH 8192
#define POSTGRESQL   "postgresql"
#define MYSQL        "mysql"
#define ODBC         "odbc"
#define ORACLE       "oracle"
#define MSSQL        "mssql"

/*#define MYSQL_INSERT "INSERT DELAYED " */
#define MYSQL_INSERT "INSERT "
#define STANDARD_INSERT "INSERT "

#define LATEST_DB_SCHEMA_VERSION 106

/******** Prototypes  **************************************************/

void          DatabaseInit(u_char *);
DatabaseData *ParseDatabaseArgs(char *);
void          Database(Packet *, char *, void *, Event *);
char *        snort_escape_string(char *, DatabaseData *);
void          SpoDatabaseCleanExitFunction(int, void *);
void          SpoDatabaseRestartFunction(int, void *);
void          InitDatabase();
int           UpdateLastCid(DatabaseData *, int, int);
int           GetLastCid(DatabaseData *, int);
int           CheckDBVersion(DatabaseData *);
void          BeginTransaction(DatabaseData * data);
void          CommitTransaction(DatabaseData * data);
void          RollbackTransaction(DatabaseData * data);
int           Insert(char *, DatabaseData *);
int           Select(char *, DatabaseData *);
void          Connect(DatabaseData *);
void          DatabasePrintUsage();
void          FreeSharedDataList();

/******** Global Variables  ********************************************/

extern PV pv;
extern OptTreeNode *otn_tmp;  /* rule node */

static SharedDatabaseDataNode *sharedDataList = NULL;
static int instances = 0;

/******** Database Specific Extras  ************************************/

/* The following is for supporting Microsoft SQL Server */
#ifdef ENABLE_MSSQL

/* If you want extra debugging information (specific to
   Microsoft SQL Server), uncomment the following line. */
#define ENABLE_MSSQL_DEBUG

#if defined(DEBUG) || defined(ENABLE_MSSQL_DEBUG)
    /* this is for debugging purposes only */
    static char g_CurrentStatement[2048];
    #define SAVESTATEMENT(str)   strncpy(g_CurrentStatement, str, sizeof(g_CurrentStatement) - 1);
    #define CLEARSTATEMENT()     bzero((char *) g_CurrentStatement, sizeof(g_CurrentStatement));
#else
    #define SAVESTATEMENT(str)   NULL;
    #define CLEARSTATEMENT()     NULL;
#endif /* DEBUG || ENABLE_MSSQL_DEBUG*/

    /* Prototype of SQL Server callback functions. 
     * See actual declaration elsewhere for details. 
     */
    static int mssql_err_handler(PDBPROCESS dbproc, int severity, int dberr, 
                                 int oserr, LPCSTR dberrstr, LPCSTR oserrstr);
    static int mssql_msg_handler(PDBPROCESS dbproc, DBINT msgno, int msgstate, 
                                 int severity, LPCSTR msgtext, LPCSTR srvname, LPCSTR procname, 
                                 DBUSMALLINT line);
#endif /* ENABLE_MSSQL */

/*******************************************************************************
 * Function: SetupDatabase()
 *
 * Purpose: Registers the output plugin keyword and initialization 
 *          function into the output plugin list.  This is the function that
 *          gets called from InitOutputPlugins() in plugbase.c.
 *
 * Arguments: None.
 *
 * Returns: void function
 *
 ******************************************************************************/
void DatabaseSetup()
{
    /* link the preprocessor keyword to the init function in 
       the preproc list */
    RegisterOutputPlugin("database", NT_OUTPUT_ALERT, DatabaseInit);

    DEBUG_WRAP(DebugMessage(DEBUG_INIT, "database(debug): database plugin is registered...\n"););
}

/*******************************************************************************
 * Function: DatabaseInit(u_char *)
 *
 * Purpose: Calls the argument parsing function, performs final setup on data
 *          structs, links the preproc function into the function list.
 *
 * Arguments: args => ptr to argument string
 *
 * Returns: void function
 *
 ******************************************************************************/
void DatabaseInit(u_char *args)
{
    DatabaseData *data;
    char * select0;
    char * select1;
    char * insert0;
    int foundEntry = 0, sensor_cid, event_cid;
    SharedDatabaseDataNode *current;

    /* parse the argument list from the rules file */
    data = ParseDatabaseArgs(args);

    /* find a unique name for sensor if one was not supplied as an option */
    if(!data->sensor_name)
    {
        data->sensor_name = GetUniqueName((char *)PRINT_INTERFACE(pv.interface));
        if ( data->sensor_name )
        {
           if( data->sensor_name[strlen(data->sensor_name)-1] == '\n' )
              data->sensor_name[strlen(data->sensor_name)-1] = '\0';

           if( !pv.quiet_flag ) printf("database:   sensor name = %s\n", data->sensor_name);
        }
    }

    data->tz = GetLocalTimezone();

    /* allocate memory for configuration queries */
    select0 = (char *)calloc(MAX_QUERY_LENGTH, sizeof(char));
    select1 = (char *)calloc(MAX_QUERY_LENGTH, sizeof(char));
    insert0 = (char *)calloc(MAX_QUERY_LENGTH, sizeof(char));

    if( data->ignore_bpf == 0 )
    {
        if(pv.pcap_cmd == NULL)
        {
            snprintf(insert0, MAX_QUERY_LENGTH, 
                     "INSERT INTO sensor (hostname, interface, detail, encoding, last_cid) "
                     "VALUES ('%s','%s','%u','%u', '0')", 
                     data->sensor_name, PRINT_INTERFACE(pv.interface), data->detail, data->encoding);
            snprintf(select0, MAX_QUERY_LENGTH, 
                     "SELECT sid FROM sensor WHERE hostname = '%s' "
                     "AND interface = '%s' AND detail = '%u' AND "
                     "encoding = '%u' AND filter IS NULL",
                     data->sensor_name, PRINT_INTERFACE(pv.interface), data->detail, data->encoding);
        }
        else
        {
            snprintf(select0, MAX_QUERY_LENGTH, 
                     "SELECT sid FROM sensor WHERE hostname = '%s' "
                     "AND interface = '%s' AND filter ='%s' AND "
                     "detail = '%u' AND encoding = '%u'",
                     data->sensor_name, PRINT_INTERFACE(pv.interface), pv.pcap_cmd,
                     data->detail, data->encoding);
            snprintf(insert0, MAX_QUERY_LENGTH, 
                     "INSERT INTO sensor (hostname, interface, filter,"
                     "detail, encoding, last_cid) "
                     "VALUES ('%s','%s','%s','%u','%u', '0')", 
                     data->sensor_name, PRINT_INTERFACE(pv.interface), pv.pcap_cmd,
                     data->detail, data->encoding);
        }
    }
    else /* data->ignore_bpf == 1 ) */
    {
        if(pv.pcap_cmd == NULL)
        {
            snprintf(insert0, MAX_QUERY_LENGTH, 
                     "INSERT INTO sensor (hostname, interface, detail, encoding) "
                     "VALUES ('%s','%s','%u','%u')", 
                     data->sensor_name, PRINT_INTERFACE(pv.interface), data->detail, data->encoding);
            snprintf(select0, MAX_QUERY_LENGTH, 
                     "SELECT sid FROM sensor WHERE hostname = '%s' "
                     "AND interface = '%s' AND detail = '%u' AND "
                     "encoding = '%u'",
                     data->sensor_name, PRINT_INTERFACE(pv.interface), data->detail, data->encoding);
        }
        else
        {
            snprintf(select0, MAX_QUERY_LENGTH, 
                     "SELECT sid FROM sensor WHERE hostname = '%s' "
                     "AND interface = '%s' AND "
                     "detail = '%u' AND encoding = '%u'",
                     data->sensor_name, PRINT_INTERFACE(pv.interface),
                     data->detail, data->encoding);
            snprintf(insert0, MAX_QUERY_LENGTH, 
                     "INSERT INTO sensor (hostname, interface, filter,"
                     "detail, encoding) "
                     "VALUES ('%s','%s','%s','%u','%u')", 
                     data->sensor_name, PRINT_INTERFACE(pv.interface), pv.pcap_cmd,
                     data->detail, data->encoding);
        }
    }

    Connect(data);

    data->shared->sid = Select(select0,data);
    if(data->shared->sid == 0)
    {
        Insert(insert0,data);
        data->shared->sid = Select(select0,data);
        if(data->shared->sid == 0)
        {
            ErrorMessage("database: Problem obtaining SENSOR ID (sid) from %s->sensor\n", 
                         data->shared->dbname);
            FatalError("\n"
                       " When this plugin starts, a SELECT query is run to find the sensor id for the\n"
                       " currently running sensor. If the sensor id is not found, the plugin will run\n"
                       " an INSERT query to insert the proper data and generate a new sensor id. Then a\n"
                       " SELECT query is run to get the newly allocated sensor id. If that fails then\n"
                       " this error message is generated.\n"
                       "\n"
                       " Some possible causes for this error are:\n"
                       "  * the user does not have proper INSERT or SELECT privileges\n"
                       "  * the sensor table does not exist\n"
                       "\n"
                       " If you are _absolutely_ certain that you have the proper privileges set and\n"
                       " that your database structure is built properly please let me know if you\n"
                       " continue to get this error. You can contact me at (roman@danyliw.com).\n"
                       "\n");
        }
    }

    if( !pv.quiet_flag )  printf("database:     sensor id = %u\n", data->shared->sid);

    /* the cid may be shared across multiple instances of the database
     * plugin, first we check the shared data list to see if we already
     * have a value to use, if so, we replace the SharedDatabaseData struct
     * in the DatabaseData struct with the one out of the sharedDataList.
     * Sound confusing enough?  
     *   -Andrew	
     */

    /* XXX: Creating a set of list handling functions would make this cleaner */
    current = sharedDataList;
    while(current != NULL)
    {
        /* We have 4 key fields to check */
        if((current->data->sid == data->shared->sid) &&
           (current->data->dbtype_id == data->shared->dbtype_id) &&
           /* XXX: should this be a case insensitive compare? */
           (strcasecmp(current->data->dbname, data->shared->dbname) == 0) &&
           (strcasecmp(current->data->host, data->shared->host) == 0))
        {
            foundEntry = 1;
            break;
        }
        current = current->next;
    }
	
    if(foundEntry == 0)
    {
        /* Add it the the shared data list */
        SharedDatabaseDataNode *newNode = (SharedDatabaseDataNode *)calloc(1,sizeof(SharedDatabaseDataNode));
        newNode->data = data->shared;
        newNode->next = NULL;
        if(sharedDataList == NULL)
        {
            sharedDataList = newNode;
        }
        else
        {
            current = sharedDataList;
            while(current->next != NULL)
                current = current->next;
            current->next = newNode;
        }

        /* Set the cid value 
         * - get the cid value in sensor.last_cid
         * - get the MAX(cid) from event 
         * - if snort crashed without storing the latest cid, then
         *     the MAX(event.cid) > sensor.last_cid.  Update last_cid in this case
         */
        sensor_cid = GetLastCid(data, data->shared->sid);

        snprintf(select1, MAX_QUERY_LENGTH,
        	 "SELECT MAX(cid) FROM event WHERE sid = '%u'", data->shared->sid);
        event_cid = Select(select1, data);

        if ( event_cid > sensor_cid )
        {
           UpdateLastCid(data, data->shared->sid, event_cid);
           ErrorMessage("database: inconsistent cid information for sid=%u\n", 
                        data->shared->sid);
           ErrorMessage("          Recovering by rolling forward the cid=%u\n", 
                        event_cid);
        }

        data->shared->cid = event_cid;
        ++(data->shared->cid);
    }
    else
    {
        /* Free memory associated with data->shared */
        free(data->shared);
        data->shared = current->data;
    }

    /* free memory */
    free(select0);
    free(select1);
    free(insert0);

    /* Get the versioning information for the DB schema */
    data->DBschema_version = CheckDBVersion(data);
    if( !pv.quiet_flag ) printf("database: schema version = %d\n", data->DBschema_version);

    if ( data->DBschema_version < LATEST_DB_SCHEMA_VERSION )
    {
       FatalError("database: The underlying database seems to be running an older version of\n"
                  "          the DB schema (current version=%d, required minimum version= %d).\n\n"
                  "          If you have an existing database with events logged by a previous\n"
                  "          version of snort, this database must first be upgraded to the latest\n"
                  "          schema (see the snort-users mailing list archive or DB plugin\n"
	          "          documention for details).\n\n"
                  "          If migrating old data is not desired, merely create a new instance\n"
                  "          of the snort database using the appropriate DB creation script\n"
                  "          (e.g. create_mysql, create_postgresql, create_oracle, create_mssql)\n"
                  "          located in the contrib\\ directory.\n\n"
		  "          See the database documentation for cursory details (doc/README.database).\n"
		  "          and the URL to the most recent database plugin documentation.\n",
		  data->DBschema_version, LATEST_DB_SCHEMA_VERSION);
    }
    /*
    else if ( data->DBschema_version < LATEST_DB_SCHEMA_VERSION )
    {                
       ErrorMessage("database: The database is using an older version of the DB schema\n");
    }
    */

    /* Add the processor function into the function list */
    if(!strncasecmp(data->facility,"log",3))
    {
        pv.log_plugin_active = 1;
        if( !pv.quiet_flag ) printf("database: using the \"log\" facility\n");
        AddFuncToOutputList(Database, NT_OUTPUT_LOG, data);
    }
    else
    {
        pv.alert_plugin_active = 1;
        if( !pv.quiet_flag ) printf("database: using the \"alert\" facility\n");
        AddFuncToOutputList(Database, NT_OUTPUT_ALERT, data);
    }

    AddFuncToCleanExitList(SpoDatabaseCleanExitFunction, data);
    AddFuncToRestartList(SpoDatabaseRestartFunction, data); 
    ++instances;
}

/*******************************************************************************
 * Function: ParseDatabaseArgs(char *)
 *
 * Purpose: Process the preprocessor arguements from the rules file and 
 *          initialize the preprocessor's data struct.
 *
 * Arguments: args => argument list
 *
 * Returns: void function
 *
 ******************************************************************************/
DatabaseData *ParseDatabaseArgs(char *args)
{
    DatabaseData *data;
    char *dbarg;
    char *a1;
    char *type;
    char *facility;

    data = (DatabaseData *)calloc(1, sizeof(DatabaseData));
    data->shared = (SharedDatabaseData *)calloc(1, sizeof(SharedDatabaseData));

    if(args == NULL)
    {
        ErrorMessage("database: you must supply arguments for database plugin\n");
        DatabasePrintUsage();
        FatalError("");
    }

    data->shared->dbtype_id = 0;
    data->sensor_name = NULL;
    data->facility = NULL;
    data->encoding = ENCODING_HEX;
    data->detail = DETAIL_FULL;
    data->ignore_bpf = 0;

    facility = strtok(args, ", ");
    if(facility != NULL)
    {
        if((!strncasecmp(facility,"log",3)) || (!strncasecmp(facility,"alert",5)))
            data->facility = facility;
        else
        {
            ErrorMessage("database: The first argument needs to be the logging facility\n");
            DatabasePrintUsage();
            FatalError("");
        }
    }
    else
    {
        ErrorMessage("database: Invalid format for first argment\n"); 
        DatabasePrintUsage();
        FatalError("");
    }

    type = strtok(NULL, ", ");

    if(type == NULL)
    {
        ErrorMessage("database: you must enter the database type in configuration file as the second argument\n");
        DatabasePrintUsage();
        FatalError("");
    }

    /* print out and test the capability of this plugin */
    if( !pv.quiet_flag ) printf("database: compiled support for ( ");


#ifdef ENABLE_MYSQL
    if( !pv.quiet_flag ) printf("%s ",MYSQL);
    if(!strncasecmp(type,MYSQL,5))
        data->shared->dbtype_id = DB_MYSQL; 
#endif
#ifdef ENABLE_POSTGRESQL
    if( !pv.quiet_flag ) printf("%s ",POSTGRESQL);
    if(!strncasecmp(type,POSTGRESQL,10))
        data->shared->dbtype_id = DB_POSTGRESQL; 
#endif
#ifdef ENABLE_ODBC
    if( !pv.quiet_flag ) printf("%s ",ODBC);
    if(!strncasecmp(type,ODBC,4))
        data->shared->dbtype_id = DB_ODBC; 
#endif
#ifdef ENABLE_ORACLE
    if( !pv.quiet_flag ) printf("%s ",ORACLE);
    if(!strncasecmp(type,ORACLE,6))
      data->shared->dbtype_id = DB_ORACLE; 
#endif
#ifdef ENABLE_MSSQL
    if( !pv.quiet_flag ) printf("%s ",MSSQL);
    if(!strncasecmp(type,MSSQL,5))
      data->shared->dbtype_id = DB_MSSQL; 
#endif

    if( !pv.quiet_flag ) printf(")\n");

    if( !pv.quiet_flag ) printf("database: configured to use %s\n", type);

    if(data->shared->dbtype_id == 0)
    {
        if ( !strncasecmp(type, MYSQL, 5) ||
             !strncasecmp(type, POSTGRESQL, 10) ||
             !strncasecmp(type, ODBC, 4) ||
             !strncasecmp(type, MSSQL, 5)  ||
             !strncasecmp(type, ORACLE, 6) )
        {
            ErrorMessage("database: '%s' support is not compiled into this build of snort\n\n", type);
            FatalError("If this build of snort was obtained as a binary distribution (e.g., rpm,\n"
                       "or Windows), then check for alternate builds that contains the necessary\n"
                       "'%s' support.\n\n"
                       "If this build of snort was compiled by you, then re-run the\n"
                       "the ./configure script using the '--with-%s' switch.\n"
                       "For non-standard installations of a database, the '--with-%s=DIR'\n"
                       "syntax may need to be used to specify the base directory of the DB install.\n\n"
		       "See the database documentation for cursory details (doc/README.database).\n"
		       "and the URL to the most recent database plugin documentation.\n",
                       type, type, type);
        }
        else
        {
           FatalError("database: '%s' is an unknown database type.  The supported\n"
                      "          databases include: MySQL (mysql), PostgreSQL (postgresql),\n"
                      "          ODBC (odbc), Oracle (oracle), and Microsoft SQL Server (mssql)\n",
                      type);
        }
    }

    dbarg = strtok(NULL, " =");
    while(dbarg != NULL)
    {
        a1 = NULL;
        a1 = strtok(NULL, ", ");
        if(!strncasecmp(dbarg,"host",4))
        {
            data->shared->host = a1;
            if( !pv.quiet_flag ) printf("database:          host = %s\n", data->shared->host);
        }
        if(!strncasecmp(dbarg,"port",4))
        {
            data->port = a1;
            if( !pv.quiet_flag ) printf("database:          port = %s\n", data->port);
        }
        if(!strncasecmp(dbarg,"user",4))
        {
            data->user = a1;
            if( !pv.quiet_flag ) printf("database:          user = %s\n", data->user);
        }
        if(!strncasecmp(dbarg,"password",8))
        {
            if( !pv.quiet_flag ) printf("database: password is set\n");
            data->password = a1;
        }
        if(!strncasecmp(dbarg,"dbname",6))
        {
            data->shared->dbname = a1;
            if( !pv.quiet_flag ) printf("database: database name = %s\n", data->shared->dbname);
        }
        if(!strncasecmp(dbarg,"sensor_name",11))
        {
            data->sensor_name = a1;
            if( !pv.quiet_flag ) printf("database:   sensor name = %s\n", data->sensor_name);
        }
        if(!strncasecmp(dbarg,"encoding",6))
        {
            if(!strncasecmp(a1, "hex", 3))
                data->encoding = ENCODING_HEX;
            else
            {
                if(!strncasecmp(a1, "base64", 6))
                    data->encoding = ENCODING_BASE64;
                else
                {
                    if(!strncasecmp(a1, "ascii", 5))
                        data->encoding = ENCODING_ASCII;
                    else
                        FatalError("database: unknown  (%s)", a1);
                }
            }
            if( !pv.quiet_flag ) printf("database: data encoding = %s\n", a1);
        }
        if(!strncasecmp(dbarg,"detail",6))
        {
            if(!strncasecmp(a1, "full", 4))
                data->detail = DETAIL_FULL;
            else
            {
                if(!strncasecmp(a1, "fast", 4))
                    data->detail = DETAIL_FAST;
                else
                    FatalError("database: unknown detail level (%s)", a1);
            } 
            if( !pv.quiet_flag ) printf("database: detail level  = %s\n", a1);
        }
        if(!strncasecmp(dbarg,"ignore_bpf",10))
        {
            if(!strncasecmp(a1, "no", 2) || !strncasecmp(a1, "0", 1))
                data->ignore_bpf = 0;
            else if(!strncasecmp(a1, "yes", 3) || !strncasecmp(a1, "1", 1))
                data->ignore_bpf = 1;
            else
                FatalError("database: unknown ignore_bpf argument (%s)", a1);

            if( !pv.quiet_flag ) printf("database: ignore_bpf = %s\n", a1);
        }
        dbarg = strtok(NULL, "=");
    } 

    if(data->shared->dbname == NULL)
    {
        ErrorMessage("database: must enter database name in configuration file\n\n");
        DatabasePrintUsage();
        FatalError("");
    }

    return data;
}

void FreeQueryNode(SQLQuery * node)
{
    if(node)
    {
        FreeQueryNode(node->next);
        free(node->val);
        free(node);
    }
}

SQLQuery * NewQueryNode(SQLQuery * parent, int query_size)
{
    SQLQuery * rval;

    if(query_size == 0)
        query_size = MAX_QUERY_LENGTH;

    if(parent)
    {
        while(parent->next)
            parent = parent->next;

        parent->next = (SQLQuery *)malloc(sizeof(SQLQuery));
        rval = parent->next;
    }
    else
        rval = (SQLQuery *)malloc(sizeof(SQLQuery));

    rval->val = (char *)malloc(query_size);
    rval->next = NULL;

    return rval;
}  

/*******************************************************************************
 * Function: Database(Packet *, char * msg, void *arg)
 *
 * Purpose: Insert data into the database
 *
 * Arguments: p   => pointer to the current packet data struct 
 *            msg => pointer to the signature message
 *
 * Returns: void function
 *
 ******************************************************************************/
void Database(Packet *p, char *msg, void *arg, Event *event)
{
    DatabaseData *data = (DatabaseData *)arg;
    SQLQuery * query;
    SQLQuery * root;
    char *tmp, *tmp1, *tmp2, 
         *sig_name, *sig_class, *ref_system_name, *ref_tag;
    char * tmp_not_escaped;
    int i, tmp1_len, tmp2_len, ok_transaction;
    char *select0, *select1, *insert0 = NULL;
    unsigned int sig_id;
    int ref_system_id;
    unsigned int ref_id, class_id=0;
    ClassType *class_ptr;
    ReferenceNode *refNode;

    query = NewQueryNode(NULL, 0);
    root = query;

#ifdef ENABLE_DB_TRANSACTIONS
    BeginTransaction(data);
#endif
    
    if(msg == NULL)
        msg = "";

    /*** Build the query for the Event Table ***/
    if(p != NULL)
        tmp = GetTimestamp((time_t *)&p->pkth->ts.tv_sec, data->tz);
    else
        tmp = GetCurrentTimestamp();

#ifdef ENABLE_MSSQL
    if(data->shared->dbtype_id == DB_MSSQL)
    {
        /* SQL Server uses a date format which is slightly
         * different from the ISO-8601 standard generated
         * by GetTimestamp() and GetCurrentTimestamp().  We
         * need to convert from the ISO-8601 format of:
         *   "1998-01-25 23:59:59+14316557"
         * to the SQL Server format of:
         *   "1998-01-25 23:59:59.143"
         */
        if( tmp!=NULL && strlen(tmp)>=22 )
        {
            tmp[19] = '.';
            tmp[23] = '\0';
        }
    }
#endif
#ifdef ENABLE_ORACLE
    if (data->shared->dbtype_id == DB_ORACLE)
    {
        /* Oracle (everything before 9i) does not support
         * date information smaller than 1 second.
         * To go along with the TO_DATE() Oracle function
         * below, this was written to strip out all the
         * excess information. (everything beyond a second)
         */
        if ( tmp!=NULL && strlen(tmp)>=22 )
        {
            tmp[19] = '\0';
        }
    }
#endif

    /* Write the signature information 
     *  - Determine the ID # of the signature of this alert 
     */
    select0 = (char *) malloc (MAX_QUERY_LENGTH+1);
    sig_name = snort_escape_string(msg, data);
    if ( event->sig_rev == 0 ) 
    {
        if( event->sig_id == 0) 
        {
            snprintf(select0, MAX_QUERY_LENGTH, 
                    "SELECT sig_id FROM signature WHERE sig_name = '%s' AND"
                    " sig_rev is NULL AND sig_sid is NULL ", sig_name);
        }
        else 
        {
            snprintf(select0, MAX_QUERY_LENGTH, 
                    "SELECT sig_id FROM signature WHERE sig_name = '%s' AND"
                    " sig_rev is NULL AND sig_sid = %u ", 
                    sig_name, event->sig_id);
        }
    }
    else
    {
        if( event->sig_id == 0)
        {
            snprintf(select0, MAX_QUERY_LENGTH,
                    "SELECT sig_id FROM signature WHERE sig_name = '%s' AND "
                    " sig_rev = %u AND sig_sid is NULL ",
                    sig_name, event->sig_rev);
        }
        else
        {
            snprintf(select0, MAX_QUERY_LENGTH,
                    "SELECT sig_id FROM signature WHERE sig_name = '%s' AND "
                    " sig_rev = %u AND sig_sid = %u ",
                    sig_name, event->sig_rev, event->sig_id);
        }
    }
    
    sig_id = Select(select0, data);

    /* If this signature is detected for the first time
     *  - write the signature
     *  - write the signature's references, classification, priority, id,
     *                          revision number
     * Note: if a signature (identified with a unique text message, revision #) 
     *       initially is logged to the DB without references/classification, 
     *       but later they are added, this information will _not_ be 
     *       stored/updated unless the revision number is changed.
     *       This algorithm is used in order to prevent many DB SELECTs to
     *       verify their presence _every_ time the alert is triggered. 
     */
    if(sig_id == 0)
    {
        /* get classification and priority information  */
        if(otn_tmp)
        {
            class_ptr = otn_tmp->sigInfo.classType;

            if(class_ptr)
            {
                /* classification */
                if(class_ptr->type)
                {
                    /* Get the ID # of this classification */ 
                    select1 = (char *) malloc (MAX_QUERY_LENGTH+1);
                    sig_class = snort_escape_string(class_ptr->type, data);
		    
                    snprintf(select1, MAX_QUERY_LENGTH, 
                            "SELECT sig_class_id FROM sig_class WHERE "
                            " sig_class_name = '%s'", sig_class);
                    class_id = Select(select1, data);

                    if ( !class_id )
                    {
                        insert0 = (char *) malloc (MAX_QUERY_LENGTH+1);
                        snprintf(insert0, MAX_QUERY_LENGTH,
                                "INSERT INTO sig_class (sig_class_name) VALUES "
                                "('%s')", sig_class);
                        Insert(insert0, data);
                        free(insert0);
                        class_id = Select(select1, data);
                        if ( !class_id )
                            ErrorMessage("database: unable to write classification\n");
                    }
                    free(select1);
		    free(sig_class);
                }
            }
        }

        insert0 = (char *) malloc (MAX_QUERY_LENGTH+1);
        tmp1 = (char *) malloc (MAX_QUERY_LENGTH+1);
        tmp2 = (char *) malloc (MAX_QUERY_LENGTH+1);
        tmp1_len = 0;
	tmp2_len = 0;

        snprintf(tmp1, MAX_QUERY_LENGTH-tmp1_len, "%s", "sig_name");
	snprintf(tmp2, MAX_QUERY_LENGTH-tmp2_len, "'%s'", sig_name);
        tmp1_len = strlen(tmp1);
        tmp2_len = strlen(tmp2);

        if ( class_id > 0 )
        {
	    snprintf(&tmp1[tmp1_len], MAX_QUERY_LENGTH-tmp1_len, "%s", ",sig_class_id");
	    snprintf(&tmp2[tmp2_len], MAX_QUERY_LENGTH-tmp2_len, ",%u", class_id);
            tmp1_len = strlen(tmp1);
            tmp2_len = strlen(tmp2);
        } 

        if ( event->priority > 0 )
        {
            snprintf(&tmp1[tmp1_len], MAX_QUERY_LENGTH-tmp1_len, "%s", ",sig_priority");
	    snprintf(&tmp2[tmp2_len], MAX_QUERY_LENGTH-tmp2_len, ",%u", event->priority);
            tmp1_len = strlen(tmp1);
            tmp2_len = strlen(tmp2);
        }

        if ( event->sig_rev > 0 )
        {
            snprintf(&tmp1[tmp1_len], MAX_QUERY_LENGTH-tmp1_len, "%s", ",sig_rev");
	    snprintf(&tmp2[tmp2_len], MAX_QUERY_LENGTH-tmp2_len, ",%u", event->sig_rev);
            tmp1_len = strlen(tmp1);
            tmp2_len = strlen(tmp2);
        }

        if ( event->sig_id > 0 )
        {
            snprintf(&tmp1[tmp1_len], MAX_QUERY_LENGTH-tmp1_len, "%s", ",sig_sid");
	    snprintf(&tmp2[tmp2_len], MAX_QUERY_LENGTH-tmp2_len, ",%u", event->sig_id);
            tmp1_len = strlen(tmp1);
            tmp2_len = strlen(tmp2);            
        }

        snprintf(insert0, MAX_QUERY_LENGTH,
                "INSERT INTO signature (%s) VALUES (%s)",
                tmp1, tmp2);

        Insert(insert0,data);
        free(insert0);
        free(tmp1);
        free(tmp2);

        sig_id = Select(select0,data);
        if(sig_id == 0)
        {
            ErrorMessage("database: Problem inserting a new signature '%s'\n", msg);
        }
        free(select0);

        /* add the external rule references  */
        if(otn_tmp)
        {
            refNode = otn_tmp->sigInfo.refs;
            i = 1;

            while(refNode)
            {
                /* Get the ID # of the reference from the DB */
                select0 = (char *) malloc (MAX_QUERY_LENGTH+1);
                insert0 = (char *) malloc (MAX_QUERY_LENGTH+1);
                ref_system_name = snort_escape_string(refNode->system->name, data);
		
                /* Note: There is an underlying assumption that the SELECT
                 *       will do a case-insensitive comparison.
                 */
                snprintf(select0, MAX_QUERY_LENGTH, 
                        "SELECT ref_system_id FROM reference_system WHERE "
                        " ref_system_name = '%s'", ref_system_name);
                snprintf(insert0, MAX_QUERY_LENGTH,
                        "INSERT INTO reference_system (ref_system_name) "
                        "VALUES ('%s')", ref_system_name);
                ref_system_id = Select(select0, data);
                if ( ref_system_id == 0 )
                {
                    Insert(insert0, data);
                    ref_system_id = Select(select0, data);
                }

                free(select0);
                free(insert0);
		free(ref_system_name);

                if ( ref_system_id > 0 )
                {
                    select0 = (char *) malloc (MAX_QUERY_LENGTH+1);
		    ref_tag = snort_escape_string(refNode->id, data);
                    snprintf(select0, MAX_QUERY_LENGTH,
                            "SELECT ref_id FROM reference WHERE "
                            "ref_system_id = %d AND ref_tag = '%s'",
                            ref_system_id, ref_tag);
                    ref_id = Select(select0, data);
                    free(ref_tag);
		    
                    /* If this reference is not in the database, write it */
                    if ( ref_id == 0 )
                    {
                        /* truncate the reference tag as necessary */
                        tmp1 = (char *) malloc (101);
                        if ( data->DBschema_version == 103 )
                            snprintf(tmp1, 20, "%s", refNode->id);
                        else if ( data->DBschema_version >= 104 )
                            snprintf(tmp1, 100, "%s", refNode->id);

                        insert0 = (char *) malloc (MAX_QUERY_LENGTH+1);
			ref_tag = snort_escape_string(tmp1, data);
                        snprintf(insert0, MAX_QUERY_LENGTH,
                                "INSERT INTO reference (ref_system_id, ref_tag) VALUES "
                                "(%d, '%s')", ref_system_id, ref_tag);
                        Insert(insert0, data);
                        ref_id = Select(select0, data);
                        free(insert0); 
                        free(tmp1);
			free(ref_tag);

                        if ( ref_id == 0 )
                        {
                            ErrorMessage("database: Unable to insert the alert reference into the DB\n");
                        }
                    }
                    free(select0);

                    insert0 = (char *) malloc (MAX_QUERY_LENGTH+1);
                    snprintf(insert0, MAX_QUERY_LENGTH,
                            "INSERT INTO sig_reference (sig_id, ref_seq, ref_id) "
                            "VALUES (%u, %d, %u)", sig_id, i, ref_id);
                    Insert(insert0, data);
                    free(insert0);
                    ++i;
                }
                else
                {
                    ErrorMessage("database: Unable to insert unknown reference tag ('%s') used in rule.\n", refNode->system);
                }

                refNode = refNode->next;
            }
        }
    }
    else
        free(select0);

    free(sig_name);
    
    if ( (data->shared->dbtype_id == DB_ORACLE) &&
            (data->DBschema_version >= 105) )
    {
        snprintf(query->val, MAX_QUERY_LENGTH,
                "INSERT INTO event (sid,cid,signature,timestamp) VALUES "
                "('%u', '%u', '%u', TO_DATE('%s', 'YYYY-MM-DD HH24:MI:SS'))",
                data->shared->sid, data->shared->cid, sig_id, tmp);
    }
    else
    {
        snprintf(query->val, MAX_QUERY_LENGTH,
                "INSERT INTO event (sid,cid,signature,timestamp) VALUES "
                "('%u', '%u', '%u', '%s')",
                data->shared->sid, data->shared->cid, sig_id, tmp);
    }

    free(tmp); 

    /* We do not log fragments! They are assumed to be handled 
       by the fragment reassembly pre-processor */

    if(p != NULL)
    {
        if((!p->frag_flag) && (p->iph)) 
        {
            /* query = NewQueryNode(query, 0); */
            if(p->iph->ip_proto == IPPROTO_ICMP && p->icmph)
            {
                query = NewQueryNode(query, 0);
                /*** Build a query for the ICMP Header ***/
                if(data->detail)
                {
                    if(p->ext)
                    {
                        snprintf(query->val, MAX_QUERY_LENGTH, 
                                "INSERT INTO icmphdr (sid, cid, icmp_type, icmp_code, "
                                "icmp_csum, icmp_id, icmp_seq) "
                                "VALUES ('%u','%u','%u','%u','%u','%u','%u')",
                                data->shared->sid, data->shared->cid, p->icmph->type, p->icmph->code,
                                ntohs(p->icmph->csum), ntohs(p->ext->id), ntohs(p->ext->seqno));
                    }
                    else
                    {
                        snprintf(query->val, MAX_QUERY_LENGTH, 
                                "INSERT INTO icmphdr (sid, cid, icmp_type, icmp_code, "
                                "icmp_csum) "
                                "VALUES ('%u','%u','%u','%u','%u')",
                                data->shared->sid, data->shared->cid, p->icmph->type, p->icmph->code,
                                ntohs(p->icmph->csum));
                    }
                }
                else
                {
                    snprintf(query->val, MAX_QUERY_LENGTH, 
                            "INSERT INTO icmphdr (sid, cid, icmp_type, icmp_code) "
                            "VALUES ('%u','%u','%u','%u')",
                            data->shared->sid, data->shared->cid, p->icmph->type, p->icmph->code);
                }
            }
            else if(p->iph->ip_proto == IPPROTO_TCP && p->tcph)
            {
                query = NewQueryNode(query, 0);
                /*** Build a query for the TCP Header ***/
                if(data->detail)
                {
                    snprintf(query->val, MAX_QUERY_LENGTH, 
                            "INSERT INTO tcphdr "
                            "(sid, cid, tcp_sport, tcp_dport, tcp_seq,"
                            " tcp_ack, tcp_off, tcp_res, tcp_flags, tcp_win,"
                            " tcp_csum, tcp_urp) "
                            "VALUES ('%u','%u','%u','%u','%lu','%lu','%u',"
                            "'%u','%u','%u','%u','%u')",
                            data->shared->sid, data->shared->cid, ntohs(p->tcph->th_sport), 
                            ntohs(p->tcph->th_dport), (u_long)ntohl(p->tcph->th_seq),
                            (u_long)ntohl(p->tcph->th_ack), TCP_OFFSET(p->tcph), 
                            TCP_X2(p->tcph), p->tcph->th_flags, 
                            ntohs(p->tcph->th_win), ntohs(p->tcph->th_sum),
                            ntohs(p->tcph->th_urp));
                }
                else
                {
                    snprintf(query->val, MAX_QUERY_LENGTH, 
                            "INSERT INTO tcphdr "
                            "(sid,cid,tcp_sport,tcp_dport,tcp_flags) "
                            "VALUES ('%u','%u','%u','%u','%u')",
                            data->shared->sid, data->shared->cid, ntohs(p->tcph->th_sport), 
                            ntohs(p->tcph->th_dport), p->tcph->th_flags);
                }


                if(data->detail)
                {
                    /*** Build the query for TCP Options ***/
                    for(i=0; i < (int)(p->tcp_option_count); i++)
                    {
                        query = NewQueryNode(query, 0);
                        if((data->encoding == ENCODING_HEX) || (data->encoding == ENCODING_ASCII))
                        {
                            tmp = fasthex(p->tcp_options[i].data, p->tcp_options[i].len); 
                        }
                        else
                        {
                            tmp = base64(p->tcp_options[i].data, p->tcp_options[i].len);
                        }
                        snprintf(query->val, MAX_QUERY_LENGTH, 
                                "INSERT INTO opt "
                                "(sid,cid,optid,opt_proto,opt_code,opt_len,opt_data) "
                                "VALUES ('%u','%u','%u','%u','%u','%u','%s')",
                                data->shared->sid, data->shared->cid, i, 6, p->tcp_options[i].code,
                                p->tcp_options[i].len, tmp); 
                        free(tmp);
                    }
                }
            }
            else if(p->iph->ip_proto == IPPROTO_UDP && p->udph)
            {
                query = NewQueryNode(query, 0);
                /*** Build the query for the UDP Header ***/
                if(data->detail)
                {
                    snprintf(query->val, MAX_QUERY_LENGTH,
                            "INSERT INTO udphdr "
                            "(sid, cid, udp_sport, udp_dport, udp_len, udp_csum) "
                            "VALUES ('%u', '%u', '%u', '%u', '%u', '%u')",
                            data->shared->sid, data->shared->cid, ntohs(p->udph->uh_sport), 
                            ntohs(p->udph->uh_dport), ntohs(p->udph->uh_len),
                            ntohs(p->udph->uh_chk));
                }
                else
                {
                    snprintf(query->val, MAX_QUERY_LENGTH,
                            "INSERT INTO udphdr "
                            "(sid, cid, udp_sport, udp_dport) "
                            "VALUES ('%u', '%u', '%u', '%u')",
                            data->shared->sid, data->shared->cid, ntohs(p->udph->uh_sport), 
                            ntohs(p->udph->uh_dport));
                }
            }
        }   

        /*** Build the query for the IP Header ***/
        if ( p->iph )
        {
            query = NewQueryNode(query, 0);

            if(data->detail)
            {
                snprintf(query->val, MAX_QUERY_LENGTH, 
                        "INSERT INTO iphdr "
                        "(sid, cid, ip_src, ip_dst, ip_ver,"
                        "ip_hlen, ip_tos, ip_len, ip_id, ip_flags, ip_off,"
                        "ip_ttl, ip_proto, ip_csum) "
                        "VALUES ('%u','%u','%lu',"
                        "'%lu','%u',"
                        "'%u','%u','%u','%u','%u','%u',"
                        "'%u','%u','%u')",
                        data->shared->sid, data->shared->cid, (u_long)ntohl(p->iph->ip_src.s_addr), 
                        (u_long)ntohl(p->iph->ip_dst.s_addr), 
                        IP_VER(p->iph), IP_HLEN(p->iph), 
                        p->iph->ip_tos, ntohs(p->iph->ip_len), ntohs(p->iph->ip_id), 
                        p->frag_flag, ntohs(p->frag_offset), p->iph->ip_ttl, 
                        p->iph->ip_proto, ntohs(p->iph->ip_csum));
            }
            else
            {
                snprintf(query->val, MAX_QUERY_LENGTH, 

                        "INSERT INTO iphdr "
                        "(sid, cid, ip_src, ip_dst, ip_proto) "
                        "VALUES ('%u','%u','%lu','%lu','%u')",
                        data->shared->sid, data->shared->cid, (u_long)ntohl(p->iph->ip_src.s_addr),
                        (u_long)ntohl(p->iph->ip_dst.s_addr), p->iph->ip_proto);
            }

            /*** Build querys for the IP Options ***/
            if(data->detail)
            {
                for(i=0 ; i < (int)(p->ip_option_count); i++)
                {
                    if(&p->ip_options[i])
                    {
                        query = NewQueryNode(query, 0);
                        if((data->encoding == ENCODING_HEX) || (data->encoding == ENCODING_ASCII))
                            tmp = fasthex(p->ip_options[i].data, p->ip_options[i].len); 
                        else
                            tmp = base64(p->ip_options[i].data, p->ip_options[i].len); 

                        snprintf(query->val, MAX_QUERY_LENGTH, 
                                "INSERT INTO opt "
                                "(sid,cid,optid,opt_proto,opt_code,opt_len,opt_data) "
                                "VALUES ('%u','%u','%u','%u','%u','%u','%s')",
                                data->shared->sid, data->shared->cid, i, 0, p->ip_options[i].code,
                                p->ip_options[i].len, tmp); 
                        free(tmp);
                    }
                }
            }
        }

        /*** Build query for the payload ***/
        if ( p->data )
        {
            if(data->detail)
            {
                if(p->dsize)
                {
                    query = NewQueryNode(query, p->dsize * 2 + MAX_QUERY_LENGTH);
		    memset(query->val, 0, p->dsize*2 + MAX_QUERY_LENGTH);
                    if(data->encoding == ENCODING_BASE64)
                        tmp_not_escaped = base64(p->data, p->dsize);
                    else
                    {
                        if(data->encoding == ENCODING_ASCII)
                            tmp_not_escaped = ascii(p->data, p->dsize);
                        else
                            tmp_not_escaped = fasthex(p->data, p->dsize);
                    }

                    tmp = snort_escape_string(tmp_not_escaped, data);

                    snprintf(query->val, (p->dsize * 2) + MAX_QUERY_LENGTH - 3, 
                            "INSERT INTO data "
                            "(sid,cid,data_payload) "
                            "VALUES ('%u','%u','%s",
                            data->shared->sid, data->shared->cid, tmp);
                    strcat(query->val, "')");
                    free (tmp);
                    free (tmp_not_escaped);
                }
            }
        }
    }

    /* Execute the queries */
    query = root;
    ok_transaction = 1;
    while(query)
    {
        if ( Insert(query->val,data) == 0 )
        {
#ifdef ENABLE_DB_TRANSACTIONS
           RollbackTransaction(data);
#endif
	   ok_transaction = 0;
           break;
        }
        else
           query = query->next;
    }
    FreeQueryNode(root); 

    /* Increment the cid*/
    data->shared->cid++;

#ifdef ENABLE_DB_TRANSACTIONS
    if ( ok_transaction )
       CommitTransaction(data);
#endif
    
    /* A Unixodbc bugfix */
#ifdef ENABLE_ODBC
    if(data->shared->cid == 600)
    {
        data->shared->cid = 601;
    }
#endif
}

/* Some of the code in this function is from the 
   mysql_real_escape_string() function distributed with mysql.

   Those portions of this function remain
   Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   We needed a more general case that was not MySQL specific so there
   were small modifications made to the mysql_real_escape_string() 
   function. */

char * snort_escape_string(char * from, DatabaseData * data)
{
    char * to;
    char * to_start;
    char * end; 
    int from_length;

    from_length = (int)strlen(from);

    to = (char *)malloc(strlen(from) * 2 + 1);
    to_start = to;
#ifdef ENABLE_ORACLE
    if (data->shared->dbtype_id == DB_ORACLE)
    {
      for (end=from+from_length; from != end; from++)
      {
        switch(*from)
        {
          case '\n':                               /* Must be escaped for logs */
            *to++= '\\';
            *to++= 'n';
            break;
          case '\r':
            *to++= '\\';
            *to++= 'r';
            break;
          case '\'':
            *to++= '\'';
            *to++= '\'';
            break;
          case '\032':                     /* This gives problems on Win32 */
            *to++= '\\';
            *to++= 'Z';
            break;
          default:
            *to++= *from;
        }
      }
    }
    else
#endif
    {
      for(end=from+from_length; from != end; from++)
      {
        switch(*from)
        {
          case 0:             /* Must be escaped for 'mysql' */
            *to++= '\\';
            *to++= '0';
            break;
          case '\n':              /* Must be escaped for logs */
            *to++= '\\';
            *to++= 'n';
            break;
          case '\r':
            *to++= '\\';
            *to++= 'r';
            break;
          case '\\':
            *to++= '\\';
            *to++= '\\';
            break;
          case '\'':
            *to++= '\\';
            *to++= '\'';
            break;
          case '"':               /* Better safe than sorry */
            *to++= '\\';
            *to++= '"';
            break;
          case '\032':            /* This gives problems on Win32 */
            *to++= '\\';
            *to++= 'Z';
            break;
          default:
            *to++= *from; 
        }
      }
    }
    *to=0;
    return(char *)to_start;
}

/*******************************************************************************
 * Function: UpdateLastCid(DatabaseData * data, int sid, int cid)
 *
 * Purpose: Sets the last cid used for a given a sensor ID (sid), 
 *
 * Arguments: data  : database information
 *            sid   : sensor ID
 *            cid   : event ID 
 *
 * Returns: status of the update
 *
 ******************************************************************************/
int UpdateLastCid(DatabaseData *data, int sid, int cid)
{
    char *insert0;
    int ret;

    insert0 = (char *) malloc (MAX_QUERY_LENGTH+1);
    snprintf(insert0, MAX_QUERY_LENGTH,
             "UPDATE sensor SET last_cid = %u WHERE sid = %u",
             cid, sid);

    ret = Insert(insert0, data);
    free(insert0);
    return ret;
}

/*******************************************************************************
 * Function: GetLastCid(DatabaseData * data, int sid)
 *
 * Purpose: Returns the last cid used for a given a sensor ID (sid), 
 *
 * Arguments: data  : database information
 *            sid   : sensor ID
 *
 * Returns: last cid for a given sensor ID (sid)
 *
 ******************************************************************************/
int GetLastCid(DatabaseData *data, int sid)
{
    char *select0;
    int tmp_cid;

    select0 = (char *) malloc (MAX_QUERY_LENGTH+1);
    snprintf(select0, MAX_QUERY_LENGTH,
             "SELECT last_cid FROM sensor WHERE sid = '%u'", sid);

    tmp_cid = Select(select0,data);
    free(select0);
   
    return tmp_cid;
}

/*******************************************************************************
 * Function: CheckDBVersion(DatabaseData * data)
 *
 * Purpose: To determine the version number of the underlying DB schema
 *
 * Arguments: database information
 *
 * Returns: version number of the schema
 *
 ******************************************************************************/
int CheckDBVersion(DatabaseData * data)
{
   char *select0;
   int schema_version;

   select0 = (char *) malloc (MAX_QUERY_LENGTH+1);

#ifdef ENABLE_MSSQL
   if ( data->shared->dbtype_id == DB_MSSQL )
      /* "schema" is a keyword in SQL Server, so use square brackets
       *  to indicate that we are referring to the table
       */
      snprintf(select0, MAX_QUERY_LENGTH, "SELECT vseq FROM [schema]");
   else
#endif
      snprintf(select0, MAX_QUERY_LENGTH, "SELECT vseq FROM schema");

   schema_version = Select(select0,data);
   free(select0);

   return schema_version;
}

/*******************************************************************************
 * Function: BeginTransaction(DatabaseData * data)
 *
 * Purpose: Database independent SQL to start a transaction
 * 
 ******************************************************************************/
void BeginTransaction(DatabaseData * data)
{
#ifdef ENABLE_MSSQL
    if( data->shared->dbtype_id == DB_MSSQL )
       Insert("BEGIN TRAN", data);
    else
#endif
#ifndef ENABLE_ORACLE
       /* No need for begin in Oracle...            */
       Insert("BEGIN", data);
#endif
}

/*******************************************************************************
 * Function: BeginTransaction(DatabaseData * data)
 *
 * Purpose: Database independent SQL to commit a transaction
 * 
 ******************************************************************************/
void CommitTransaction(DatabaseData * data)
{
#ifdef ENABLE_MSSQL
    if( data->shared->dbtype_id == DB_MSSQL )
       Insert("COMMIT TRAN", data);
    else
#endif
       Insert("COMMIT", data);
}

/*******************************************************************************
 * Function: RollbackTransaction(DatabaseData * data)
 *
 * Purpose: Database independent SQL to rollback a transaction
 * 
 ******************************************************************************/
void RollbackTransaction(DatabaseData * data)
{
#ifdef ENABLE_MSSQL
    if( data->shared->dbtype_id == DB_MSSQL )
       Insert("ROLLBACK TRAN", data);
    else
#endif
       Insert("ROLLBACK", data);
}

/*******************************************************************************
 * Function: Insert(char * query, DatabaseData * data)
 *
 * Purpose: Database independent function for SQL inserts
 * 
 * Arguments: query (An SQL insert)
 *
 * Returns: 1 if successful, 0 if fail
 *
 ******************************************************************************/
int Insert(char * query, DatabaseData * data)
{
    int result = 0;
#ifdef ENABLE_MYSQL
    char * modified_query;
    char * query_ptr;
#endif

#ifdef ENABLE_POSTGRESQL
    if( data->shared->dbtype_id == DB_POSTGRESQL )
    {
        data->p_result = PQexec(data->p_connection,query);
        if(!(PQresultStatus(data->p_result) != PGRES_COMMAND_OK))
        {
            result = 1;
        }
        else
        {
            if(PQerrorMessage(data->p_connection)[0] != '\0')
            {
                ErrorMessage("database: postgresql_error: %s\n", PQerrorMessage(data->p_connection));
            }
        } 
        PQclear(data->p_result);
    }
#endif

#ifdef ENABLE_MYSQL
    if(data->shared->dbtype_id == DB_MYSQL)
    {
        
        if(!strncasecmp(query, STANDARD_INSERT, strlen(STANDARD_INSERT)))
        {
           modified_query = (char *)malloc(strlen(query) + 10);
           strncpy(modified_query, MYSQL_INSERT, strlen(MYSQL_INSERT)+1);
           query_ptr = query + strlen(STANDARD_INSERT);
           strncat(modified_query, query_ptr, strlen(query_ptr)+1);
           strncpy(query,modified_query,strlen(modified_query)+1);
           free(modified_query);
        } 
        
        if(!(mysql_query(data->m_sock,query)))
        {
            result = 1;
        }
        else
        {
            if(mysql_errno(data->m_sock))
            {
              ErrorMessage("database: mysql_error: %s\nSQL=%s\n", 
                           mysql_error(data->m_sock), query);
            }
        }
    }
#endif

#ifdef ENABLE_ODBC
    if(data->shared->dbtype_id == DB_ODBC)
    {
        if(SQLAllocStmt(data->u_connection, &data->u_statement) == SQL_SUCCESS)
            if(SQLPrepare(data->u_statement, query, SQL_NTS) == SQL_SUCCESS)
                if(SQLExecute(data->u_statement) == SQL_SUCCESS)
                    result = 1;
    }
#endif

#ifdef ENABLE_ORACLE
    if(data->shared->dbtype_id == DB_ORACLE)
    {
        if (OCIStmtPrepare(data->o_statement, data->o_error, query, strlen(query), OCI_NTV_SYNTAX, OCI_DEFAULT) || 
	    OCIStmtExecute(data->o_servicecontext, data->o_statement, data->o_error, 1,  0, NULL, NULL, OCI_COMMIT_ON_SUCCESS))
        {
	    OCIErrorGet(data->o_error, 1, NULL, &data->o_errorcode, data->o_errormsg, sizeof(data->o_errormsg), OCI_HTYPE_ERROR);
	    ErrorMessage("database: oracle_error: %s\n", data->o_errormsg);
        } 
	else 
        {
  	    result = 1;
	}
    }
#endif

#ifdef ENABLE_MSSQL
    if(data->shared->dbtype_id == DB_MSSQL)
    {
        SAVESTATEMENT(query);
        dbfreebuf(data->ms_dbproc);
        if( dbcmd(data->ms_dbproc, query) == SUCCEED )
            if( dbsqlexec(data->ms_dbproc) == SUCCEED )
                if( dbresults(data->ms_dbproc) == SUCCEED )
                {
                    while (dbnextrow(data->ms_dbproc) != NO_MORE_ROWS)
                    {
                        result = (int)data->ms_col;
                    }
                    result = 1;
                }
        CLEARSTATEMENT();
    }
#endif

#ifdef DEBUG
    if(result)
    {
        DEBUG_WRAP(DebugMessage(DEBUG_LOG,"database(debug): (%s) executed\n", query););
    }
    else
    {
        DEBUG_WRAP(DebugMessage(DEBUG_LOG,"database(debug): (%s) failed\n", query););
    }
#endif

    return result;
}

/*******************************************************************************
 * Function: Select(char * query, DatabaeData * data)
 *
 * Purpose: Database independent function for SQL selects that 
 *          return a non zero int
 * 
 * Arguments: query (An SQL insert)
 *
 * Returns: result of query if successful, 0 if fail
 *
 ******************************************************************************/
int Select(char * query, DatabaseData * data)
{
    int result = 0;

#ifdef ENABLE_POSTGRESQL
    if( data->shared->dbtype_id == DB_POSTGRESQL )
    {
        data->p_result = PQexec(data->p_connection,query);
        if((PQresultStatus(data->p_result) == PGRES_TUPLES_OK))
        {
            if(PQntuples(data->p_result))
            {
                if((PQntuples(data->p_result)) > 1)
                {
                    ErrorMessage("database: warning (%s) returned more than one result\n", query);
                    result = 0;
                }
                else
                {
                    result = atoi(PQgetvalue(data->p_result,0,0));
                } 
            }
        }
        if(!result)
        {
            if(PQerrorMessage(data->p_connection)[0] != '\0')
            {
                ErrorMessage("database: postgresql_error: %s\n",PQerrorMessage(data->p_connection));
            }
        }
        PQclear(data->p_result);
    }
#endif

#ifdef ENABLE_MYSQL
    if(data->shared->dbtype_id == DB_MYSQL)
    {
        if(mysql_query(data->m_sock,query))
        {
            result = 0;
        }
        else
        {
            if(!(data->m_result = mysql_use_result(data->m_sock)))
            {
                result = 0;
            }
            else
            {
                if((data->m_row = mysql_fetch_row(data->m_result)))
                {
                    if(data->m_row[0] != NULL)
                    {
                        result = atoi(data->m_row[0]);
                    }
                }
            }
            mysql_free_result(data->m_result);
        }
        if(!result)
        {
            if(mysql_errno(data->m_sock))
            {
                ErrorMessage("database: mysql_error: %s\n", mysql_error(data->m_sock));
            }
        }
    }
#endif

#ifdef ENABLE_ODBC
    if(data->shared->dbtype_id == DB_ODBC)
    {
        if(SQLAllocStmt(data->u_connection, &data->u_statement) == SQL_SUCCESS)
            if(SQLPrepare(data->u_statement, query, SQL_NTS) == SQL_SUCCESS)
                if(SQLExecute(data->u_statement) == SQL_SUCCESS)
                    if(SQLRowCount(data->u_statement, &data->u_rows) == SQL_SUCCESS)
                        if(data->u_rows)
                        {
                            if(data->u_rows > 1)
                            {
                                ErrorMessage("database: warning (%s) returned more than one result\n", query);
                                result = 0;
                            }
                            else
                            {
                                if(SQLFetch(data->u_statement) == SQL_SUCCESS)
                                    if(SQLGetData(data->u_statement,1,SQL_INTEGER,&data->u_col,
                                                  sizeof(data->u_col), NULL) == SQL_SUCCESS)
                                        result = (int)data->u_col;
                            }
                        }
    }
#endif

#ifdef ENABLE_ORACLE
    if(data->shared->dbtype_id == DB_ORACLE)
    {
        if (OCIStmtPrepare(data->o_statement, data->o_error, query, strlen(query), OCI_NTV_SYNTAX, OCI_DEFAULT) ||
	    OCIStmtExecute(data->o_servicecontext, data->o_statement, data->o_error, 0, 0, NULL, NULL, OCI_DEFAULT) ||
	    OCIDefineByPos (data->o_statement, &data->o_define, data->o_error, 1, &result, sizeof(result), SQLT_INT, 0, 0, 0, OCI_DEFAULT) ||
	    OCIStmtFetch (data->o_statement, data->o_error, 1, OCI_FETCH_NEXT, OCI_DEFAULT))
	{
	    OCIErrorGet(data->o_error, 1, NULL, &data->o_errorcode, data->o_errormsg, sizeof(data->o_errormsg), OCI_HTYPE_ERROR);
	    ErrorMessage("database: oracle_error: %s\n", data->o_errormsg);
	}
    }
#endif

#ifdef ENABLE_MSSQL
    if(data->shared->dbtype_id == DB_MSSQL)
    {
        SAVESTATEMENT(query);
        dbfreebuf(data->ms_dbproc);
        if( dbcmd(data->ms_dbproc, query) == SUCCEED )
            if( dbsqlexec(data->ms_dbproc) == SUCCEED )
                if( dbresults(data->ms_dbproc) == SUCCEED )
                    if( dbbind(data->ms_dbproc, 1, INTBIND, (DBINT) 0, (BYTE *) &data->ms_col) == SUCCEED )
                        while (dbnextrow(data->ms_dbproc) != NO_MORE_ROWS)
                        {
                            result = (int)data->ms_col;
                        }
        CLEARSTATEMENT();
    }
#endif

#ifdef DEBUG
    if(result)
    {
        DEBUG_WRAP(DebugMessage(DEBUG_LOG,"database(debug): (%s) returned %u\n", query, result););
    }
    else
    {
        DEBUG_WRAP(DebugMessage(DEBUG_LOG,"database(debug): (%s) failed\n", query););
    }
#endif

    return result;
}


/*******************************************************************************
 * Function: Connect(DatabaseData * data)
 *
 * Purpose: Database independent function to initiate a database 
 *          connection
 *
 ******************************************************************************/
void Connect(DatabaseData * data)
{
#ifdef ENABLE_MYSQL
    int x; 
#endif

#ifdef ENABLE_POSTGRESQL
    if( data->shared->dbtype_id == DB_POSTGRESQL )
    {
        data->p_connection = PQsetdbLogin(data->shared->host,data->port,NULL,NULL,data->shared->dbname,data->user,data->password);
        if(PQstatus(data->p_connection) == CONNECTION_BAD)
        {
            PQfinish(data->p_connection);
            FatalError("database: Connection to database '%s' failed\n", data->shared->dbname);
        }
    }
#endif

#ifdef ENABLE_MYSQL
    if(data->shared->dbtype_id == DB_MYSQL)
    {
        data->m_sock = mysql_init(NULL);
        if(data->m_sock == NULL)
        {
            FatalError("database: Connection to database '%s' failed\n", data->shared->dbname);
        }
        if(data->port != NULL)
        {
            x = atoi(data->port);
        }
        else
        {
            x = 0;
        }
        if(mysql_real_connect(data->m_sock, data->shared->host, data->user, data->password, data->shared->dbname, x, NULL, 0) == 0)
        {
            if(mysql_errno(data->m_sock))
            {
                FatalError("database: mysql_error: %s\n", mysql_error(data->m_sock));
            }
            FatalError("database: Failed to logon to database '%s'\n", data->shared->dbname);
        }
    }
#endif

#ifdef ENABLE_ODBC
    if(data->shared->dbtype_id == DB_ODBC)
    {
        if(!(SQLAllocEnv(&data->u_handle) == SQL_SUCCESS))
        {
            FatalError("database: unable to allocate ODBC environment\n");
        }
        if(!(SQLAllocConnect(data->u_handle, &data->u_connection) ==
             SQL_SUCCESS))
        {
            FatalError("database: unable to allocate ODBC connection handle\n");
        }
        if(!(SQLConnect(data->u_connection, data->shared->dbname, SQL_NTS, data->user, SQL_NTS, data->password, SQL_NTS) == SQL_SUCCESS))
        {
            FatalError("database: ODBC unable to connect\n");
        }
    }
#endif

#ifdef ENABLE_ORACLE

    #define PRINT_ORACLE_ERR(func_name) \
     { \
         OCIErrorGet(data->o_error, 1, NULL, &data->o_errorcode, \
                     data->o_errormsg, sizeof(data->o_errormsg), OCI_HTYPE_ERROR); \
         ErrorMessage("database: Oracle_error: %s\n", data->o_errormsg); \
         FatalError("database: %s : Connection to database '%s' failed\n", \
                    func_name, data->shared->dbname); \
     }

    if(data->shared->dbtype_id == DB_ORACLE)
    {
      if (!getenv("ORACLE_HOME")) 
      {
 	 ErrorMessage("database : ORACLE_HOME environment variable not set\n");
      }
 
      if (!data->user || !data->password || !data->shared->dbname) 
      { 
         ErrorMessage("database: user, password and dbname required for Oracle\n");
         ErrorMessage("database: dbname must also be in tnsnames.ora\n");
      }

      if (data->shared->host) 
      {
         ErrorMessage("database: hostname not required for Oracle, use dbname\n");
         ErrorMessage("database: dbname must be in tnsnames.ora\n");
      }

      if (OCIInitialize(OCI_DEFAULT, NULL, NULL, NULL, NULL)) 
         PRINT_ORACLE_ERR("OCIInitialize");
 
      if (OCIEnvInit(&data->o_environment, OCI_DEFAULT, 0, NULL)) 
         PRINT_ORACLE_ERR("OCIEnvInit");
 
      if (OCIEnvInit(&data->o_environment, OCI_DEFAULT, 0, NULL)) 
         PRINT_ORACLE_ERR("OCIEnvInit (2)");
 
      if (OCIHandleAlloc(data->o_environment, (dvoid **)&data->o_error, OCI_HTYPE_ERROR, (size_t) 0, NULL))
         PRINT_ORACLE_ERR("OCIHandleAlloc");

      if (OCILogon(data->o_environment, data->o_error, &data->o_servicecontext,
                   data->user, strlen(data->user), data->password, strlen(data->password), 
                   data->shared->dbname, strlen(data->shared->dbname))) 
      {   
         OCIErrorGet(data->o_error, 1, NULL, &data->o_errorcode, data->o_errormsg, sizeof(data->o_errormsg), OCI_HTYPE_ERROR);
         ErrorMessage("database: oracle_error: %s\n", data->o_errormsg);
         ErrorMessage("database: Checklist: check database is listed in tnsnames.ora\n");
         ErrorMessage("database:            check tnsnames.ora readable\n");
         ErrorMessage("database:            check database accessible with sqlplus\n");
         FatalError("database: OCILogon : Connection to database '%s' failed\n", data->shared->dbname);
      }
 
      if (OCIHandleAlloc(data->o_environment, (dvoid **)&data->o_statement, OCI_HTYPE_STMT, 0, NULL))
         PRINT_ORACLE_ERR("OCIHandleAlloc (2)");
    }
#endif

#ifdef ENABLE_MSSQL
    if(data->shared->dbtype_id == DB_MSSQL)
    {
        CLEARSTATEMENT();
        dberrhandle(mssql_err_handler);
        dbmsghandle(mssql_msg_handler);

        if( dbinit() != NULL )
        {
            data->ms_login = dblogin();
            if( data->ms_login == NULL )
            {
                FatalError("database: Failed to allocate login structure\n");
            }
            /* Set up some informational values which are stored with the connection */
            DBSETLUSER (data->ms_login, data->user);
            DBSETLPWD  (data->ms_login, data->password);
            DBSETLAPP  (data->ms_login, "snort");
  
            data->ms_dbproc = dbopen(data->ms_login, data->shared->host);
            if( data->ms_dbproc == NULL )
            {
                FatalError("database: Failed to logon to host '%s'\n", data->shared->host);
            }
            else
            {
                if( dbuse( data->ms_dbproc, data->shared->dbname ) != SUCCEED )
                {
                    FatalError("database: Unable to change context to database '%s'\n", data->shared->dbname);
                }
            }
        }
        else
        {
            FatalError("database: Connection to database '%s' failed\n", data->shared->dbname);
        }
        CLEARSTATEMENT();
    }
#endif
}

/*******************************************************************************
 * Function: Disconnect(DatabaseData * data)
 *
 * Purpose: Database independent function to close a connection
 *
 ******************************************************************************/
void Disconnect(DatabaseData * data)
{
    if( !pv.quiet_flag ) 
      printf("database: Closing connection to database \"%s\"\n", 
             data->shared->dbname);

    if(data)
    {
#ifdef ENABLE_POSTGRESQL
        if(data->shared->dbtype_id == DB_POSTGRESQL)
        {
            if(data->p_connection) PQfinish(data->p_connection);
        }
#endif

#ifdef ENABLE_MYSQL
        if(data->shared->dbtype_id == DB_MYSQL)
        {
            if(data->m_sock) mysql_close(data->m_sock);
        }
#endif

#ifdef ENABLE_ODBC
        if(data->shared->dbtype_id == DB_ODBC)
        {
            if(data->u_handle)
            {
                SQLDisconnect(data->u_connection); 
                SQLFreeHandle(SQL_HANDLE_ENV, data->u_handle); 
            }
        }
#endif

#ifdef ENABLE_MSSQL
        if(data->shared->dbtype_id == DB_MSSQL)
        {
            CLEARSTATEMENT();
            if( data->ms_dbproc != NULL )
            {
                dbfreelogin(data->ms_login);
                data->ms_login = NULL;
                dbclose(data->ms_dbproc);
                data->ms_dbproc = NULL;
            }
        }
#endif
    }
}

void DatabasePrintUsage()
{
    puts("\nUSAGE: database plugin\n");

    puts(" output database: [log | alert], [type of database], [parameter list]\n");
    puts(" [log | alert] selects whether the plugin will use the alert or");
    puts(" log facility.\n");

    puts(" For the first argument, you must supply the type of database.");
    puts(" The possible values are mysql, postgresql, unixodbc, oracle and");
    puts(" mssql ");

    puts(" The parameter list consists of key value pairs. The proper");
    puts(" format is a list of key=value pairs each separated a space.\n");

    puts(" The only parameter that is absolutely necessary is \"dbname\"."); 
    puts(" All other parameters are optional but may be necessary");
    puts(" depending on how you have configured your RDBMS.\n");

    puts(" dbname - the name of the database you are connecting to\n"); 

    puts(" host - the host the RDBMS is on\n");

    puts(" port - the port number the RDBMS is listening on\n"); 

    puts(" user - connect to the database as this user\n");

    puts(" password - the password for given user\n");

    puts(" sensor_name - specify your own name for this snort sensor. If you");
    puts("        do not specify a name one will be generated automatically\n");

    puts(" encoding - specify a data encoding type (hex, base64, or ascii)\n");

    puts(" detail - specify a detail level (full or fast)\n");

    puts(" ignore_bpf - specify if you want to ignore the BPF part for a sensor\n");
    puts("              definition (yes or no, no is default)\n");

    puts(" FOR EXAMPLE:");
    puts(" The configuration I am currently using is MySQL with the database");
    puts(" name of \"snort\". The user \"snortusr@localhost\" has INSERT and SELECT");
    puts(" privileges on the \"snort\" database and does not require a password.");
    puts(" The following line enables snort to log to this database.\n");

    puts(" output database: log, mysql, dbname=snort user=snortusr host=localhost\n");
}

void SpoDatabaseCleanExitFunction(int signal, void *arg)
{
    DatabaseData *data = (DatabaseData *)arg;

    DEBUG_WRAP(DebugMessage(DEBUG_LOG,"database(debug): entered SpoDatabaseCleanExitFunction\n"););

    UpdateLastCid(data, data->shared->sid, data->shared->cid-1);
    Disconnect(data); 
    if(data) 
       free(data);

    if(--instances == 0)
       FreeSharedDataList();
}

void SpoDatabaseRestartFunction(int signal, void *arg)
{
    DatabaseData *data = (DatabaseData *)arg;

    DEBUG_WRAP(DebugMessage(DEBUG_LOG,"database(debug): entered SpoDatabaseRestartFunction\n"););

    UpdateLastCid(data, data->shared->sid, data->shared->cid-1);
    Disconnect(data);
    if(data) 
       free(data);

    if(--instances == 0)
       FreeSharedDataList();
}

void FreeSharedDataList()
{
   SharedDatabaseDataNode *current;

   while(sharedDataList != NULL)
   { 
       current = sharedDataList;
       free(current->data);
       sharedDataList = current->next;
       free(current);
   }
}

#ifdef ENABLE_MSSQL
/*
 * The functions mssql_err_handler() and mssql_msg_handler() are callbacks that are registered
 * when we connect to SQL Server.  They get called whenever SQL Server issues errors or messages.
 * This should only occur whenever an error has occurred, or when the connection switches to
 * a different database within the server.
 */
static int mssql_err_handler(PDBPROCESS dbproc, int severity, int dberr, int oserr, 
                             LPCSTR dberrstr, LPCSTR oserrstr)
{
    int retval;
    ErrorMessage("database: DB-Library error:\n\t%s\n", dberrstr);

    if ( severity == EXCOMM && (oserr != DBNOERR || oserrstr) )
        ErrorMessage("database: Net-Lib error %d:  %s\n", oserr, oserrstr);
    if ( oserr != DBNOERR )
        ErrorMessage("database: Operating-system error:\n\t%s\n", oserrstr);
#ifdef ENABLE_MSSQL_DEBUG
    if( strlen(g_CurrentStatement) > 0 )
        ErrorMessage("database:  The above error was caused by the following statement:\n%s\n", g_CurrentStatement);
#endif
    if ( (dbproc == NULL) || DBDEAD(dbproc) )
        retval = INT_EXIT;
    else
        retval = INT_CANCEL;
    return(retval);
}


static int mssql_msg_handler(PDBPROCESS dbproc, DBINT msgno, int msgstate, int severity, 
                             LPCSTR msgtext, LPCSTR srvname, LPCSTR procname, DBUSMALLINT line)
{
    ErrorMessage("database: SQL Server message %ld, state %d, severity %d: \n\t%s\n",
                 msgno, msgstate, severity, msgtext);
    if ( (srvname!=NULL) && strlen(srvname)!=0 )
        ErrorMessage("Server '%s', ", srvname);
    if ( (procname!=NULL) && strlen(procname)!=0 )
        ErrorMessage("Procedure '%s', ", procname);
    if (line !=0) 
        ErrorMessage("Line %d", line);
    ErrorMessage("\n");
#ifdef ENABLE_MSSQL_DEBUG
    if( strlen(g_CurrentStatement) > 0 )
        ErrorMessage("database:  The above error was caused by the following statement:\n%s\n", g_CurrentStatement);
#endif

    return(0);
}
#endif
